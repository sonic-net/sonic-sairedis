# Phase 1 VPP SAI Unit Test Progress - 2026-06-03

## Purpose

This note captures the current state of the Phase 1 MVP for the VPP SAI unit-test framework. It is intentionally a step back from the bug-by-bug loop: the goal is to separate what belongs to the Docker harness implementation from what appears to be pre-existing SAI/VPP backend behavior exposed by the harness.

Phase 1 in the HLD is the foundation milestone: run a single SAI PTF test end-to-end against a real VPP dataplane in one privileged container. The specific target is `sai_sanity_test.SaiSanityTest`, using `saiserver` as the Thrift-to-SAI shim and `libsaivs.so` as the VPP SAI implementation.

Current status: the container builds, starts Redis, VPP, `saiserver`, and PTF, creates the expected veth/AF_PACKET topology, and reaches SAI test setup. It has not yet produced a clean `SaiSanityTest` pass. The current blocker is that the SAI view of front-panel port operational status remains down during the 17-port validation run.

## What Has Been Done

### Branch and Scope Setup

- Touched submodules were moved to the required branch name `sai_vpp_unittest`.
- Work has been kept to Phase 1 unless a blocker prevented the Phase 1 test from advancing.
- The HLD has been treated as the source of truth. One known deviation is documented below: the PTF peer interface name is shortened from the HLD's `OEthernetN_peer` to `OEthN_peer` because Linux interface names cannot exceed 15 characters.

Purpose in Phase 1: keep the MVP aligned with the HLD while preserving enough traceability to distinguish intentional harness decisions from fixes made only because the MVP exposed backend/runtime issues.

### Docker Harness

Added the Phase 1 Docker context under `.azure-pipelines/docker-sai-test-vpp/`:

- `Dockerfile`
- `run_test.sh`
- `sai.profile`
- `lanemap.ini`
- `port-map.ini`
- `ptf-port-map.ini`
- `config_db.json`
- `vpp-port-admin-state-bug.md`

The Dockerfile uses Debian Bookworm and installs the runtime pieces needed by the HLD architecture: VPP packages, `libsaivs`, `libsairedis`, `libsaimetadata`, `saiserver`, Python `sai_thrift`, PTF, Scapy, and Redis.

Purpose in Phase 1: produce a single self-contained image that can run VPP, `saiserver`, and PTF in one privileged container without requiring a full SONiC stack.

Why this approach: this follows the HLD's single-container MVP. It deliberately avoids orchagent, syncd, bgpd, and other SONiC services because Phase 1 is meant to exercise SAI APIs directly through Thrift against the VPP backend.

### Package Staging and Image Build

Implemented the local package artifact flow selected as option A:

- Build packages through the SONiC build system.
- Stage the resulting `.deb` files into `.azure-pipelines/docker-sai-test-vpp/debs/`.
- Keep the staged package directory ignored by git.
- Build the image from the `sonic-sairedis` repository root.

Latest successful package rebuild:

```bash
DOCKER_CONFIG=/nobackup/nicching/.docker-sonic-build NOTRIXIE=1 \
    make -C /nobackup/nicching/sonic-buildimage \
    target/debs/bookworm/libsairedis_1.0.0_amd64.deb
```

The build completed successfully and emitted the required artifacts:

- `libsairedis_1.0.0_amd64.deb`
- `libsaimetadata_1.0.0_amd64.deb`
- `libsaivs_1.0.0_amd64.deb`
- `libsaivs-dev_1.0.0_amd64.deb`

Latest image rebuild completed successfully with image id beginning `22c3ae6df5f4`. The installed image library hash was:

```text
333635d625db1376de71d16deb708236fce816b713a3618003fdca9dfedebf5f  /usr/lib/x86_64-linux-gnu/libsaivs.so.0.0.0
```

The staged and target `libsaivs_1.0.0_amd64.deb` hashes matched:

```text
09efa8795c7b610eafa8a57d422b1dd44f61e68eb9e8b0fd25cdf712662c672a
```

Purpose in Phase 1: make the image consume the exact rebuilt VPP SAI packages under test, not stale packages from a prior image layer.

Why this approach: local staging is a pragmatic Phase 1 substitute for the future CI artifact chain described in the HLD. It gives deterministic local rebuild/test loops before adding Azure pipeline stages.

