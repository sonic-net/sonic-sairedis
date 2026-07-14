#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Harness assets (e.g. vpp_startup.conf.template) live next to this script in the
# source tree and are installed under /opt/docker-sai-test-vpp in the image. When
# the image runs the script from /usr/local/bin, fall back to the install dir.
HARNESS_DIR="$SCRIPT_DIR"
[[ -f "$HARNESS_DIR/vpp_startup.conf.template" ]] || HARNESS_DIR="/opt/docker-sai-test-vpp"

PORT_COUNT="${PORT_COUNT:-32}"
MTU="${MTU:-9100}"
# Number of PortChannel (LAG) netdevs used for cleanup at container exit.
LAG_COUNT="${LAG_COUNT:-4}"
# LAG/SVI connected-IP patterns consumed by sai_test's SIMULATE_SONIC helper
# (config/simulate_sonic.py), which assigns them in setUp when a LAG or VLAN RIF is
# created. These env vars retarget the helper's defaults to this VPP bench.
LAG_RIF_IPS="${LAG_RIF_IPS:-1}"
LAG_RIF_IPV4_PATTERN="${LAG_RIF_IPV4_PATTERN:-10.1.%d.1/24}"
LAG_RIF_IPV6_PATTERN="${LAG_RIF_IPV6_PATTERN:-fc00:1::%d:1/112}"
# VPP linux_cp host-interface (LCP tap) name prefix for a BondEthernet<N>.
LAG_BE_TAP_PREFIX="${LAG_BE_TAP_PREFIX:-be}"

# Assign the DUT-side connected IP for each SVI (VLAN) router interface (see
# simulate_sonic.assign_svi_rif_ips). Unlike a BondEthernet, a BVI has no linux_cp
# host-interface; the IP is set directly in VPP via vppctl once the BVI exists.
SVI_RIF_IPS="${SVI_RIF_IPS:-1}"
SVI_RIF_VLANS="${SVI_RIF_VLANS:-10:1 20:2}"
SVI_RIF_IPV4_PATTERN="${SVI_RIF_IPV4_PATTERN:-192.168.%d.1/24}"
SVI_RIF_IPV6_PATTERN="${SVI_RIF_IPV6_PATTERN:-fc02::%d:1/112}"
# VPP BVI interface name prefix for a VLAN SVI.
SVI_BVI_PREFIX="${SVI_BVI_PREFIX:-bvi}"
SAI_PROFILE="${SAI_PROFILE:-/etc/sai/sai.profile}"
SAISERVER_PORTMAP="${SAISERVER_PORTMAP:-/etc/sai/port-map.ini}"
PTF_PORTMAP="${PTF_PORTMAP:-/etc/sai/ptf-port-map.ini}"
SAI_TEST_DIR="${SAI_TEST_DIR:-/sai_test}"
TEST_RESULTS_DIR="${TEST_RESULTS_DIR:-/test-results}"
SONIC_VPP_IFMAP="${SONIC_VPP_IFMAP:-/usr/share/sonic/hwsku/sonic_vpp_ifmap.ini}"
THRIFT_PORT="${THRIFT_PORT:-9092}"
STARTUP_TIMEOUT="${STARTUP_TIMEOUT:-60}"
VPPCTL_TIMEOUT="${VPPCTL_TIMEOUT:-5}"
VPP_LOG="${VPP_LOG:-/var/log/vpp.log}"
VPP_STDOUT_LOG="${VPP_STDOUT_LOG:-/var/log/vpp-startup.log}"
VPP_API_TRACE="${VPP_API_TRACE:-/tmp/vpp-sai-api-trace.api}"
VPP_API_TRACE_TXT="${VPP_API_TRACE_TXT:-/var/log/vpp-api-trace.txt}"
SAISERVER_LOG="${SAISERVER_LOG:-/var/log/saiserver.log}"
REDIS_SOCKET="${REDIS_SOCKET:-/var/run/redis/redis.sock}"
REDIS_LOG="${REDIS_LOG:-/var/log/redis.log}"
LINKS_UP_MARKER="${LINKS_UP_MARKER:-/tmp/sai-vpp-links-up}"
LINK_UP_TRIGGER="${LINK_UP_TRIGGER:-Turn up ports...}"
KEEP_VETHS_UP_SECONDS="${KEEP_VETHS_UP_SECONDS:-120}"
KEEP_VETHS_UP_INTERVAL="${KEEP_VETHS_UP_INTERVAL:-3}"

# Port admin-up wait tuning, consumed by the OCP test framework's
# turn_up_and_get_checked_ports() (SAI/test/sai_test/config/port_configer.py).
# In this VPP/veth harness the SAI port oper-status reads DOWN for the whole
# wait window (linux_cp carrier settles only after the config build), yet the
# links do come up and the dataplane works - so the framework's default
# per-port serial wait (32 ports x retries x interval ~= 64s) is pure dead time
# in every common-config build. We opt into the shared bounded wait (all ports
# polled together for at most retries x interval seconds) and a short interval,
# turning ~64s into a few seconds. These are exported so the ptf subprocess
# (and thus port_configer.py) sees them; unset = upstream/real-HW default.
export SAI_PORT_UP_SHARED_WAIT="${SAI_PORT_UP_SHARED_WAIT:-1}"
export SAI_PORT_UP_RETRIES="${SAI_PORT_UP_RETRIES:-2}"
export SAI_PORT_UP_POLL_INTERVAL="${SAI_PORT_UP_POLL_INTERVAL:-1}"

