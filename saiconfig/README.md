# saiconfig

The `saiconfig` tool creates, sets, gets and removes SAI objects directly through syncd, similar to how `swssconfig` writes to APPL_DB one layer up. It uses sairedis's VirtualObjectIdManager for SAI object ID allocation and sends requests to syncd over ZMQ when the context config enables it, falling back to the ASIC_DB redis queue otherwise.

Objects created this way are not known to orchagent. They are not validated, not reference-counted and not cleaned up by it, and a later orchagent operation touching the same resources may fail. Use this tool for debugging and experiments, not for production configuration.

## Purpose

This tool mimics what an orchagent does when creating SAI objects, but allows for direct configuration without going through the normal orchagent flow. Some example use cases:

- Testing SAI object creation directly
- Bypassing orchagent logic for specific configurations
- Calling SAI APIs for which corresponding orch does not exist

## Usage

```bash
# Process specific JSON files
saiconfig /path/to/config.json [--no-wait] [--verbose] [-z|--mode redis_sync|zmq_sync] [-x|--context-config context_config.json]

Use optional [--no-wait] to skip waiting for SAI response resulting in failures getting ignored. Recommend setting this when modifying huge no of entries through saiconfig.

Use optional [--verbose] to set the log level for saiconfig to NOTICE.

# Show help
saiconfig -h
```

## JSON Format

The tool accepts JSON files with the following array of dict structure.

### Format 1: Create Operation (using object type name)

For creating new objects, use the object type name (without the SAI_OBJECT_TYPE_ prefix):

```json
[
    {
        "object_name": {
            "SAI_ATTRIBUTE_NAME": "SAI_ATTRIBUTE_VALUE",
            "SAI_ATTRIBUTE_NAME2": "SAI_ATTRIBUTE_VALUE2"
        },
        "OP": "create",
        "OID_NAME": "symbolic_name" // a new optional value that can be referenced by other SAI objects.
    },
    }
]
```

### Format 2: Set Operation (using OID_VALUE field)

For setting attributes on existing objects, keep the object type name as the key and
use the top‑level `OID_VALUE` field to identify which instance to modify. The `OID_VALUE` field
supports two forms:

- **Literal OID string**: `"oid:0x..."`
- **Variable reference**: `"$name"` where `name` was previously stored via `OID_NAME`

```json
[
    {
        "object_name": {
            "SAI_ATTRIBUTE_NAME": "SAI_ATTRIBUTE_VALUE",
            "SAI_ATTRIBUTE_NAME2": "SAI_ATTRIBUTE_VALUE2"
        },
        "OP": "set",
        "OID_VALUE": "oid:0x123456789abcdef"
    }
]
```

### Format 3: Get operation (Using OID_VALUE field)

The tool supports a `get` call on existing objects using the `OID_VALUE` field. This can be augmented
optionally with `object_name` and attributes that are then specifically picked out. If no attributes are
specified, then the entire object is retrieved according to the value from `OID_VALUE` field. This again
supports two forms:

- **Literal OID string**: `"oid:0x..."`
- **Variable reference**: `"$name"` where `name` was previously stored via `OID_NAME`

```json
[
    {
        "acl_counter": {
            "SAI_ACL_COUNTER_ATTR_TABLE_ID": "",
            "SAI_ACL_COUNTER_ATTR_ENABLE_PACKET_COUNT": "",
        },
        "OP": "get",
        "OID_VALUE": "$my_acl_counter"
    },
    {
        "OP": "get",
        "OID_VALUE": "oid:0x9000000000a4"
    },
    {
        "OP": "get",
        "OID_VALUE": "$my_acl_counter"
    }
]
```

### Format 4: Remove operation (Using OID_VALUE field)

Similar to the `set` operation, a `remove` operation is also supported, which
removes the objects from both the `ASIC_DB` as well as from the hardware side
through `syncd`:

When a `remove` operation is hit, the following steps happen:

1. Resolve the `OID_VALUE` field to either an `oid` or a variable reference.
2. Use this field to retrieve the oid, and subsequently the object type by using the
`VirtualObjectIdManager`'s `saiObjectTypeQuery` API call.
3. Push to `ASIC_STATE_KEY_VALUE_OP_QUEUE`:
   - Key: `SAI_OBJECT_TYPE_ACL_TABLE:oid:0x7000000000a94`
   - Value: `["SAI_ACL_TABLE_ATTR_ACL_ACTION_TYPE_LIST","1:SAI_ACL_ACTION_TYPE_PACKET_ACTION","SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST","1:SAI_ACL_BIND_POINT_TYPE_SWITCH","SAI_ACL_TABLE_ATTR_ACL_STAGE","SAI_ACL_STAGE_INGRESS","SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL","true"]`
   - Operation: `Sremove`