### Runtime Orchestration

Implemented `run_test.sh` to perform the HLD's runtime sequence:

1. Start Redis on a Unix socket.
2. Create `PORT_COUNT` veth pairs.
3. Generate `/usr/share/sonic/hwsku/sonic_vpp_ifmap.ini` mapping SAI names to VPP host interfaces.
4. Start VPP with the required plugins enabled.
5. Create VPP AF_PACKET host interfaces.
6. Start `saiserver`.
7. Run PTF with `sai_test` and JUnit XML output enabled.
8. Preserve logs and state in `--debug` mode.

Important harness choices:

- `PORT_COUNT` defaults to 32, matching the HLD and standard T0 assumptions.
- `PORT_COUNT=4` is supported only as a quick debug mode; it is not enough for full `SaiSanityTest` T0 setup.
- PTF peer names use `OEthN_peer`, not `OEthernetN_peer`, because of the Linux interface-name length limit.
- Linux veth links are brought up only once PTF reaches its readiness/setup line. This avoids an early VPP event storm before the test stack is ready.
- VPP AF_PACKET interfaces are set to polling RX mode to reduce interrupt/event-loop pressure in the software topology.
- VPP host-interface creation is batched to reduce setup pressure and to provide clearer failure boundaries.

Purpose in Phase 1: automate the manual setup a developer would otherwise need to perform to reproduce the HLD topology and run one SAI test.

### Redis Runtime Dependency

Redis was added to the container and started by `run_test.sh`.

Purpose in Phase 1: satisfy inherited `saiserver`/`libsairedis` runtime plumbing.

Why this does not violate the HLD's goal of abstracting away SONiC: Redis here is not being used as SONiC orchestration state. There is no orchagent/syncd/full SONiC control plane in the harness. Redis is present because `saiserver` and the sairedis libraries expect Redis infrastructure to exist as part of their runtime environment.

### saithrift and PTF Support

Changes were made in the SAI and platform trees so the Phase 1 image can run the existing SAI PTF tests in the Bookworm/Python 3/VPP environment:

- `platform/vpp/rules/libsaithrift-dev.mk` now builds the VPP SAI thrift package with the VPP virtual switch library and Python 3/v2 settings.
- `SAI/debian/python-saithrift.install` installs generated Python thrift files into Python 3 dist-packages.
- `SAI/test/saithriftv2/convert_header.py` now returns enum names in a Python 3.11-compatible way.
- `SAI/test/ptf/src/ptf/dataplane.py` skips a transient AF_PACKET `recvmsg failed` runtime error instead of crashing the receive thread.

Purpose in Phase 1: reuse the existing `sai_test` PTF suite as required by the HLD, instead of writing a separate VPP-only test driver.

Ownership assessment: these are integration/build compatibility fixes around the test infrastructure. They are not changes to the Phase 1 topology itself.

### Backend Fixes Exposed by the Harness

The Phase 1 harness started exercising real VPP SAI paths that were not fully working in the standalone `saiserver` plus PTF flow. Several backend fixes were made to keep the Phase 1 validation moving:

- `vslib/vpp/SwitchVppRif.cpp`: decoupled port admin-state and port MTU handling from `is_ip_nbr_active()`.
- `vslib/vpp/SwitchVppHostif.cpp`: replayed stored port admin-state and MTU after hostif mapping exists.
- `vslib/vpp/SwitchVpp.cpp`: populated `SAI_QUEUE_ATTR_PARENT_SCHEDULER_NODE` for front-panel and CPU queues.
- `vslib/vpp/vppxlate/SaiVppXlate.c`: fixed endian handling for VPP `sw_interface_event` fields and added temporary interface-dump notices.
- `vslib/SwitchStateBaseHostif.cpp`: attempted to update cached port operational status before the optional switch notification callback lookup.

Purpose in Phase 1: these were not part of the Docker harness design. They were added because the MVP could not reach the single sanity-test pass without satisfying SAI object and state contracts expected by the existing `sai_test` suite.

Important caution: because VPP SAI is used in active deployments, each backend change should be treated as a hypothesis until validated in the standalone harness and reviewed against the deployment path. The harness may be exercising a lifecycle or callback-registration pattern different from production SONiC, so failures do not automatically mean the production path is broken.