# The SAI PTF T0 framework is designed to build the common switch configuration
# once and have subsequent tests reuse it (the persisted object IDs in
# /tmp/sai_model) by passing common_configured=true. When that path is NOT used,
# every test re-runs sai_create_switch + 32x sai_create_hostif inside the same
# long-lived saiserver/VPP process, which returns a null OID on the duplicate
# host-interface and crashes saiserver - so only the first test can run. With
# reuse enabled (default) the harness runs each test target as its own ptf
# invocation against one long-lived saiserver/VPP: the first builds + persists
# the common config (common_configured=false), the rest reuse it
# (common_configured=true). Set COMMON_CONFIGURED_REUSE=0 to fall back to a
# single ptf invocation for all targets (legacy behavior).
COMMON_CONFIGURED_REUSE="${COMMON_CONFIGURED_REUSE:-1}"

# Per-test isolation. When set to 1 (default), every test target runs in its OWN
# config group: the backend (VPP + saiserver) is torn down and brought up fresh
# before each test, and each test rebuilds its own common config from scratch
# (common_configured=false) rather than reloading a `dut` persisted by a previous
# test. This eliminates all cross-test state contamination (the ECMP -6/-7
# config-reuse artifacts and the v4/v6 NHG port-list aliasing), so the upstream OCP
# tests need no per-test workarounds. It is only affordable because the port-up
# wait was reduced from ~64s to ~6s (see SAI_PORT_UP_SHARED_WAIT and
# devdocs/progress-6-19.md); each test pays ~22s of recycle+rebuild. Set to 0 to
# fall back to config-signature grouping with persisted-config reuse (faster full
# runs, but tests then share a backend within a group).
ISOLATE_EACH_TEST="${ISOLATE_EACH_TEST:-1}"

# Turn on sai_test's SONiC control-plane simulation (config/simulate_sonic.py):
# create PortChannel netdevs and assign LAG/SVI RIF IPs from the test setUp, since
# this standalone bench has no teamd / IntfMgr. The remaining vars retarget the
# helper's interface names / address patterns to this VPP bench.
export SIMULATE_SONIC=1
export LAG_RIF_IPS LAG_RIF_IPV4_PATTERN LAG_RIF_IPV6_PATTERN LAG_BE_TAP_PREFIX
export SVI_RIF_IPS SVI_RIF_VLANS SVI_RIF_IPV4_PATTERN SVI_RIF_IPV6_PATTERN SVI_BVI_PREFIX
# A VLAN SVI (BVI) has no host-interface netdev, so simulate_sonic cannot use "ip".
# Provide the VPP-specific command templates it runs to program the BVI address
# directly ({ifname}/{addr} are substituted). Keeping these here (the harness layer)
# is what lets sai_test's simulate_sonic stay backend-neutral. Note: the {ifname}
# braces can't sit inside a ${VAR:-default} expansion, so guard with a plain if.
if [[ -z "${SVI_RIF_PROBE_CMD:-}" ]]; then
    SVI_RIF_PROBE_CMD="timeout ${VPPCTL_TIMEOUT} vppctl show interface {ifname}"
fi
if [[ -z "${SVI_RIF_SET_IP_CMD:-}" ]]; then
    SVI_RIF_SET_IP_CMD="timeout ${VPPCTL_TIMEOUT} vppctl set interface ip address {ifname} {addr}"
fi
export SVI_RIF_PROBE_CMD SVI_RIF_SET_IP_CMD
export MTU VPPCTL_TIMEOUT

DEBUG=0
TEST_FILTER="${TEST_FILTER:-}"
TEST_FILTERS=()
VPP_PID=""
SAISERVER_PID=""
REDIS_PID=""
VPP_CONF=""
VPP_INIT_CLI=""
KEEP_VETHS_UP_PID=""

log()
{
    echo "[run_test] $*"
}

die()
{
    echo "[run_test] ERROR: $*" >&2
    exit 2
}

usage()
{
    cat <<'EOF'
Usage: run_test.sh [--debug] [TEST_FILTER ...]

Examples:
  run_test.sh
  run_test.sh sai_route_test.RouteRifTest
  run_test.sh sai_route_test.RouteRifTest sai_route_test.RouteRifv6Test
  run_test.sh --debug sai_route_test.RouteRifTest

Multiple TEST_FILTERs run sequentially. By default each target runs as its own
ptf invocation against one long-lived saiserver/VPP: the first builds and
persists the common config, the rest reuse it (common_configured=true). This is
required because the VPP SAI backend cannot re-create the switch/host-interfaces
twice in one process. Set COMMON_CONFIGURED_REUSE=0 to run all targets in a
single legacy invocation.

Without any TEST_FILTER, every discovered test class under /sai_test runs (each
as its own reuse invocation). In debug mode, VPP, saiserver, and veth
interfaces are left running for vppctl inspection.
EOF
}

