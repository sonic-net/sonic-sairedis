#!/bin/bash
set -euo pipefail

PORT_COUNT="${PORT_COUNT:-32}"
MTU="${MTU:-9100}"
SAI_PROFILE="${SAI_PROFILE:-/etc/sai/sai.profile}"
SAISERVER_PORTMAP="${SAISERVER_PORTMAP:-/etc/sai/port-map.ini}"
PTF_PORTMAP="${PTF_PORTMAP:-/etc/sai/ptf-port-map.ini}"
SAI_TEST_DIR="${SAI_TEST_DIR:-/sai_test}"
TEST_RESULTS_DIR="${TEST_RESULTS_DIR:-/test-results}"
SONIC_VPP_IFMAP="${SONIC_VPP_IFMAP:-/usr/share/sonic/hwsku/sonic_vpp_ifmap.ini}"
THRIFT_PORT="${THRIFT_PORT:-9092}"
STARTUP_TIMEOUT="${STARTUP_TIMEOUT:-60}"
VPPCTL_TIMEOUT="${VPPCTL_TIMEOUT:-5}"
VPP_CREATE_BATCH_SIZE="${VPP_CREATE_BATCH_SIZE:-8}"
VPP_LOG="${VPP_LOG:-/var/log/vpp.log}"
VPP_STDOUT_LOG="${VPP_STDOUT_LOG:-/var/log/vpp-startup.log}"
SAISERVER_LOG="${SAISERVER_LOG:-/var/log/saiserver.log}"
REDIS_SOCKET="${REDIS_SOCKET:-/var/run/redis/redis.sock}"
REDIS_LOG="${REDIS_LOG:-/var/log/redis.log}"
LINKS_UP_MARKER="${LINKS_UP_MARKER:-/tmp/sai-vpp-links-up}"

DEBUG=0
TEST_FILTER="${TEST_FILTER:-}"
VPP_PID=""
SAISERVER_PID=""
REDIS_PID=""
VPP_CONF=""

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
Usage: run_test.sh [--debug] [TEST_FILTER]

Examples:
  run_test.sh
  run_test.sh sai_route_test.RouteRifTest
  run_test.sh --debug sai_route_test.RouteRifTest