## Blockers Faced

### 1. Linux Interface Name Length

Problem: the HLD names the PTF peer interfaces `OEthernetN_peer`, but that exceeds Linux's 15-character interface-name limit once `N` reaches two digits.

Symptoms: veth creation would fail for larger port indexes if the HLD name was used literally.

Ownership: harness/HLD mismatch with Linux constraints.

Resolution: use `OEthernetN` for the VPP-facing veth and `OEthN_peer` for the PTF-facing peer.

Validation: the 17-port run successfully created and brought up `OEthernet0..16` and `OEth0_peer..OEth16_peer`.

Status: resolved; this is a documented HLD deviation.

### 2. Local Image Build Needed Explicit Package Staging

Problem: the Phase 1 Dockerfile needs local `.deb` artifacts, but the full CI artifact chain from the HLD is not implemented yet.

Symptoms: Docker builds would be ambiguous or stale unless the exact required packages were placed in the Docker context.

Ownership: local implementation gap for Phase 1 before CI integration.

Resolution: stage required packages into `.azure-pipelines/docker-sai-test-vpp/debs/`, validate that required package patterns exist, and ignore the staged package directory in git.

Validation: latest package rebuild succeeded; image rebuild succeeded; target/staged package hashes matched; image `libsaivs.so.0.0.0` hash changed after rebuild.

Status: resolved for local Phase 1. CI artifact wiring remains future Phase 3 work.

### 3. Docker Build Network/Proxy Requirements

Problem: apt package installation inside Docker needed the build environment's proxy settings.

Symptoms: Docker build failed during apt operations without proxy build arguments.

Ownership: local build environment, not SAI/VPP logic.

Resolution: build with `http_proxy`, `https_proxy`, and `no_proxy` build arguments.

Validation: Docker image rebuilt successfully with those arguments.

Status: resolved locally; CI should encode this through standard pipeline proxy handling.

### 4. Redis Requirement in a Standalone SAI Test

Problem: `saiserver`/`libsairedis` expect Redis runtime plumbing even though the harness is not running full SONiC.

Symptoms: `saiserver` readiness and SAI runtime behavior depended on Redis availability.

Ownership: inherited sairedis runtime dependency, not SONiC orchestration logic.

Resolution: start a minimal Redis server on `/var/run/redis/redis.sock` inside `run_test.sh`.

Validation: the harness now starts Redis, reaches `saiserver` readiness, and PTF begins SAI setup.

Status: resolved.

### 5. Python 3 saithrift Import and Enum Behavior

Problem: the existing saithrift packaging/generation assumptions did not cleanly match the Python 3.11 Bookworm container path.

Symptoms: Python imports and enum stringification failed before tests could run reliably.

Ownership: test/build integration code, not VPP dataplane behavior.

Resolution: adjust Python thrift packaging to install into Python 3 dist-packages and update generated enum stringification to return `self.name`.

Validation: Dockerfile now runs `python3 -c 'import sai_thrift'`; PTF starts and reaches `SaiSanityTest` setup.

Status: resolved for Phase 1 local image.

### 6. PTF AF_PACKET Receive Loop Robustness

Problem: PTF could hit a transient `recvmsg failed` RuntimeError while reading AF_PACKET sockets.

Symptoms: the PTF receive thread could fail independently of the SAI test result.

Ownership: test framework robustness in this AF_PACKET-heavy software topology.

Resolution: catch only the transient `recvmsg failed` RuntimeError, log it at debug level, sleep briefly, and continue.

Validation: PTF now stays alive long enough to execute SAI setup and expose later SAI/VPP issues.

Status: resolved for the observed symptom.

### 7. VPP Port Admin-State and MTU Propagation

Problem: SAI port admin-state and MTU updates were not reliably applied to VPP host interfaces.

Symptoms: Linux veth links and VPP hardware links could be up, but VPP software interfaces such as `host-OEthernet0` stayed admin down. Manual `vppctl set interface state host-OEthernet0 up` worked.

Ownership: VPP SAI backend lifecycle issue exposed by the harness. The harness was creating real VPP AF_PACKET interfaces and then running existing SAI tests; the failure was in translating/staging SAI port attributes to VPP interfaces.