4. Publish "G" to `ASIC_STATE_CHANNEL@1` to notify syncd

```json
[
    {
        "OP": "remove",
        "OID_VALUE": "oid:0x9000000000a43"
    }
]
```

### Variable Expansion

The tool supports variable expansion to reference previously created objects. This is essential for creating linked objects (e.g., VLAN members that reference VLANs).

**Features:**
- **Store Object IDs**: Use the optional `OID_NAME` field to assign a symbolic name to a created object
- **Reference Objects**: Use `$variable_name` syntax in attribute values to reference stored object IDs
- **Automatic Resolution**: Variables are resolved to actual OIDs before writing to ASIC_DB

**Example with Variable Expansion: Create an ACL table with rules and bind the rules to a port**
```json
[
    {
        "acl_table": {
            "SAI_ACL_TABLE_ATTR_ACL_STAGE": "SAI_ACL_STAGE_INGRESS",
            "SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST": "1:SAI_ACL_BIND_POINT_TYPE_PORT",
            "SAI_ACL_TABLE_ATTR_FIELD_SRC_IP": "true",
            "SAI_ACL_TABLE_ATTR_FIELD_DST_IP": "true",
            "SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL": "true",
            "SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT": "true",
            "SAI_ACL_TABLE_ATTR_ACL_ACTION_TYPE_LIST": "2:SAI_ACL_ACTION_TYPE_PACKET_ACTION,SAI_ACL_ACTION_TYPE_COUNTER"
        },
        "OP": "create",
        "OID_NAME": "my_acl_table"
    },
    {
        "acl_counter": {
            "SAI_ACL_COUNTER_ATTR_TABLE_ID": "$my_acl_table",
            "SAI_ACL_COUNTER_ATTR_ENABLE_PACKET_COUNT": "true",
            "SAI_ACL_COUNTER_ATTR_ENABLE_BYTE_COUNT": "true"
        },
        "OP": "create",
        "OID_NAME": "my_acl_counter"
    },
    {
        "acl_entry": {
            "SAI_ACL_ENTRY_ATTR_TABLE_ID": "$my_acl_table",
            "SAI_ACL_ENTRY_ATTR_PRIORITY": "100",
            "SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP": "192.168.1.0&mask:255.255.255.0",
            "SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT": "80&mask:0xFFFF",
            "SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION": "SAI_PACKET_ACTION_DROP",
            "SAI_ACL_ENTRY_ATTR_ACTION_COUNTER": "$my_acl_counter"
        },
        "OP": "create",
        "OID_NAME": "my_acl_rule"
    }
]
```
### Example: ACL Table Object (Create)
In this example:
1. An ACL table is created and given the name "my_acl_table"
2. An ACL counter is created and linked to the ACL table using the variable `$my_acl_table`
3. An ACL entry is created, linked to the ACL table using the variable `$my_acl_table`, and linked to the ACL counter using the variable `$my_acl_counter`

NOTE: `$name` variables live for one saiconfig process and are shared across all files given on that command line. Linked objects must appear in dependency order; a forward reference is an error and nothing from that item is sent. Objects created outside saiconfig (by orchagent, or by an earlier saiconfig run) are referenced by their literal `oid:` value instead.

### Example: ACL Table Object (without NAME and linkage)

```json
[
    {
        "acl_table": {
            "SAI_ACL_TABLE_ATTR_ACL_STAGE": "SAI_ACL_STAGE_INGRESS",
            "SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST": "1:SAI_ACL_BIND_POINT_TYPE_SWITCH",
            "SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL": "true",
            "SAI_ACL_TABLE_ATTR_ACL_ACTION_TYPE_LIST": "1:SAI_ACL_ACTION_TYPE_PACKET_ACTION"
        },
        "OP": "create"
    }
]
```

### Example: Port Object (Set)

```json
[
    {
        "port": {
            "SAI_PORT_ATTR_ADMIN_STATE": "true",
            "SAI_PORT_ATTR_SPEED": "100000"
        },
        "OP": "set",
        "OID_VALUE": "oid:0x1000000000001"
    }
]
```

### Composite Key Entries

Some SAI objects are identified by a composite key (a JSON struct with multiple fields) rather than a simple OID. The tool supports three composite key entry types:

| Entry Type | Key Fields | Description |
|------------|------------|-------------|
| `route_entry` | `dest`, `vr` | IP route entries |
| `fdb_entry` | `mac`, `bvid` | MAC forwarding database entries |
| `neighbor_entry` | `rif`, `ip` | ARP/neighbor entries |

For composite key entries:
- The `switch_id` is automatically injected
- Key fields support variable references (`$variable_name`)
- SAI attributes are specified alongside key fields in the same object
- For `set`, `get`, and `remove` operations, only the key fields are required (no `OID_VALUE` needed)

### Example: Route Entry (Create)

```json
[
  {
    "route_entry": {
      "dest": "1.1.1.0/24",
      "vr": "oid:0x3000000000043",
      "SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION": "SAI_PACKET_ACTION_FORWARD",
      "SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID": "oid:0x4000000000a94"
    },
    "OP": "create"
  }
]
```

### Example: FDB Entry (Create and Remove)

```json
[
  {
    "fdb_entry": {
      "mac": "00:E2:E2:E2:E2:E2",
      "bvid": "oid:0x26000000000050",
      "SAI_FDB_ENTRY_ATTR_TYPE": "SAI_FDB_ENTRY_TYPE_STATIC",
      "SAI_FDB_ENTRY_ATTR_PACKET_ACTION": "SAI_PACKET_ACTION_DROP"
    },
    "OP": "create"
  },
  {
    "fdb_entry": {
      "mac": "00:E2:E2:E2:E2:E2",
      "bvid": "oid:0x26000000000050"
    },
    "OP": "remove"
  }
]
```

### Example: Neighbor Entry (Create and Remove)

```json
[
  {
    "neighbor_entry": {
      "rif": "oid:0x6000000000a36",
      "ip": "192.168.99.1",
      "SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS": "00:AA:BB:CC:DD:EE"
    },
    "OP": "create"
  },
  {
    "neighbor_entry": {
      "rif": "oid:0x6000000000a36",
      "ip": "192.168.99.1"
    },
    "OP": "remove"
  }
]
```

### Example: Route Entry with Variable References

```json
[
  {
    "virtual_router": {
      "SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE": "true",
      "SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V6_STATE": "true"
    },
    "OP": "create",
    "OID_NAME": "vr1"
  },
  {
    "next_hop": {
      "SAI_NEXT_HOP_ATTR_TYPE": "SAI_NEXT_HOP_TYPE_IP",
      "SAI_NEXT_HOP_ATTR_IP": "192.168.1.1",
      "SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID": "oid:0x60000000000001"
    },
    "OP": "create",
    "OID_NAME": "nexthop1"
  },
  {
    "route_entry": {
      "dest": "10.0.0.0/24",
      "vr": "$vr1",
      "SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION": "SAI_PACKET_ACTION_FORWARD",
      "SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID": "$nexthop1"
    },
    "OP": "create"
  },
  {
    "route_entry": {
      "dest": "10.0.0.0/24",
      "vr": "$vr1"
    },
    "OP": "remove"
  }
]
```

## How it Works

1. **Parse JSON**: Reads the JSON configuration file with array-based attribute format
2. **Determine Object ID**:
   - For **create** operations:
      For non -composite keys, uses VirtualObjectIdManager to generate a new SAI-compliant object ID
      For composite keys, uses key as objectId
   - For **set** operations: Resolves the `OID_VALUE` field:
     - If it starts with `oid:`, it is treated as a literal SAI object ID
     - If it starts with `$`, it is resolved via the `OID_NAME` mapping created by earlier `create` operations
3. **Map Object Type**:
   - For both **create** and **set** operations: Maps the object key (e.g., `"port"`, `"acl_table"`) to the corresponding `SAI_OBJECT_TYPE_*`
4. **Send to syncd** (see Transport below):
   - Over ZMQ when `/usr/share/sonic/hwsku/context_config.json` has `zmq_enable` set: the request goes to syncd's ZMQ endpoint and the reply carries the SAI status
   - Otherwise pushes the object to `ASIC_STATE_KEY_VALUE_OP_QUEUE` and publishes "G" to `ASIC_STATE_CHANNEL@1`
5. **Format**: Uses the key format `SAI_OBJECT_TYPE:object_id` with JSON array attributes as value

## Transport

saiconfig must use the same channel syncd listens on: a syncd started with `-z zmq_sync` never services the redis queue, and a redis-mode syncd has no ZMQ socket. The choice is made the way the syncd start script makes it, so the two agree on an untouched box:

1. `-z` / `--mode` `redis_sync` or `zmq_sync` on the command line, if given;
2. otherwise `zmq_enable` in the context config (`-x` / `--context-config <file>`, default `/usr/share/sonic/hwsku/context_config.json`), if the file sets it either way;
3. otherwise CONFIG_DB, as `syncd_init_common.sh` reads it: `DEVICE_METADATA|localhost switch_type` = `dpu` or `SYSTEM_DEFAULTS|swss_zmq` status = `enabled` means ZMQ, anything else means redis.

Over ZMQ saiconfig opens a request-only `ZeroMQChannel` to the context config's `zmq_endpoint` (default `tcp://127.0.0.1:5555`): only the ZMQ_REQ socket, no notification socket or thread. The notification socket binds its endpoint, so a second client cannot share the one orchagent already holds; the request-only channel lets saiconfig run alongside a live orchagent. Each request waits for syncd's reply and reports the SAI status. With `--verbose` the decision and its inputs are logged.

## ASIC_DB Interaction

On the redis path the tool interacts with ASIC_DB (database ID 1) using the same pattern that orchagents use:

- **Queue**: `ASIC_STATE_KEY_VALUE_OP_QUEUE`
- **Channel**: `ASIC_STATE_CHANNEL@1`
- **Operation**: "Screate", "Sset", "Sget" or "Sremove"
- **Notification**: "G" to trigger syncd processing
- **Key Format**: `ASIC_STATE:SAI_OBJECT_TYPE:object_id`
- **Value Format**: JSON string with SAI attributes

### Example ASIC_DB Entry

When creating a ACL_TABLE object, the tool will:

1. Push to `ASIC_STATE_KEY_VALUE_OP_QUEUE`:
   - Key: `SAI_OBJECT_TYPE_ACL_TABLE:oid:0x7000000000a94`
   - Value: `["SAI_ACL_TABLE_ATTR_ACL_ACTION_TYPE_LIST","1:SAI_ACL_ACTION_TYPE_PACKET_ACTION","SAI_ACL_TABLE_ATTR_ACL_BIND_POINT_TYPE_LIST","1:SAI_ACL_BIND_POINT_TYPE_SWITCH","SAI_ACL_TABLE_ATTR_ACL_STAGE","SAI_ACL_STAGE_INGRESS","SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL","true"]`
   - Operation: `Screate`

2. Publish "G" to `ASIC_STATE_CHANNEL@1` to notify syncd

## Why not saiplayer

`saiplayer` replays a `sairedis.rec` recording: a full session that begins with `create_switch` and references objects by the concrete VIDs allocated during that recording. It reproduces what orchagent already did. saiconfig authors a new object graph against a live switch:

- it references objects it did not create, by real ASIC_DB OID or by a `$name` bound with `OID_NAME`;
- it coexists with a running orchagent through the request-only ZMQ channel, whereas a recording needs a fresh or virtual syncd;
- its input is hand-written JSON with symbolic links, where `.rec` is a serialised API trace with no variable concept.

Composite (entry) keys such as route, neighbor and FDB are handled by both tools.

## Building

The tool is built as part of the sonic-sairedis package and requires:
- libsairedis
- libsaimetadata
- libsaimeta
- libswsscommon
- nlohmann/json
- Standard SONiC build environment

## Input validation

Before anything is sent to syncd, every request is checked against SAI metadata and ASIC_DB:

- every attribute name must be a known SAI attribute of the object type being addressed;
- `set`, `get` and `remove` must target an object that exists in ASIC_DB;
- every OID in an attribute value, single (`SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID`) or list (`SAI_PORT_ATTR_EGRESS_MIRROR_SESSION`), must reference an object that exists in ASIC_DB; `oid:0x0` is allowed;
- every OID in a composite key field (`vr`, `rif`, `bvid`) must reference an object that exists in ASIC_DB;
- syncd must have finished starting: its switch must be present in the `VIDTORID` map. For about a minute after a syncd restart requests are refused.

A request that fails any check is rejected with an error and nothing is sent. syncd treats an unknown attribute id or an untranslatable VID as a fatal runtime error: it enters shutdown-wait mode and asks orchagent to exit, which restarts the whole swss/syncd stack. These checks keep a typo in a JSON file from taking down the data plane.

## Notes

- The tool uses SONiC's VirtualObjectIdManager for proper SAI-compliant object ID generation
- Object IDs are generated using the same mechanism as other SONiC components (e.g., oid:0x49000000000001)
- Supported operations: create, set, get and remove
- The tool also supports creation of linked SAI objects through variable expansion.
- Error handling includes logging to syslog via SWSS_LOG macros