parse_args()
{
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --debug)
                DEBUG=1
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            --)
                shift
                break
                ;;
            --*)
                die "unknown option: $1"
                ;;
            *)
                break
                ;;
        esac
    done

    if [[ $# -gt 0 ]]; then
        TEST_FILTERS=("$@")
    fi
}

exec_requested_shell()
{
    if [[ $# -eq 0 ]]; then
        return 0
    fi

    case "$1" in
        bash|sh|/bin/bash|/bin/sh)
            exec "$@"
            ;;
    esac
}

require_command()
{
    local command_name="$1"

    command -v "$command_name" >/dev/null 2>&1 || die "required command not found: $command_name"
}

require_file()
{
    local file_path="$1"

    [[ -f "$file_path" ]] || die "required file not found: $file_path"
}

vpp_interface_name()
{
    local port_index="$1"

    echo "OEthernet${port_index}"
}

ptf_interface_name()
{
    local port_index="$1"

    echo "OEth${port_index}_peer"
}

preflight()
{
    [[ "${EUID}" -eq 0 ]] || die "run_test.sh must run as root; use docker run --privileged"

    require_command ip
    require_command vpp
    require_command vppctl
    require_command saiserver
    require_command ptf
    require_command redis-server
    require_command redis-cli
    require_command timeout

    require_file "$SAI_PROFILE"
    require_file "$SAISERVER_PORTMAP"
    require_file "$PTF_PORTMAP"
    [[ -d "$SAI_TEST_DIR" ]] || die "required directory not found: $SAI_TEST_DIR"

    mkdir -p /run/vpp /var/log "$(dirname "$REDIS_SOCKET")" "$TEST_RESULTS_DIR" "$(dirname "$SONIC_VPP_IFMAP")"
}

start_redis()
{
    log "Starting Redis on $REDIS_SOCKET"
    rm -f "$REDIS_SOCKET"

    redis-server \
        --daemonize no \
        --bind 127.0.0.1 \
        --port 0 \
        --unixsocket "$REDIS_SOCKET" \
        --unixsocketperm 777 \
        --save '' \
        --appendonly no \
        --logfile "$REDIS_LOG" &
    REDIS_PID="$!"

    for ((attempt = 1; attempt <= STARTUP_TIMEOUT; attempt++)); do
        if [[ -n "$REDIS_PID" ]] && ! kill -0 "$REDIS_PID" >/dev/null 2>&1; then
            die "Redis exited before becoming ready; see $REDIS_LOG"
        fi

        if redis-cli -s "$REDIS_SOCKET" ping >/dev/null 2>&1; then
            return 0
        fi

        sleep 1
    done

    die "timed out waiting for Redis; see $REDIS_LOG"
}

delete_veths()
{
    set +e
    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        local vpp_if
        local ptf_if

        vpp_if="$(vpp_interface_name "$port_index")"
        ptf_if="$(ptf_interface_name "$port_index")"

        if ip link show "$vpp_if" >/dev/null 2>&1; then
            ip link delete "$vpp_if" >/dev/null 2>&1
        elif ip link show "$ptf_if" >/dev/null 2>&1; then
            ip link delete "$ptf_if" >/dev/null 2>&1
        fi
    done
    set -e
}

disable_ipv6_autoconf()
{
    # Disable IPv6 autoconfiguration on default and all interfaces. This prevents
    # the Linux kernel from automatically generating IPv6 DAD and NDP neighbor/router
    # solicitation packets when interfaces are brought up. Otherwise, at scale
    # (PORT_COUNT=32), VPP raw sockets are flooded with unsolicited packets causing
    # level-triggered poll event starvation of CLI and binary API connections.
    echo 1 > /proc/sys/net/ipv6/conf/default/disable_ipv6 2>/dev/null || true
    echo 1 > /proc/sys/net/ipv6/conf/all/disable_ipv6 2>/dev/null || true
}

delete_portchannels()
{
    set +e
    for ((lag_index = 0; lag_index < LAG_COUNT; lag_index++)); do
        local pc_if="PortChannel${lag_index}"
        if ip link show "$pc_if" >/dev/null 2>&1; then
            ip link delete "$pc_if" >/dev/null 2>&1
        fi
    done
    set -e
}

create_veths()
{
    log "Creating ${PORT_COUNT} OEthernet/OEth peer veth pair(s)"
    delete_veths
    rm -f "$LINKS_UP_MARKER"

    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        local vpp_if
        local ptf_if

        vpp_if="$(vpp_interface_name "$port_index")"
        ptf_if="$(ptf_interface_name "$port_index")"

        ip link add "$vpp_if" type veth peer name "$ptf_if"
        ip link set dev "$vpp_if" mtu "$MTU"
        ip link set dev "$ptf_if" mtu "$MTU"
    done
}

bring_up_veths()
{
    if [[ -f "$LINKS_UP_MARKER" ]]; then
        return 0
    fi

    log "Bringing up ${PORT_COUNT} OEthernet/OEth peer veth pair(s)"

    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        local vpp_if
        local ptf_if

        vpp_if="$(vpp_interface_name "$port_index")"
        ptf_if="$(ptf_interface_name "$port_index")"

        ip link set dev "$vpp_if" up
        ip link set dev "$ptf_if" up
    done

    : > "$LINKS_UP_MARKER"
}

# VPP host-interfaces (host-OEthernetX) are created administratively DOWN by the
# SAI hostif-creation path. While a host-interface is admin-DOWN, linux_cp keeps
# the paired kernel netdev (OEthernetX) in NO-CARRIER/M-DOWN, which drops the
# carrier on the PTF-side veth peer (OEthX_peer) and makes the dataplane unusable
# ("Network is down" on transmit/receive).
#
# Empirically, a single `set interface state host-OEthernetX up` in VPP is enough:
# VPP comes up, linux_cp propagates the state to the kernel netdev (takes a few
# seconds to settle) and brings the peer carrier up, and it stays up afterwards
# (no flapping). Setting the state again is idempotent. So run a short-lived
# background watchdog that re-asserts admin-up for all host-interfaces across the
# host-interface-creation window; interfaces that do not exist yet are simply
# reported as errors by vppctl and ignored.
keep_vpp_veths_up()
{
    local deadline=$((SECONDS + KEEP_VETHS_UP_SECONDS))
    local vpp_up_cmds
    vpp_up_cmds="$(mktemp /tmp/vpp-sai-test-keepup.XXXXXX.cmds)"
    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        echo "set interface state host-OEthernet${port_index} up" >> "$vpp_up_cmds"
    done

    while [[ "$SECONDS" -lt "$deadline" ]]; do
        timeout "$VPPCTL_TIMEOUT" vppctl exec "$vpp_up_cmds" >/dev/null 2>&1 || true
        sleep "$KEEP_VETHS_UP_INTERVAL"
    done

    rm -f "$vpp_up_cmds"
}

create_sonic_vpp_ifmap()
{
    log "Writing SONiC-to-VPP interface map to $SONIC_VPP_IFMAP"
    : > "$SONIC_VPP_IFMAP"

    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        echo "Ethernet$((port_index * 4)) host-OEthernet${port_index}" >> "$SONIC_VPP_IFMAP"
    done
}

generate_vpp_init_cli()
{
    VPP_INIT_CLI="$(mktemp /tmp/vpp-sai-test-init.XXXXXX.cli)"
    : > "$VPP_INIT_CLI"

    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        echo "create host-interface name OEthernet${port_index}" >> "$VPP_INIT_CLI"
        echo "set interface rx-mode host-OEthernet${port_index} interrupt" >> "$VPP_INIT_CLI"
    done
}

generate_vpp_config()
{
    VPP_CONF="$(mktemp /tmp/vpp-sai-test.XXXXXX.conf)"
    local template="${HARNESS_DIR}/vpp_startup.conf.template"
    local buffers_per_numa=$((PORT_COUNT * 2048))

    sed -e "s|__VPP_LOG__|${VPP_LOG}|g" \
        -e "s|__BUFFERS_PER_NUMA__|${buffers_per_numa}|g" \
        -e "s|__VPP_INIT_CLI__|${VPP_INIT_CLI}|g" \
        "$template" > "$VPP_CONF"
}

wait_for_vpp_ready()
{
    log "Waiting for VPP to become ready"

    for ((attempt = 1; attempt <= STARTUP_TIMEOUT; attempt++)); do
        if [[ -n "$VPP_PID" ]] && ! kill -0 "$VPP_PID" >/dev/null 2>&1; then
            die "VPP exited before becoming ready; see $VPP_LOG and $VPP_STDOUT_LOG"
        fi

        if timeout "$VPPCTL_TIMEOUT" vppctl show version >/dev/null 2>&1; then
            return 0
        fi

        sleep 1
    done

    die "timed out waiting for VPP; see $VPP_LOG and $VPP_STDOUT_LOG"
}

verify_vpp_interfaces()
{
    log "Verifying VPP interfaces are created"
    local interface_output
    local port_index
    local attempt

    for ((attempt = 1; attempt <= 15; attempt++)); do
        local all_created=1
        interface_output="$(timeout "$VPPCTL_TIMEOUT" vppctl show interface 2>/dev/null || true)"
        for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
            if ! grep -q "host-OEthernet${port_index}" <<< "$interface_output"; then
                all_created=0
                break
            fi
        done
        if [[ "$all_created" -eq 1 ]]; then
            return 0
        fi
        sleep 1
    done

    die "VPP interfaces were not created; see $VPP_LOG and $VPP_STDOUT_LOG"
}

start_vpp()
{
    generate_vpp_init_cli
    generate_vpp_config
    log "Starting VPP"
    vpp -c "$VPP_CONF" > "$VPP_STDOUT_LOG" 2>&1 &
    VPP_PID="$!"

    wait_for_vpp_ready
    verify_vpp_interfaces
}

wait_for_saiserver_ready()
{
    log "Waiting for saiserver Thrift endpoint on 127.0.0.1:${THRIFT_PORT}"

    for ((attempt = 1; attempt <= STARTUP_TIMEOUT; attempt++)); do
        if [[ -n "$SAISERVER_PID" ]] && ! kill -0 "$SAISERVER_PID" >/dev/null 2>&1; then
            die "saiserver exited before becoming ready; see $SAISERVER_LOG"
        fi

        if (echo >/dev/tcp/127.0.0.1/"$THRIFT_PORT") >/dev/null 2>&1; then
            return 0
        fi

        sleep 1
    done

    die "timed out waiting for saiserver; see $SAISERVER_LOG"
}

start_saiserver()
{
    log "Starting saiserver"
    # libsaivs logs through SWSS_LOG_* (libswsscommon). saiserver.cpp no longer
    # configures swss::Logger after the SAI standalone cleanup; LD_PRELOAD routes
    # those lines to stdout so the redirect below lands in SAISERVER_LOG.
    export LD_PRELOAD="/usr/local/lib/libswss_log_stdout.so${LD_PRELOAD:+:${LD_PRELOAD}}"
    saiserver -p "$SAI_PROFILE" -f "$SAISERVER_PORTMAP" > "$SAISERVER_LOG" 2>&1 &
    SAISERVER_PID="$!"

    wait_for_saiserver_ready
}

build_ptf_args()
{
    # Args: <test_target> <common_configured> <xunit_dir>
    #   test_target      a ptf test selector (module or module.Class), or empty
    #                    for the whole /sai_test suite.
    #   common_configured  "true"  -> reuse the previously persisted common config
    #                      "false" -> build (and persist) the common config
    #                      ""      -> do not pass the param at all (legacy)
    #   xunit_dir        directory PTF writes its JUnit XML into. PTF rmtree's
    #                    this dir on startup, so it must be a private per-call
    #                    dir (NOT a bind mount, NOT shared across invocations).
    local test_target="$1"
    local common_configured="$2"
    local xunit_dir="$3"
    local test_params="thrift_server='127.0.0.1';port_map_file='$PTF_PORTMAP'"

    if [[ -n "$common_configured" ]]; then
        test_params="${test_params};common_configured='${common_configured}'"
    fi

    PTF_ARGS=(--test-dir "$SAI_TEST_DIR")

    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        PTF_ARGS+=(--interface "${port_index}@$(ptf_interface_name "$port_index")")
    done

    PTF_ARGS+=(--test-params "$test_params")
    PTF_ARGS+=(--xunit --xunit-dir "$xunit_dir")

    # The T0 sanity flood test sends one unknown-unicast frame and expects it to
    # be flooded to every other member port of the VLAN. PTF's flood verifier
    # (verify_each_packet_on_multiple_port_lists) consumes only one copy from the
    # expected port list and then asserts via verify_no_other_packets() that no
    # further packets remain - which is impossible for a real flood that delivers
    # a copy to every member port. --relax makes verify_no_other_packets() a
    # no-op, which is the intended/standard mode for flooding tests.
    PTF_ARGS+=(--relax)

    if [[ -n "$test_target" ]]; then
        PTF_ARGS+=("$test_target")
    fi
}

# Enumerate every test class under $SAI_TEST_DIR as "module.Class" selectors so
# each can run as its own config-reuse ptf invocation. Best-effort: on any
# failure prints nothing and the caller falls back to a single invocation.
enumerate_test_classes()
{
    python3 - "$SAI_TEST_DIR" <<'PY' 2>/dev/null || true
import sys, unittest
test_dir = sys.argv[1]
try:
    suite = unittest.TestLoader().discover(
        test_dir, pattern="sai_*_test.py", top_level_dir=test_dir)
except Exception:
    sys.exit(0)
seen = []
def walk(s):
    for t in s:
        if isinstance(t, unittest.TestSuite):
            walk(t)
        else:
            cls = t.__class__
            name = "%s.%s" % (cls.__module__, cls.__name__)
            if name not in seen:
                seen.append(name)
walk(suite)
for n in seen:
    print(n)
PY
}

print_debug_state()
{
    set +e
    log "Linux veth interfaces"
    ip -br link show type veth

    if command -v vppctl >/dev/null 2>&1 && timeout "$VPPCTL_TIMEOUT" vppctl show version >/dev/null 2>&1; then
        log "VPP interfaces"
        timeout "$VPPCTL_TIMEOUT" vppctl show interface
        log "VPP hardware interfaces"
        timeout "$VPPCTL_TIMEOUT" vppctl show hardware-interfaces
        log "Saving VPP API trace to $VPP_API_TRACE"
        timeout "$VPPCTL_TIMEOUT" vppctl api trace save "$(basename "$VPP_API_TRACE")"
        log "Writing decoded VPP API trace to $VPP_API_TRACE_TXT"
        timeout "$VPPCTL_TIMEOUT" vppctl api trace dump > "$VPP_API_TRACE_TXT" 2>&1
    fi

    log "Last 200 lines of $VPP_STDOUT_LOG"
    tail -n 200 "$VPP_STDOUT_LOG" 2>/dev/null
    log "Last 200 lines of $VPP_LOG"
    tail -n 200 "$VPP_LOG" 2>/dev/null
    log "Last 200 lines of $SAISERVER_LOG"
    tail -n 200 "$SAISERVER_LOG" 2>/dev/null
    log "Last 200 lines of $REDIS_LOG"
    tail -n 200 "$REDIS_LOG" 2>/dev/null
    set -e
}

terminate_process()
{
    local process_name="$1"
    local process_pid="$2"

    if [[ -z "$process_pid" ]]; then
        return 0
    fi

    if ! kill -0 "$process_pid" >/dev/null 2>&1; then
        wait "$process_pid" 2>/dev/null || true
        return 0
    fi

    kill "$process_pid" >/dev/null 2>&1 || true
    for ((attempt = 1; attempt <= 5; attempt++)); do
        if ! kill -0 "$process_pid" >/dev/null 2>&1; then
            wait "$process_pid" 2>/dev/null || true
            return 0
        fi
        sleep 1
    done

    log "Force stopping $process_name"
    kill -9 "$process_pid" >/dev/null 2>&1 || true
    wait "$process_pid" 2>/dev/null || true
}

# In --debug mode, hold the script (PID 1) open after the tests finish so the
# container keeps running with VPP, saiserver, Redis, and the veths still up for
# interactive `vppctl` / `docker exec` inspection. Without this the entrypoint
# would exit right after the last test and Docker would stop the container,
# tearing down exactly the state debug mode is meant to preserve. Exit the
# container with `docker rm -f <name>` when done.
debug_hold()
{
    [[ "$DEBUG" -eq 1 ]] || return 0

    log "Debug mode: tests complete; holding container open for inspection."
    log "Inspect with: docker exec <name> vppctl show interface"
    log "Stop with:    docker rm -f <name>"
    # Sleep in a loop so the process stays in PID 1 and reaps cleanly on signal.
    while true; do
        sleep 3600 &
        wait "$!"
    done
}

cleanup()
{
    local status="$?"

    set +e
    if [[ -n "$KEEP_VETHS_UP_PID" ]]; then
        kill "$KEEP_VETHS_UP_PID" >/dev/null 2>&1 || true
    fi
    if [[ "$status" -ne 0 || "$DEBUG" -eq 1 ]]; then
        print_debug_state
    fi

    if [[ "$DEBUG" -eq 1 ]]; then
        log "Debug mode enabled; leaving VPP, saiserver, and veth interfaces running"
        return "$status"
    fi

    log "Cleaning up runtime state"
    terminate_process saiserver "$SAISERVER_PID"
    terminate_process vpp "$VPP_PID"
    terminate_process redis "$REDIS_PID"
    delete_veths
    delete_portchannels
    [[ -n "$VPP_CONF" ]] && rm -f "$VPP_CONF"
    [[ -n "$VPP_INIT_CLI" ]] && rm -f "$VPP_INIT_CLI"
    set -e

    return "$status"
}

run_ptf()
{
    local -a targets=()

    if [[ "${#TEST_FILTERS[@]}" -gt 0 ]]; then
        targets=("${TEST_FILTERS[@]}")
    elif [[ -n "$TEST_FILTER" ]]; then
        targets=("$TEST_FILTER")
    fi

    # Legacy single invocation (reuse disabled): start the backend once and run
    # exactly one ptf process for whatever was requested.
    if [[ "$COMMON_CONFIGURED_REUSE" != "1" ]]; then
        local legacy_target=""
        [[ "${#targets[@]}" -gt 0 ]] && legacy_target="${targets[0]}"
        start_backend
        local legacy_rc=0
        run_one_ptf "$legacy_target" "" || legacy_rc="$?"
        debug_hold
        exit "$legacy_rc"
    fi

    # Reuse mode. Plan the run as a sequence of (group_id, target) lines, grouped
    # by each test's common-config signature (the kwargs it passes to
    # T0TestBase.setUp). Tests that need a DIFFERENT common config than the
    # group's first test cannot reuse it (e.g. ECMP tests need next-hop groups),
    # so each signature gets its own group. Every group runs against a FRESHLY
    # restarted backend: the first test in the group builds + persists that
    # group's config (common_configured=false), the rest reuse it
    # (common_configured=true). The backend restart (fresh saiserver) is what
    # makes a second full config build safe — building twice in ONE saiserver
    # process crashes it (the original "one test per container" bug).
    local plan
    if [[ "${#targets[@]}" -gt 0 ]]; then
        plan="$(plan_test_groups "${targets[@]}")"
    else
        log "No test target given; planning all discovered test classes"
        plan="$(plan_test_groups)"
    fi

    # Per-test isolation: rewrite the group id of every plan line to a unique value
    # so each test gets its own freshly restarted backend and rebuilds its own
    # common config (common_configured=false). The signature column is preserved for
    # logging. This is what lets the upstream OCP tests stay free of config-reuse
    # workarounds.
    if [[ "$ISOLATE_EACH_TEST" == "1" && -n "$plan" ]]; then
        plan="$(printf '%s\n' "$plan" | awk -F'\t' 'NF{printf "%d\t%s\t%s\n", NR-1, $2, $3}')"
        log "ISOLATE_EACH_TEST=1: each test runs in its own group (fresh backend + own config)"
    fi

    if [[ -z "$plan" ]]; then
        log "Planning produced no targets; running full suite in one invocation"
        start_backend
        local suite_rc=0
        run_one_ptf "" "false" || suite_rc="$?"
        debug_hold
        exit "$suite_rc"
    fi

    local total
    total="$(printf '%s\n' "$plan" | grep -c .)"
    log "Planned ${total} test target(s) across $(printf '%s\n' "$plan" | cut -f1 | sort -u | grep -c .) config group(s)"

    local overall_rc=0
    local prev_group=""
    local idx_in_group=0
    local idx=0
    local group target sig common_configured rc

    while IFS=$'\t' read -r group target sig; do
        [[ -z "$target" ]] && continue
        idx=$((idx + 1))

        if [[ "$group" != "$prev_group" ]]; then
            # New config group: restart the backend for a clean saiserver, then
            # the first test of the group rebuilds the common config.
            if [[ -n "$prev_group" ]]; then
                stop_backend
            fi
            log "### Config group ${group} (signature: ${sig:-()}) ###"
            start_backend
            prev_group="$group"
            idx_in_group=0
        fi

        if [[ "$idx_in_group" -eq 0 ]]; then
            common_configured="false"
        else
            common_configured="true"
        fi
        idx_in_group=$((idx_in_group + 1))

        log "=== [${idx}/${total}] ${target} (group ${group}, common_configured=${common_configured}) ==="
        rc=0
        run_one_ptf "$target" "$common_configured" || rc="$?"
        if [[ "$rc" -ne 0 ]]; then
            overall_rc="$rc"
            log "test target '${target}' returned rc=${rc}"
        fi
    done <<< "$plan"

    debug_hold
    exit "$overall_rc"
}

# Start the runtime backend (Redis + VPP + saiserver + veth-up watchdog). VPP and
# saiserver are fresh processes each time this is called, so a subsequent group's
# common_configured=false config build runs in a clean saiserver and does not hit
# the duplicate-create crash. Veth netdevs persist across backend restarts;
# PortChannel netdevs are created on demand in sai_test setUp (SIMULATE_SONIC).
# across restarts; only the dataplane daemons are recycled.
start_backend()
{
    start_redis
    start_vpp
    start_saiserver
    keep_vpp_veths_up &
    KEEP_VETHS_UP_PID="$!"
}

# Stop the runtime backend and reset per-run state so the next group starts clean.
#
# NOTE: ideally each test would restore the initial state via SAI teardown in its
# own tearDown(), avoiding this full backend recycle. That does not fully work yet
# because the VS-VPP backend's object removal is not idempotent enough to rebuild
# the whole T0 config a second time in one saiserver process: on remove, leftover
# VPP state (linux-cp pairs, bridge domains, bonds, etc.) is not always torn down,
# so the next create hits "already exists" (VPP VALUE_EXIST/-81) and fails. The
# host-interface case was fixed in sonic-sairedis PR #1952 (vs_remove_hostif now
# deletes the linux-cp pair + disables IPv6, and create tolerates VALUE_EXIST), but
# VLAN, bridge-port, LAG, RIF and route removal still need the same treatment.
# Until that is done, we restart the backend (fresh saiserver) per group so each
# config build starts from clean VPP state. See devdocs/progress-7-3-hostif-removal.md.
stop_backend()
{
    if [[ -n "$KEEP_VETHS_UP_PID" ]]; then
        kill "$KEEP_VETHS_UP_PID" >/dev/null 2>&1 || true
        wait "$KEEP_VETHS_UP_PID" 2>/dev/null || true
        KEEP_VETHS_UP_PID=""
    fi
    terminate_process saiserver "$SAISERVER_PID"; SAISERVER_PID=""
    terminate_process vpp "$VPP_PID"; VPP_PID=""
    terminate_process redis "$REDIS_PID"; REDIS_PID=""
    # Drop persisted SAI object IDs and the link-up marker so the next group's
    # first test rebuilds (and re-persists) its own config and re-asserts veths.
    rm -rf /tmp/sai_model 2>/dev/null || true
    rm -f "$LINKS_UP_MARKER" 2>/dev/null || true
}

# Emit a run plan: tab-separated "<group_id>\t<module.Class>\t<signature>" lines,
# grouped by each test class's common-config signature so identical-config tests
# run together (and can reuse one config build). With no args, plans every
# discovered test class under $SAI_TEST_DIR.
plan_test_groups()
{
    SAI_TEST_DIR="$SAI_TEST_DIR" python3 - "$@" <<'PY'
import ast, os, sys, glob, collections

test_dir = os.environ.get("SAI_TEST_DIR", "/sai_test")
targets = sys.argv[1:]

# kwargs that do NOT change the common config (so they must not split groups)
NON_CONFIG_KW = {"skip_reason", "wait_sec"}

mod_file = {}
for p in glob.glob(os.path.join(test_dir, "sai_*_test.py")):
    mod_file[os.path.basename(p)[:-3]] = p

# Build a cross-file class registry: name -> {bases:[...], setup:FunctionDef|None,
# module:str}. Classes can subclass intermediate test bases (e.g. EcmpBaseTestV4)
# whose setUp sets the common-config kwargs, so signatures must resolve through
# the inheritance chain, not just the class's own setUp.
registry = {}
mod_classes = collections.OrderedDict()
for mod in sorted(mod_file):
    try:
        tree = ast.parse(open(mod_file[mod]).read(), mod_file[mod])
    except Exception:
        continue
    names = []
    for node in tree.body:
        if not isinstance(node, ast.ClassDef):
            continue
        names.append(node.name)
        bases = [b.id for b in node.bases if isinstance(b, ast.Name)]
        setup = next((m for m in node.body
                      if isinstance(m, ast.FunctionDef) and m.name == "setUp"), None)
        # last definition wins if duplicated
        registry[node.name] = {"bases": bases, "setup": setup, "module": mod}
    mod_classes[mod] = names

def _kwargs_from_setup_call(setup):
    """Return (kwargs_str_or_None, base_to_recurse_or_None) for a class's setUp.
    kwargs_str: config signature if this setUp passes config kwargs.
    base_to_recurse: if setUp only chains to super()/Base.setUp() without config
    kwargs, the class name to resolve the signature from."""
    for sub in ast.walk(setup):
        if not (isinstance(sub, ast.Call) and isinstance(sub.func, ast.Attribute)
                and sub.func.attr == "setUp"):
            continue
        kws = []
        for kw in sub.keywords:
            if kw.arg in NON_CONFIG_KW or kw.arg is None:
                continue
            try:
                val = ast.literal_eval(kw.value)
            except Exception:
                val = "?"
            kws.append("%s=%s" % (kw.arg, val))
        if kws:
            return ("|".join(sorted(kws)), None)
        # No config kwargs: figure out which base this chains to.
        f = sub.func.value
        if isinstance(f, ast.Call) and isinstance(f.func, ast.Name) \
                and f.func.id == "super":
            return (None, "__super__")          # super().setUp()
        if isinstance(f, ast.Name):
            return (None, f.id)                 # <Base>.setUp(self, ...)
        return (None, "__super__")
    return ("()", None)

def signature(clsname, seen=None):
    if seen is None:
        seen = set()
    if clsname in seen or clsname not in registry:
        return "()"   # unknown base (e.g. T0TestBase) -> default config
    seen.add(clsname)
    info = registry[clsname]
    setup = info["setup"]
    if setup is None:
        # inherits setUp wholesale from first base
        for b in info["bases"]:
            return signature(b, seen)
        return "()"
    sig, base = _kwargs_from_setup_call(setup)
    if sig is not None:
        return sig
    # chained to super/base without config kwargs -> resolve that base
    if base == "__super__":
        for b in info["bases"]:
            return signature(b, seen)
        return "()"
    return signature(base, seen)

# Expand requested targets (or everything) into (module, class) pairs.
pairs = []
if not targets:
    for mod in mod_classes:
        for c in mod_classes[mod]:
            pairs.append((mod, c))
else:
    for t in targets:
        if "." in t:
            mod, cls = t.split(".", 1)
            pairs.append((mod, cls))
        elif t in mod_classes:
            for c in mod_classes[t]:
                pairs.append((t, c))
        else:
            pairs.append((t, ""))   # unknown selector: run as-is, default group

# Group by resolved signature, preserving first-seen order.
groups = collections.OrderedDict()
for mod, cls in pairs:
    sig = signature(cls) if cls else "()"
    groups.setdefault(sig, []).append("%s.%s" % (mod, cls) if cls else mod)

for gid, (sig, tlist) in enumerate(groups.items()):
    for t in tlist:
        sys.stdout.write("%d\t%s\t%s\n" % (gid, t, sig))
PY
}

# Run a single ptf invocation for one target. Echoes PTF output through and
# triggers the one-time veth bring-up on the framework's "Turn up ports..."
# marker. Returns ptf's exit code.
#
# PTF rmtree's its --xunit-dir on startup, so each invocation gets its own
# private temp dir (which PTF may freely wipe), and the resulting JUnit XML is
# copied into the shared $TEST_RESULTS_DIR afterward. This keeps results from
# accumulating across invocations and avoids EBUSY when $TEST_RESULTS_DIR is a
# bind mount (rmtree of a mount point fails).
run_one_ptf()
{
    local test_target="$1"
    local common_configured="$2"
    local test_rc
    local xunit_dir

    xunit_dir="$(mktemp -d /tmp/ptf-xunit.XXXXXX)"

    build_ptf_args "$test_target" "$common_configured" "$xunit_dir"
    log "Running PTF${test_target:+ filter: $test_target}"

    set +e
    PYTHONUNBUFFERED=1 ptf "${PTF_ARGS[@]}" 2>&1 | while IFS= read -r ptf_line; do
        printf '%s\n' "$ptf_line"

        case "$ptf_line" in
            *"Turn up ports..."*)
                bring_up_veths
                ;;
        esac
    done
    test_rc="${PIPESTATUS[0]}"
    set -e

    # Collect this invocation's JUnit XML into the shared results dir.
    if [[ -n "$TEST_RESULTS_DIR" ]]; then
        mkdir -p "$TEST_RESULTS_DIR"
        cp -f "$xunit_dir"/*.xml "$TEST_RESULTS_DIR"/ 2>/dev/null || true
    fi
    rm -rf "$xunit_dir"

    return "$test_rc"
}

main()
{
    exec_requested_shell "$@"
    parse_args "$@"
    trap cleanup EXIT

    preflight
    disable_ipv6_autoconf
    create_veths
    create_sonic_vpp_ifmap
    run_ptf
}

main "$@"