Resolution: remove the neighbor-mode gate from basic port admin/MTU handling and replay stored admin-state/MTU after hostif mapping is registered.

Validation: four-port debug validation showed `host-OEthernet0..3` up with MTU 9100. This is documented in `vpp-port-admin-state-bug.md`.

Status: resolved for the targeted admin-state/MTU symptom.

### 8. CPU Queue Parent Scheduler Metadata

Problem: `SaiSanityTest` reads CPU queue attributes, including `SAI_QUEUE_ATTR_PARENT_SCHEDULER_NODE`. The VPP virtual switch queues did not consistently populate that read-only attribute.

Symptoms: PTF failed in CPU queue setup with `TypeError: 'NoneType' object is not subscriptable` after reading queue attributes.

Ownership: VPP virtual switch SAI metadata contract. The backend may not model full HQoS, but it still needs to return valid queue metadata expected by the SAI tests.

Resolution: populate queue parent scheduler node metadata for front-panel queues and CPU queues, and backfill warm-up queues when missing.

Validation: the previous CPU queue `NoneType` failure disappeared. A four-port run progressed past CPU queue setup and failed later in VLAN setup for a separate reason.

Status: resolved for the observed CPU queue symptom.

### 9. Four-Port Debug Mode Is Not Enough for `SaiSanityTest`

Problem: `PORT_COUNT=4` is useful for fast infrastructure debugging but does not satisfy the standard T0 setup inside `SaiSanityTest`.

Symptoms: VLAN setup later failed because the test expects port indexes up through 16.

Ownership: test topology mismatch, not a backend bug.

Resolution: use `PORT_COUNT=17` or the default `PORT_COUNT=32` for full `SaiSanityTest` validation.

Validation: the next validation attempt used `PORT_COUNT=17` and reached the port bring-up checks for all required T0 ports.

Status: resolved as a testing-method issue.

### 10. Port Operational Status Still Reads Down

Problem: in the 17-port validation run, SAI still reports front-panel ports as down even after the admin-state/MTU fixes and image rebuild.

Symptoms: `SaiSanityTest` repeatedly prints lines like:

```text
port 0 , local index 0 id 4294967297 is not up, status: 2. Retry. Reset Admin State.
```

The latest run showed this for multiple ports. Linux veth interfaces were up and lower-up. VPP and `saiserver` were both consuming high CPU during the run. One `vppctl show interface` inspection attempt timed out, which means VPP control-plane responsiveness during the test is also suspect.

Ownership assessment: not proven yet. Current evidence makes a stale Docker image unlikely because the package rebuild, staged package hash, image rebuild, and in-image `libsaivs` hash were verified. It also does not look like a simple veth creation failure because Linux links were up. The likely area is VPP SAI operational-status synchronization: VPP interface event delivery, interface-name/index mapping, cached SAI `SAI_PORT_ATTR_OPER_STATUS`, or the standalone `saiserver` callback/lifecycle path. It may still involve a harness timing issue, so this should be debugged before adding more backend patches.

What was tried: moved `update_port_oper_status()` before the optional switch notification callback lookup in `SwitchStateBaseHostif.cpp`, rebuilt packages, rebuilt the image, and reran `PORT_COUNT=17`. The symptom remained.

Validation of attempted resolution: failed. The attempted cache-ordering patch did not clear the 17-port port-up polling loop.

Status: current active blocker.

### 11. 32-Port Scale/Hang

Problem: a prior 32-port run reached host-interface creation and then VPP plus `saiserver` consumed high CPU. `vppctl show interface` timed out and the container had to be stopped.

Symptoms: high CPU in both VPP and `saiserver`, poor VPP CLI responsiveness, and no clean test progress.

Ownership assessment: unresolved. It could be VPP event pressure, AF_PACKET setup scale, backend interface-dump behavior, or harness timing. The later 17-port oper-status blocker should be solved first so we can distinguish functional correctness from 32-port scale behavior.

What was done: added batching for VPP host-interface creation and switched AF_PACKET RX mode to polling. These changes reduce setup/event pressure but do not yet prove the 32-port path is healthy.

Validation: incomplete. The current priority is 17-port functional validation.

Status: pending.