Without TEST_FILTER, PTF runs the full /sai_test suite. In debug mode,
VPP, saiserver, and veth interfaces are left running for vppctl inspection.
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
        TEST_FILTER="$1"
        shift
    fi

    if [[ $# -gt 0 ]]; then
        die "unexpected extra argument(s): $*"
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
    [[ "$VPP_CREATE_BATCH_SIZE" =~ ^[0-9]+$ ]] || die "VPP_CREATE_BATCH_SIZE must be a positive integer"
    [[ "$VPP_CREATE_BATCH_SIZE" -gt 0 ]] || die "VPP_CREATE_BATCH_SIZE must be greater than zero"

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

create_sonic_vpp_ifmap()
{
    log "Writing SONiC-to-VPP interface map to $SONIC_VPP_IFMAP"
    : > "$SONIC_VPP_IFMAP"

    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        echo "Ethernet$((port_index * 4)) OEthernet${port_index}" >> "$SONIC_VPP_IFMAP"
    done
}

generate_vpp_config()
{
    VPP_CONF="$(mktemp /tmp/vpp-sai-test.XXXXXX.conf)"

    cat > "$VPP_CONF" <<EOF
unix {
  nodaemon
  log $VPP_LOG
  full-coredump
  cli-listen /run/vpp/cli.sock
}

api-trace {
  on
}

api-segment {
  global-size 256M
  api-size 64M
}

socksvr {
  default
}

memory {
    main-heap-size 2G
}

l3fib {
    fib-entry-pool-size 256K
    load-balance-pool-size 256K
    ip4-mtrie-pool-size 256K
}

ip6 {
    heap-size 128M
}

plugins {
  plugin default { disable }
  plugin af_packet_plugin.so { enable }
  plugin linux_cp_plugin.so { enable }
  plugin linux_nl_plugin.so { enable }
  plugin acl_plugin.so { enable }
  plugin vxlan_plugin.so { enable }
  plugin tunterm_acl_plugin.so { enable }
  plugin ip_validate_plugin.so { enable }
}

linux-cp {
  lcp-auto-subint
}

buffers {
  buffers-per-numa $((PORT_COUNT * 2048))
}
EOF
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

create_vpp_host_interfaces()
{
    local batch_cmds=""
    local batch_count=0
    local batch_start=0

    log "Creating VPP host interfaces"

    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        if [[ "$batch_count" -eq 0 ]]; then
            batch_cmds="$(mktemp /tmp/vpp-sai-test-create.XXXXXX.cmds)"
            batch_start="$port_index"
        fi

        echo "create host-interface name OEthernet${port_index}" >> "$batch_cmds"
        batch_count=$((batch_count + 1))

        if [[ "$batch_count" -eq "$VPP_CREATE_BATCH_SIZE" ]]; then
            if ! timeout "$STARTUP_TIMEOUT" vppctl exec "$batch_cmds" >/dev/null; then
                rm -f "$batch_cmds"
                die "failed to create VPP host interfaces ${batch_start}..${port_index}; see $VPP_LOG and $VPP_STDOUT_LOG"
            fi
            rm -f "$batch_cmds"
            batch_cmds=""
            batch_count=0
        fi
    done

    if [[ "$batch_count" -gt 0 ]]; then
        if ! timeout "$STARTUP_TIMEOUT" vppctl exec "$batch_cmds" >/dev/null; then
            rm -f "$batch_cmds"
            die "failed to create VPP host interfaces ${batch_start}..$((PORT_COUNT - 1)); see $VPP_LOG and $VPP_STDOUT_LOG"
        fi
        rm -f "$batch_cmds"
    fi
}

verify_vpp_interfaces()
{
    local interface_output

    interface_output="$(timeout "$VPPCTL_TIMEOUT" vppctl show interface 2>/dev/null || true)"

    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        if ! grep -q "host-OEthernet${port_index}" <<< "$interface_output"; then
            die "VPP interface host-OEthernet${port_index} was not created"
        fi
    done
}

set_vpp_rx_mode_interrupt()
{
    local rx_mode_cmds

    log "Setting VPP AF_PACKET RX mode to interrupt"
    rx_mode_cmds="$(mktemp /tmp/vpp-sai-test-rxmode.XXXXXX.cmds)"

    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        echo "set interface rx-mode host-OEthernet${port_index} interrupt" >> "$rx_mode_cmds"
    done

    if ! timeout "$STARTUP_TIMEOUT" vppctl exec "$rx_mode_cmds" >/dev/null; then
        rm -f "$rx_mode_cmds"
        die "failed to set VPP host interface RX mode; see $VPP_LOG and $VPP_STDOUT_LOG"
    fi

    rm -f "$rx_mode_cmds"
}

start_vpp()
{
    generate_vpp_config
    log "Starting VPP"
    vpp -c "$VPP_CONF" > "$VPP_STDOUT_LOG" 2>&1 &
    VPP_PID="$!"

    wait_for_vpp_ready
    create_vpp_host_interfaces
    verify_vpp_interfaces
    set_vpp_rx_mode_interrupt
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
    saiserver -p "$SAI_PROFILE" -f "$SAISERVER_PORTMAP" > "$SAISERVER_LOG" 2>&1 &
    SAISERVER_PID="$!"

    wait_for_saiserver_ready
}

build_ptf_args()
{
    PTF_ARGS=(--test-dir "$SAI_TEST_DIR")

    for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
        PTF_ARGS+=(--interface "${port_index}@$(ptf_interface_name "$port_index")")
    done

    PTF_ARGS+=(--test-params "thrift_server='127.0.0.1';port_map_file='$PTF_PORTMAP'")
    PTF_ARGS+=(--xunit --xunit-dir "$TEST_RESULTS_DIR")

    if [[ -n "$TEST_FILTER" ]]; then
        PTF_ARGS+=("$TEST_FILTER")
    fi
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

    if [[ -z "$process_pid" ]] || ! kill -0 "$process_pid" >/dev/null 2>&1; then
        return 0
    fi

    kill "$process_pid" >/dev/null 2>&1 || true
    for ((attempt = 1; attempt <= 5; attempt++)); do
        if ! kill -0 "$process_pid" >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
    done

    log "Force stopping $process_name"
    kill -9 "$process_pid" >/dev/null 2>&1 || true
}

cleanup()
{
    local status="$?"

    set +e
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
    [[ -n "$VPP_CONF" ]] && rm -f "$VPP_CONF"
    set -e

    return "$status"
}

run_ptf()
{
    local test_rc

    build_ptf_args
    log "Running PTF${TEST_FILTER:+ filter: $TEST_FILTER}"

    set +e
    PYTHONUNBUFFERED=1 ptf "${PTF_ARGS[@]}" 2>&1 | while IFS= read -r ptf_line; do
        printf '%s\n' "$ptf_line"

        case "$ptf_line" in
            *"common config done"*)
                bring_up_veths
                ;;
        esac
    done
    test_rc="${PIPESTATUS[0]}"
    set -e

    exit "$test_rc"
}

main()
{
    exec_requested_shell "$@"
    parse_args "$@"
    trap cleanup EXIT

    preflight
    start_redis
    create_veths
    create_sonic_vpp_ifmap
    start_vpp
    start_saiserver
    run_ptf
}

main "$@"