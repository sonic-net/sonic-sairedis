# Copilot Instructions for sonic-sairedis

## Project Overview

sonic-sairedis provides the SAI (Switch Abstraction Interface) object interface to the Redis database in SONiC. It contains `syncd` — the daemon that translates SAI API calls from orchagent (via ASIC_DB) into vendor-specific ASIC SDK calls. It also includes `vslib` (virtual switch library) for testing without real hardware, and various SAI diagnostic tools.

## Architecture

```
sonic-sairedis/
├── syncd/               # syncd daemon — bridges SAI API and ASIC SDK
│   ├── Syncd.cpp        # Main syncd implementation
│   ├── SaiSwitch.cpp    # SAI switch abstraction
│   ├── VidManager.cpp   # Virtual ID management
│   └── ...
├── lib/                 # sairedis library (SAI-to-Redis translation)
│   ├── Sai.cpp          # SAI interface implementation over Redis
│   ├── RedisChannel.*   # Redis communication channel
│   └── ...
├── vslib/               # Virtual Switch SAI library (for testing)
│   ├── Sai.cpp          # VS SAI implementation
│   ├── SwitchState*.cpp # Virtual switch state management
│   └── ...
├── meta/                # SAI metadata library
│   ├── Meta.cpp         # SAI metadata validation
│   └── ...
├── proxylib/            # SAI proxy library
├── saiplayer/           # SAI recording player (replay tool)
├── saidump/             # Dump ASIC_DB contents
├── saisdkdump/          # Dump SDK state
├── saidiscovery/        # SAI object discovery
├── saiasiccmp/          # ASIC state comparison tool
├── pyext/               # Python bindings
├── unittest/            # Unit tests
├── tests/               # Integration tests
└── debian/              # Debian packaging
```

### Key Concepts
- **syncd**: The critical daemon that programs the actual ASIC hardware via vendor SAI
- **VID/RID mapping**: Virtual IDs (VID) used by orchagent map to Real IDs (RID) in the ASIC
- **SAI recording**: syncd can record all SAI calls for replay/debugging via `saiplayer`
- **Virtual Switch (VS)**: `vslib` provides a software SAI implementation for testing
- **ASIC_DB**: Redis database where orchagent writes SAI objects for syncd to process

## Language & Style

- **Primary language**: C++ (syncd, vslib, lib), Python (tests, bindings)
- **C++ standard**: C++14/17
- **Indentation**: 4 spaces
- **Naming conventions**:
  - Classes: `PascalCase` (e.g., `Syncd`, `SaiSwitch`, `VirtualSwitchSaiInterface`)
  - Methods: `camelCase`
  - Member variables: `m_` prefix (e.g., `m_vendorSai`)
  - SAI types: Follow SAI naming (`sai_object_id_t`, `sai_status_t`)
  - Constants: `UPPER_CASE`
- **File naming**: `PascalCase.cpp/.h` for classes

## Build Instructions

```bash
# Install dependencies
sudo apt-get install libswsscommon libswsscommon-dev libhiredis-dev \
  libzmq3-dev libpython-dev doxygen graphviz aspell libtool autoconf dh-exec

# Initialize SAI submodule
git submodule update --init --recursive

# Build from source
./autogen.sh
./configure
make && sudo make install

# Build Debian package
./autogen.sh
fakeroot debian/rules binary
```

## Testing

```bash
# Unit tests (in unittest/)
cd unittest
make
./unittest

# VS integration tests (in tests/)
# Requires VS environment setup
cd tests
pytest -v
```

- **unittest/**: C++ unit tests using Google Test
- **tests/**: Integration tests using Python pytest
- **vslib**: Tests can run against virtual switch without real ASIC hardware
- **saiplayer**: Can replay SAI recordings for regression testing

## PR Guidelines

- **Commit format**: `[component]: Description` (e.g., `[syncd]: Fix warm restart handling`)
- **Signed-off-by**: REQUIRED (`git commit -s`)
- **CLA**: Sign Linux Foundation EasyCLA
- **SAI compatibility**: Ensure changes work with the SAI version in the submodule
- **VS testing**: All changes should be testable with VS platform
- **Recording compatibility**: Don't break SAI recording/replay format

## Common Patterns

### SAI API Implementation Pattern (in lib/)
```cpp
sai_status_t Sai::create(
    sai_object_type_t objectType,
    sai_object_id_t *objectId,
    sai_object_id_t switchId,
    uint32_t attrCount,
    const sai_attribute_t *attrList)
{
    // Serialize to Redis
    // Send to syncd via ASIC_DB
    // Wait for response
}
```

### syncd Processing Pattern
```cpp
// syncd reads from ASIC_DB
// Deserializes SAI calls
// Calls vendor SAI implementation
// Writes results back to ASIC_DB
```

### Virtual Switch Pattern
```cpp
// vslib implements SAI APIs in software
// Maintains in-memory state for all SAI objects
// Used for CI/CD testing and development
```

## Dependencies

- **SAI (submodule)**: Switch Abstraction Interface headers and metadata
- **sonic-swss-common**: Database connectivity, logging, common utilities
- **Vendor SAI SDK**: Platform-specific SAI implementation (Broadcom, Mellanox, etc.)
- **Redis/hiredis**: Database backend
- **libzmq**: ZeroMQ for notification channels

## Gotchas

- **SAI submodule pinning**: The SAI submodule pin must match what's expected by vendor SDKs
- **VID/RID translation**: Incorrect VID↔RID mapping causes hard-to-debug ASIC programming failures
- **Warm restart**: syncd warm restart is complex — it must reconcile pre/post state carefully
- **Thread safety**: syncd processes SAI calls sequentially — blocking calls affect all features
- **VS limitations**: vslib doesn't implement all SAI features — some behaviors differ from real ASICs
- **Recording format**: Changes to SAI call serialization break saiplayer compatibility
- **Notification handling**: SAI notifications (port state, FDB events) are async — handle carefully
- **Memory management**: SAI objects have complex lifecycle — always pair create/remove
- **ASIC_DB schema**: Changes to ASIC_DB entry format affect both syncd and orchagent