### 12. Dirty Generated/Build Artifacts

Problem: local builds have produced generated files and package artifacts in several working trees, especially under nested SAI paths and platform Docker output directories.

Symptoms: `git status` includes intended source changes mixed with generated headers, debhelper logs, package artifacts, and Docker build output.

Ownership: workspace hygiene/build artifact management.

Resolution plan: before preparing a reviewable change set, separate intended source changes from generated build products. Do not revert user changes blindly, but do inspect and clean or document generated artifacts before any commit/PR.

Validation: pending.

Status: pending cleanup.

## Assessment: Harness vs. Backend vs. Build Image

The number of surfaced issues is concerning, but it does not by itself prove the Docker harness is fundamentally wrong or that deployed VPP SAI is broadly broken.

Evidence that the harness/image is mostly doing what Phase 1 requires:

- The image builds cleanly from the current staged artifacts.
- The rebuilt package hashes and in-image `libsaivs` hash were verified after the latest backend change.
- Redis, VPP, `saiserver`, and PTF all start in the same container.
- The expected veth pairs are created and PTF reaches the real `SaiSanityTest` setup path.
- The harness exposed targeted backend symptoms, and at least two of those symptoms were validated as resolved: VPP admin-state/MTU propagation and CPU queue metadata.

Evidence that we should slow down before adding more backend patches:

- The latest oper-status patch did not resolve the 17-port blocker.
- Active deployments may use a different lifecycle, callback registration path, or SONiC orchestration sequence than this standalone `saiserver` harness.
- Some current changes are temporary/debug-oriented, such as interface-dump notices in `SaiVppXlate.c`.
- The workspace contains generated/build artifacts that should not be conflated with the intended Phase 1 source changes.

Conclusion: the Phase 1 harness is plausible and largely HLD-aligned, but the current blocker needs a more controlled diagnosis. The next debugging step should verify the standalone event/oper-status path before any further code changes.

## Next Steps

1. Freeze feature changes and perform a harness audit.
   - Confirm all files copied into the image are the expected local versions.
   - Confirm installed package versions and hashes inside the image.
   - Confirm `saiserver` links against the intended VPP SAI libraries.
   - Confirm port maps, config DB, lane maps, and `sonic_vpp_ifmap.ini` are internally consistent.

2. Reproduce the oper-status issue with the smallest controlled test.
   - Prefer a one-port or two-port custom probe that creates the switch, hostif, sets port admin up, and reads `SAI_PORT_ATTR_OPER_STATUS` directly.
   - Avoid the full T0 setup until the basic status transition is understood.
   - Capture `saiserver.log`, `vpp.log`, VPP interface state, and SAI get/set traces in the same run.

3. Verify VPP event delivery before patching more backend code.
   - Check whether `sw_interface_event` is emitted for `host-OEthernetN`.
   - Check whether the event maps to the expected SAI port object id.
   - Check whether `send_port_oper_status_notification()` runs in this standalone harness.
   - Check whether `SAI_PORT_ATTR_OPER_STATUS` is updated even when the switch notification callback is absent.

4. Decide whether the fix belongs in the harness or backend.
   - If VPP never emits the event because of harness timing or interface state, fix `run_test.sh` sequencing.
   - If VPP emits the event but the backend loses the port mapping or skips cache update, fix the VPP SAI backend.
   - If production SONiC registers callbacks or creates hostifs in a different order, document that difference explicitly before changing shared backend behavior.

5. Rerun validation in increasing scope.
   - Run the minimal oper-status probe.
   - Run `PORT_COUNT=17` with `sai_sanity_test.SaiSanityTest`.
   - Only after 17-port functional validation passes, rerun the default 32-port case and investigate scale separately if needed.

6. Clean the workspace into a reviewable set.
   - Keep the Phase 1 Docker harness files.
   - Keep only intentional SAI/PTF/build-rule changes.
   - Remove or ignore generated package artifacts and local Docker build outputs.
   - Revisit temporary debug logging before proposing final changes.

7. Update this report as validation changes.
   - Record the root cause of the oper-status blocker once proven.
   - Record the exact passing image/package hashes when `SaiSanityTest` passes.
   - Add remaining HLD deviations, if any, instead of leaving them implicit.