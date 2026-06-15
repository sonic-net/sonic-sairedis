# VPP SAI Unit Test Framework - Phase 1 Progress Report

### Sections
- [1. File Architectures (Block-by-Block Explanation)](#1-file-architectures-block-by-block-explanation)
- [2. Working with the Test Container (Step-by-Step Guide)](#2-working-with-the-test-container-step-by-step-guide)
- [3. Errors Fixed](#3-errors-fixed)
- [4. The Current Blocking Issue (The TAP Dependency Lock)](#4-the-current-blocking-issue-the-tap-dependency-lock)

---

This report documents the design, usage, solved problems, and remaining architectural blockers for the Phase 1 MVP of the VPP SAI Unit Test framework. It is written for developers and engineers who are familiar with software networking but may not be familiar with the internal details of the OCP `sai_test` suite, Thrift RPC, or VPP’s translation driver layer.

---

## 1. File Architectures (Block-by-Block Explanation)

### A. Dockerfile (`.azure-pipelines/docker-sai-test-vpp/Dockerfile`)

The Dockerfile defines a sealed, self-contained environment running Debian Bookworm. It installs all dependencies, registers configuration files, and sets up the entrypoint.

* **Block 1: Base Image & Env Variables**
  Sets the main OS image and default paths:
  ```dockerfile
  FROM debian:bookworm

  ENV DEBIAN_FRONTEND=noninteractive \
      PIP_BREAK_SYSTEM_PACKAGES=1 \
      PYTHONUNBUFFERED=1 \
      SAI_PROFILE=/etc/sai/sai.profile \
      SAISERVER_PORTMAP=/etc/sai/port-map.ini \
      PTF_PORTMAP=/etc/sai/ptf-port-map.ini \
      SAI_TEST_DIR=/sai_test \
      TEST_RESULTS_DIR=/test-results
  ```
* **Block 2: Source File Copy**
  Copies the test framework code and the configuration directory into their expected paths:
  ```dockerfile
  COPY .azure-pipelines/docker-sai-test-vpp /opt/docker-sai-test-vpp
  COPY SAI/test/ptf /opt/ptf
  COPY SAI/test/sai_test /sai_test
  ```
* **Block 3: APT Package Installation**
  Installs base prerequisite networking, database, structures, and Python components from Debian's official repository:
  ```dockerfile
  RUN echo "deb http://deb.debian.org/debian bookworm-backports main" > /etc/apt/sources.list.d/bookworm-backports.list && \
      apt-get update && \
      apt-get install -y --no-install-recommends \
          bash \
          ca-certificates \
          iproute2 \
          libboost-serialization1.83.0/bookworm-backports \
          libpcap0.8 \
          libthrift-0.17.0 \
          libzmq5 \
          procps \
          python3 \
          python3-pip \
          python3-setuptools \
          python3-thrift \
          python3-wheel \
          redis-server \
          redis-tools \
          tcpdump && \
      rm -rf /var/lib/apt/lists/*
  ```
* **Block 4: Python PIP Packages**
  Compiles and installs Scapy and the PTF framework inside the container:
  ```dockerfile
  RUN python3 -m pip install --no-cache-dir scapy==2.5.0 unittest-xml-reporting==3.2.0 && \
      python3 -m pip install --no-cache-dir /opt/ptf
  ```
* **Block 5: Local Package Staging & Smoke Test**
  Validates that every single compiled deb is present, installs them while allowing downgrades to match local requirements, overrides sysctl, and finally verifies saithrift is correctly importable in Python:
  ```dockerfile
  RUN set -eux; \
      deb_dir=/opt/docker-sai-test-vpp/debs; \
      require_deb() { \
          local label="$1"; \
          shift; \
          local found=0; \
          local pattern; \
          for pattern in "$@"; do \
              if compgen -G "${deb_dir}/${pattern}" >/dev/null; then \
                  found=1; \
              fi; \
          done; \
          if [[ "$found" -eq 0 ]]; then \
              echo "Missing ${label}; expected one of: $*" >&2; \
              exit 1; \
          fi; \
      }; \
      require_deb "VPP infra package" "libvppinfra_*.deb"; \
      require_deb "VPP package" "vpp_*.deb"; \
      require_deb "VPP core plugin package" "vpp-plugin-core_*.deb"; \
      require_deb "VPP DPDK plugin package" "vpp-plugin-dpdk_*.deb"; \
      require_deb "SAI virtual switch package" "libsaivs_*.deb"; \
      require_deb "SAI Redis package" "libsairedis_*.deb"; \
      require_deb "SAI metadata package" "libsaimetadata_*.deb"; \
      require_deb "SONiC SWSS common package" "libswsscommon_*.deb"; \
      require_deb "YANG runtime package" "libyang_*.deb"; \
      require_deb "SAI thrift server package" "saiserver_*.deb" "saiserverv2_*.deb"; \
      require_deb "SAI Python thrift package" "python-saithrift_*.deb" "python-saithriftv2_*.deb"; \
      runtime_debs=(); \
      have_saiserverv2=0; \
      have_python_saithriftv2=0; \
      compgen -G "${deb_dir}/saiserverv2_*.deb" >/dev/null && have_saiserverv2=1; \
      compgen -G "${deb_dir}/python-saithriftv2_*.deb" >/dev/null && have_python_saithriftv2=1; \
      while IFS= read -r deb_file; do \
          deb_name="$(basename "$deb_file")"; \
          case "$deb_name" in \
              *-dbg_*.deb|*-dbgsym_*.deb|*-dev_*.deb) continue ;; \
              libyang3-tools_*.deb) continue ;; \
              saiserver_*.deb) [[ "$have_saiserverv2" -eq 1 ]] && continue ;; \
              python-saithrift_*.deb) [[ "$have_python_saithriftv2" -eq 1 ]] && continue ;; \
          esac; \
          runtime_debs+=("$deb_file"); \
      done < <(find "$deb_dir" -maxdepth 1 -type f -name '*.deb' | sort); \
      if [[ "${#runtime_debs[@]}" -eq 0 ]]; then \
          echo "No runtime .deb packages found under ${deb_dir}" >&2; \
          exit 1; \
      fi; \
      apt-get update; \
      cp /usr/sbin/sysctl /usr/sbin/sysctl.real; \
      cp /usr/bin/true /usr/sbin/sysctl; \
      apt-get install -y --no-install-recommends --allow-downgrades "${runtime_debs[@]}"; \
      find /usr/lib/python3/dist-packages -maxdepth 1 -type d -name 'saithrift-*.egg' -print > /usr/lib/python3/dist-packages/saithrift.pth; \
      test -s /usr/lib/python3/dist-packages/saithrift.pth; \
      python3 -c 'import sai_thrift'; \
      mv /usr/sbin/sysctl.real /usr/sbin/sysctl; \
      apt-get clean; \
      rm -rf /var/lib/apt/lists/*
  ```
* **Block 6: Config Registration & Entrypoint**
  Registers and copies static config mapping files, links the execution target, and marks the starting command:
  ```dockerfile
  RUN install -d \
          /etc/sai \
          /run/vpp \
          /var/run/redis \
          /test-results \
          /usr/share/sonic/hwsku \
          /var/log && \
      install -m 0644 /opt/docker-sai-test-vpp/sai.profile /etc/sai/sai.profile && \
      install -m 0644 /opt/docker-sai-test-vpp/lanemap.ini /etc/sai/lanemap.ini && \
      install -m 0644 /opt/docker-sai-test-vpp/port-map.ini /etc/sai/port-map.ini && \
      install -m 0644 /opt/docker-sai-test-vpp/ptf-port-map.ini /etc/sai/ptf-port-map.ini && \
      install -m 0755 /opt/docker-sai-test-vpp/run_test.sh /usr/local/bin/run_test.sh && \
      ln -sf /usr/local/bin/run_test.sh /run_test.sh

  WORKDIR /

  ENTRYPOINT ["/usr/local/bin/run_test.sh"]
  ```

---

### B. Entrypoint Script (`run_test.sh`)

`run_test.sh` controls the execution of the entire pipeline, managing processes and virtual interfaces.

* **Block 1: Defaults & Parameter Parsing**
  Grabs runtime configuration variables and parses cmdline filters:
  ```bash
  PORT_COUNT="${PORT_COUNT:-32}"
  MTU="${MTU:-9100}"
  SAI_PROFILE="${SAI_PROFILE:-/etc/sai/sai.profile}"
  SAISERVER_PORTMAP="${SAISERVER_PORTMAP:-/etc/sai/port-map.ini}"
  PTF_PORTMAP="${PTF_PORTMAP:-/etc/sai/ptf-port-map.ini}"
  SAI_TEST_DIR="${SAI_TEST_DIR:-/sai_test}"
  TEST_RESULTS_DIR="${TEST_RESULTS_DIR:-/test-results}"
  SONIC_VPP_IFMAP="${SONIC_VPP_IFMAP:-/usr/share/sonic/hwsku/sonic_vpp_ifmap.ini}"
  THRIFT_PORT="${THRIFT_PORT:-9092}"
  ...
  while [[ $# -gt 0 ]]; do
      case "$1" in
          --debug) DEBUG=1; shift ;;
          ...
  ```
* **Block 2: Preflight Verification & Cwd**
  Ensures root execution space, verifies path targets exist, and boots directory slots:
  ```bash
  preflight()
  {
      [[ "${EUID}" -eq 0 ]] || die "run_test.sh must run as root; use docker run --privileged"
      require_command ip
      require_command vpp
      ...
      require_file "$SAI_PROFILE"
      ...
      mkdir -p /run/vpp /var/log "$(dirname "$REDIS_SOCKET")" "$TEST_RESULTS_DIR" "$(dirname "$SONIC_VPP_IFMAP")"
  }
  ```
* **Block 3: Redis Daemonization**
  Spawns local standalone redis memory server and verifies connectivity:
  ```bash
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
      ...
  ```
  
  ##### Purpose of starting Redis in a "standalone" test:
  Even though this test suite is designed as a standalone framework meant to bypass and abstract away the rest of the SONiC operating system stack (No orchagent, no portsyncd, etc.), **Redis is still required because of internal sairedis architectural dependencies.** 
  The `saiserver` binary links against the standard `libsairedis.so` shared library. Internally, `libsairedis` expects a running Redis database instance to exist on the switch environment's local Unix socket directory to act as a runtime memory pool, handle notifications communications (ZeroMQ/Redis pub-sub), and write and validate active database transactions. Starting a headless Redis daemon satisfies this shared library constraint without bringing up any of the surrounding control-plane orchestration components of SONiC.

* **Block 4: Veth Interface Allocation**
  Flashes previous link layers and provisions pure veth endpoints with correct MTU:
  ```bash
  create_veths()
  {
      log "Creating ${PORT_COUNT} OEthernet/OEth peer veth pair(s)"
      delete_veths
      rm -f "$LINKS_UP_MARKER"

      for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
          local vpp_if="$(vpp_interface_name "$port_index")"
          local ptf_if="$(ptf_interface_name "$port_index")"

          ip link add "$vpp_if" type veth peer name "$ptf_if"
          ip link set dev "$vpp_if" mtu "$MTU"
          ip link set dev "$ptf_if" mtu "$MTU"
      done
  }
  ```
* **Block 5: Interface Map Creation**
  Generates the VPP hwif translations map sequentially matching lanes offset:
  ```bash
  create_sonic_vpp_ifmap()
  {
      log "Writing SONiC-to-VPP interface map to $SONIC_VPP_IFMAP"
      : > "$SONIC_VPP_IFMAP"

      for ((port_index = 0; port_index < PORT_COUNT; port_index++)); do
          echo "Ethernet$((port_index * 4)) OEthernet${port_index}" >> "$SONIC_VPP_IFMAP"
      done
  }
  ```
* **Block 6: VPP Boot & Setup**
  Launches VPP daemon, creates the AF_PACKET host interfaces in batches, and configures interrupt mode:
  ```bash
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
  ```
* **Block 7: Saiserver Spawn**
  Spawns thrift saiserver daemon and waits for interface port open:
  ```bash
  start_saiserver()
  {
      log "Starting saiserver"
      saiserver -p "$SAI_PROFILE" -f "$SAISERVER_PORTMAP" > "$SAISERVER_LOG" 2>&1 &
      SAISERVER_PID="$!"

      wait_for_saiserver_ready
  }
  ```
* **Block 8: PTF Execution & Link Activation**
  Triggers PTF testing, logs stream to console, and brings up veth pipes asynchronously upon setup completion:
  ```bash
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
  ```
* **Block 9: Cleanup on Exit**
  Gracefully stops background nodes, removes loop/veth interfaces, and exits preserving error codes:
  ```bash
  cleanup()
  {
      local status="$?"
      set +e
      ...
      log "Cleaning up runtime state"
      terminate_process saiserver "$SAISERVER_PID"
      terminate_process vpp "$VPP_PID"
      terminate_process redis "$REDIS_PID"
      delete_veths
      ...
  }
  ```

---

## 2. Working with the Test Container (Step-by-Step Guide)

### A. When does the Container need to be Rebuilt?

Rebuilding is only necessary when **code or configuration inside the sealed image changes**.

1. **You do NOT need to rebuild the container if:**
   - You only want to change the test filter or scope (you pass different arguments to `docker run` at runtime).
   - You want to change `PORT_COUNT` (you pass `-e PORT_COUNT=X` to `docker run`).

2. **You MUST rebuild the container if:**
   - You modify `run_test.sh` or the `Dockerfile`.
   - You change any of the mapping definitions (`sai.profile`, `lanemap.ini`, `port-map.ini`, `ptf-port-map.ini`).
   - You compiled new C++ code inside the `sonic-sairedis` repository and compiled new `.deb` packages (you must stage them under `debs/` and rebuild the image so it consumes the new libraries).
   - You modified Python test files in the `sai_test` subdirectory.

#### Recompiling and Staging new `.deb` Packages

When you make changes to any C++ code inside the `sonic-sairedis` repository, you must recompile the submodule and stage the new `.deb` files into the Docker context so the next build picks them up.

1. **Rebuild the deb packages:**
   From the main `sonic-buildimage` repository root, run the master make command. This builds the saiveredis `.deb` packages inside the SONiC slave build container:
   ```bash
   DOCKER_CONFIG=/nobackup/nicching/.docker-sonic-build NOTRIXIE=1 \
     make -C /nobackup/nicching/sonic-buildimage \
     target/debs/bookworm/libsairedis_1.0.0_amd64.deb
   ```
   *Note: This command will re-compile all sairedis binaries (`libsaivs.so`, `saiserver`, `libsaimetadata.so`, etc.) under `src/sonic-sairedis` and output them to the target directory.*

2. **Stage the new package `.deb`s:**
   Copy the newly compiled `.deb` packages from `sonic-buildimage`'s build output target directory into our local Docker build staging directory:
   ```bash
   # Clear the old packages to avoid conflict
   rm -f /nobackup/nicching/sonic-buildimage/src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/*.deb
   
   # Copy the freshly compiled deb packages
   cp /nobackup/nicching/sonic-buildimage/target/debs/bookworm/*.deb \
      /nobackup/nicching/sonic-buildimage/src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/
   ```
   *Note: Ensure both VPP packages and our sairedis custom debs are copied here. The Dockerfile's `require_deb` function acts as a safeguard and will verify all required libraries exist before compiling the image.*

#### The Build Command:
```bash
DOCKER_CONFIG=/nobackup/nicching/.docker-sonic-build \
docker build \
  --build-arg http_proxy=http://sonic-build-rtp.cisco.com:3128/ \
  --build-arg https_proxy=http://sonic-build-rtp.cisco.com:3128/ \
  --build-arg no_proxy=.cisco.com,.webex.com,localhost,127.0.0.1,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16 \
  -f .azure-pipelines/docker-sai-test-vpp/Dockerfile \
  -t docker-sai-test-vpp:phase1 \
  .
```

* **Parameter Breakdown:**
  * `DOCKER_CONFIG=/nobackup/...`: Directs docker to use the custom caching configuration for this build environment.
  * `--build-arg http_proxy` / `https_proxy`: Instructs `apt-get` and `pip` inside the container build process to route requests through the corporate proxy.
  * `--build-arg no_proxy`: Lists domains that bypass the build proxy (such as local loopbacks).
  * `-f .../Dockerfile`: Explicitly identifies our Dockerfile path.
  * `-t docker-sai-test-vpp:phase1`: Tags the resulting build product in Docker's registry with this name.
  * `.`: The context root (run from `src/sonic-sairedis/`). This tells Docker to copy files starting from the repository root.

---

### B. The Running Modes

#### Mode 1: Fast Attribute/Negocation Test (Lightweight, 4 ports)
This mode runs a quick port config test (such as `PortAutoNegTest`) using a small, lightweight 4-port topology. It takes less than 20 seconds to run.

```bash
DOCKER_CONFIG=/nobackup/nicching/.docker-sonic-build \
docker run --rm --privileged \
  -e PORT_COUNT=4 \
  docker-sai-test-vpp:phase1 \
  sai_port_test.PortAutoNegTest
```

#### Mode 2: Interactive Debug Mode (4 ports, bash prompt)
This mode opens a shell inside the container without running the tests, leaving VPP, Redis, and saiserver running. This allows you to run `vppctl` directly to inspect hardware tables or print logs.

```bash
DOCKER_CONFIG=/nobackup/nicching/.docker-sonic-build \
docker run --rm --privileged -it \
  -e PORT_COUNT=4 \
  -e DEBUG=1 \
  docker-sai-test-vpp:phase1 \
  bash
```
*Once inside:* Run `run_test.sh --debug` to configure the switch, then open another terminal and use `docker exec -it <container> vppctl` to inspect VPP's internal state.

#### Mode 3: Full-Scale Sanity Test (17 ports minimum)
This mode attempts to run the standard flooding test, which requires ports up to index 16 to be present (representing a minimum of 17 ports mapped to lanes matching the default `config_db.json`).

```bash
DOCKER_CONFIG=/nobackup/nicching/.docker-sonic-build \
docker run --rm --privileged \
  -e PORT_COUNT=17 \
  docker-sai-test-vpp:phase1 \
  sai_sanity_test.SaiSanityTest
```

---

## 3. Errors Fixed

During early testing and scaling, we investigated and solved two major bottlenecks and crashes:

### First Error: Python 3.11 IntEnum Stringifier Regression (saithrift / Python)
* **What appeared to be happening:** Upon attempting to turn up the first port (Port 0), the test run crashed immediately with `IndexError: list index out of range` inside saithrift's auto-generated python module sai_headers.py.
* **The Root Cause:** In sai_headers.py, the auto-generated code for `SAIEnum` defined `__str__` as:
  ```python
  def __str__(self):      
      return super().__str__().split(".")[1]
  ```
  While this was designed for older Python versions, in Python 3.11, calling `super().__str__()` on an `IntEnum` returns just the string representation of the integer value (e.g. `"2"` instead of `"sai_port_oper_status.SAI_PORT_OPER_STATUS_UP"`). Calling `.split(".")[1]` on `"2"` fails with an `IndexError`.
* **How we fixed it:** We updated [SAI/test/saithriftv2/convert_header.py](SAI/test/saithriftv2/convert_header.py) (which generates the files during compiling) to produce a modern and safe implementation of the `__str__` method for python enum classes that uses `self.name` instead:
  ```python
  class SAIEnum(enum.IntEnum):
      def __str__(self):      
          return self.name
  ```
  We rebuilt the python-saithrift deb package, verified that enums are serialized to their string representations (such as `SAI_COMMON_API_CREATE`), and built the new container with the updated libraries.

### Second Error: VPP API Socket Flooding (VPP / saiserver)
* **What appeared to be happening:** When scaled to 17 or 32 ports, `saiserver` would freeze inside `create_hostif` or `create_switch` and fail to return any values.
* **The Root Cause:** Saiserver communicates with VPP over a single-threaded synchronous Unix socket. The `WR(ret)` macro inside [vslib/vpp/vppxlate/SaiVppXlate.c](vslib/vpp/vppxlate/SaiVppXlate.c) writes VPP API commands and then waits up to 1 second for the reply. If background, unsolicited interface events (due to 17 or 32 veth interfaces or LCP TAP creations going UP) overwhelm the socket, the wait loop spends its entire 1-second timeout parsing background events instead of the command’s reply. This causes all synchronous setup commands to fail with a `-99` timeout error, freezing the client.
* **The Timeout Mechanism & Blocking Source:**
  - **What had the 1-second timeout:** The synchronous wait macro `WR(ret)` compiled inside [libsaivs.so](target/debs/bookworm/libsaivs_1.0.0_amd64.deb). It initializes a relative deadline: `f64 timeout = vat_time_now (vam) + 1.0;` and loops until that 1.0-second window is crossed.
  - **What was blocking it:** VPP's unsolicited `sw_interface_event` notifications. Inside the 1-second loop, the socket-reading function `vl_socket_client_read(5)` is called to retrieve arriving data. While this function has a 5-second maximum socket read block timer, it returns immediately when *any* data arrives. If VPP pushes a dense burst of transition events, `vl_socket_client_read` gets called repeatedly and is completely saturated parsing those background event messages. Because the thread is busy reading background events, `vam->result_ready` remains `0` (as these are not the command's reply), the 1.0-second deadline is breached, and the call returns a `-99` timeout error.
* **Why what we were doing before triggered this hang:**
  Originally, [src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/run_test.sh](src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/run_test.sh) brought up *all* veth interfaces UP before starting VPP (under `LINK_UP_MODE=early`), which caused the initial `create_switch` configuration to hang. To bypass this, we tried a mid-setup compromise: bringing up only 4 ports early, and then bringing up the remaining 28 ports in the background as soon as PTF reached its `"Waiting for switch to get ready,"` readiness log.
  - **Why this caused the crash:** Firing the veth bring-up script at that exact location triggered VPP's interface notifications *at the same microsecond* that the synchronous main-thread setup reached the `Create Host intfs...` phase. 
  - When the test suite called the first `sai_thrift_create_hostif()`, the backend driver executed `configure_lcp_interface()` which uses the blocking `WR()` macro. 
  - Because the background bring-up notifications were floods of data arriving on the same shared socket, `vl_socket_client_read(5)` was saturated processing those transitions. It could not capture the LCP creation reply before the 1-second `WR(ret)` timeout expired. 
  - The very first host interface creation returned a `-99` error status, causing the execution thread to freeze.
* **How we fixed it:** 
  1. Configured VPP RX host interfaces to `interrupt` mode in [src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/run_test.sh](src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/run_test.sh) to relieve CPU pressure.
  2. Implemented a **delayed link bring-up**: all veth interfaces are created and left `DOWN` during both the Switch and Host Interface creation phases, keeping the VPP API connection completely silent during setup. Only when PTF logs `"common config done"` (indicating setup has finished and saiserver is no longer executing blocking API commands) does [src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/run_test.sh](src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/run_test.sh) call `bring_up_veths()` in the background.

---

## 4. The Current Blocking Issue (The TAP Dependency Lock)

Although we resolved the event-flood hangs and successfully scaled the bootstrap setup to 17 and 32 ports, running the test suite now crashes with the following error:

```text
  File "/sai_test/config/vlan_configer.py", line 128, in create_vlan
    members = self.create_vlan_member(vlan_oid, vlan_port_idxs, vlan_tagging_mode)
  File "/sai_test/config/vlan_configer.py", line 153, in create_vlan_member
    vlan_member = sai_thrift_create_vlan_member(
        self.client,
        vlan_id=vlan_oid,
        bridge_port_id=self.test_obj.dut.port_obj_list[port_index].bridge_port_oid,
        vlan_tagging_mode=vlan_tagging_mode)
  ...
struct.error: required argument is not an integer
```

### The Architectural Circular Dependency (Visual Diagram)

The failure is caused by an **architectural circular dependency** between OCP's standard hardware-centric test execution order and VPP’s software-CP interface mapping model.

```
       SAI TEST SUITE EXPECTATION                          VPP SAI ACTUAL LIFECYCLE
       (Written for hardware ASICs)                        (Software implementation)
 ┌──────────────────────────────────────┐            ┌────────────────────────────────────┐
 │  1. Set Port ADMIN_STATE = True      │            │  1. Create Port OID                │
 └──────────────────┬───────────────────┘            └─────────────────┬──────────────────┘
                    │                                                  │
                    ▼                                                  ▼
 ┌──────────────────────────────────────┐            ┌────────────────────────────────────┐
 │  2. Expect Port OPER_STATUS == UP    │ ◄──DEAD─── │  2. Create Host Interface (TAP)    │
 └──────────────────┬───────────────────┘    LOCK    │     (Needs Port OID as Parent)     │
                    │                                └─────────────────┬──────────────────┘
                    ▼                                                  │
 ┌──────────────────────────────────────┐                              ▼
 │  3. Allocate 1Q Bridge Ports         │            ┌────────────────────────────────────┐
 │     (Needs Port to be UP/Active)     │            │  3. LCP maps Port ID to VPP interface│
 └──────────────────┬───────────────────┘            │     (Link can now transit to UP)   │
                    │                                └────────────────────────────────────┘
                    ▼
 ┌──────────────────────────────────────┐
 │  4. Setup Vlans / Vlan Members       │
 │     (Crashes on null BridgePort OID) │
 └──────────────────────────────────────┘
```

### Detailed Breakdown of the Disconnect

This issue is caused by a **fundamental design disconnect and circular dependency between the standard hardware SAI test suite (`sai_test`) and VPP SAI's software tap-binding implementation.**

- **The SAI test script assumes real hardware:** On real switch silicon, bringing a port admin-up has immediate effect. You check `oper_status` (in `port_configer.py`), get `UP`, and only *then* map L2 bridge ports and VLAN members.
- **VPP SAI is a software stack:** On VPP, the physical port (`OEthernet0`) has no connection to the logical SAI port (`Ethernet0`) until **after** a Host Interface (the Linux Tap device) has been created. The operational status cannot transition to `UP` before the TAP interface has been created and bound via LCP (`configure_lcp_interface` inside `SwitchVppHostif.cpp`).
- **Clarification: Why the "ports down" warning occurs in both link-up modes:**
  The `"Ports are down after retries"` warning is guaranteed to print during the `turn_up_ports()` setup phase regardless of which link-up mode we use. Here is the technical explanation for why:
  - Inside `port_configer.py`, the ports are turnup-polled **before** `create_host_intf()` is called. At that poll time, TAP devices do not exist yet in either link mode, so the port status is guaranteed to be reported as DOWN during the `turn_up_ports()` loop, printing the status "2" warnings.
  - However, in the **early bring-up mode**, once the execution exits the turnup loop and proceeds to `create_host_intf()`, VPP allocates the LCP TAP devices and immediately detects the UP carrier of the pre-existing veths. This transits the ports to `up` and updates the `libsaivs.so` state cache to UP in the background. Thus, when `reset_1q_bridge_ports()` runs later in the setup sequence, the ports *are* UP and bridge ports are allocated successfully (avoiding the `struct.error` crash).
  - In our **delayed bring-up mode** (used to solve the event flood), the links are held DOWN during *both* the TAP creation and the bridge-port setup phases. As a result, the ports never transition to UP within VPP before bridge-port allocation is executed, leading directly to the `struct.error` crash.
- **The Circular Dependency:**
  1. The test suite tries to poll for `oper_status=UP` during `turn_up_and_get_checked_ports()` (where it fails).
  2. Because the link state isn't UP, bridge port allocation fails/gets dropped by VPP's L2/Bridge Domain APIs.
  3. But the TAP interfaces that would *cause* the link state to change to UP are only created *after* the turnup loop finishes!

This is an **architectural circular dependency** that has nothing to do with Docker or helper scripts. It represents a real implementation gap in how `libsaivs.so` translates this hardware-centric test sequence into the VPP hostif/TAP lifecycle.

The `port_configer` does not create a bridge port for ports that failed the lookup, so `bridge_port_oid` remains `None`. When the test reaches `sai_thrift_create_vlan_member()`, it tries to pack that `None` as a 64-bit integer, and the Thrift binary protocol serializer crashes with `struct.error: required argument is not an integer`.

---

### Why does this not fail in Production?

In production VPP-SONiC, we **pre-create the virtual interfaces and configurations** before Syncd or Orchagent start:

1. An external script (`start_sonic_vpp.sh`) pre-creates your veth pair links and places them in the container.
2. The VPP platform scripts pre-compile VPP host interfaces and pre-write `sonic_vpp_ifmap.ini` to disk *before* Syncd boots.
3. This pre-existing layout allows the database-load sequence to create the switch ports, bind the host interfaces, and establish link states in the correct order.

Our standalone mock-test environment has **no production orchestration layer (no orchagent or portsyncd)**. Saiserver starts with a completely blank database, forcing us to use this hardware-centric sequential setup loop that exposes VPP SAI's state-engine limitations under direct Thrift testing.

### How to solve this?

Because we cannot modify the OCP standard `sai_test` test suite block sequence (we must reuse it as-is), we can approach resolving this via several possible architectural paths:

1. **VPP Port State Engine Cache Override**: Patch the C++ `libsaivs.so` driver state machine to temporarily report `SAI_PORT_ATTR_OPER_STATUS` as `UP` when its administrator state is successfully set to True, even if the corresponding LCP TAP interface hasn’t been mapped yet. This overrides the state cached in the standalone helper while keeping API communication silent.
2. **Platform Mock Layer Calibration**: Coordinate with the VPP SAI code owners to patch or add a mock-override setting to the `libsaivs.so` layer. This setting would allow virtual port interfaces to return an `oper_status` state reflecting their `admin_status` during strict bootstrap validation testing.
3. **Helper Configuration Pre-Caching**: Explore potential database pre-injection layers within the thrift shim (`saiserver`) to inject simulated operational statuses into the switch state cache before the hardware-centric poll routine is initiated.
