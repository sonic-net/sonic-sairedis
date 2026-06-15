# VPP SAI UT Debugging and Recompile Process

Date: June 12, 2026

This document details the exact procedures for recompiling the upstream `sonic-sairedis` libraries, staging them into the virtual switch Unit Test (UT) build context, running the debug container harness, and extracting execution logs/API traces for development review.

---

## 1. Recompiling Upstream Libraries (`sonic-sairedis`)

To prevent GNU Make from reporting targets as "up to date" when files are edited inside submodules, bypass the slower platform check sequences by deleting the `.deb` objects from the output directory directly.

### Step 1: Remove existing targets from the host build cache
```bash
cd /nobackup/nicching/sonic-buildimage

# Remove previous compiled .debs to force incremental re-generation
rm -f target/debs/bookworm/libsairedis_*.deb \
      target/debs/bookworm/libsairedis-dev_*.deb \
      target/debs/bookworm/libsaivs_*.deb \
      target/debs/bookworm/libsaivs-dev_*.deb \
      target/debs/bookworm/saiserver_*.deb \
      target/debs/bookworm/libsaithrift-dev_*.deb
```

### Step 2: Trigger package recompilation
Run the build inside the slave Bookworm container:
```bash
# Build libsairedis/libsaivs
DOCKER_CONFIG=/nobackup/nicching/.docker-sonic-build NOTRIXIE=1 \
    make target/debs/bookworm/libsairedis_1.0.0_amd64.deb

# Build saiserver/saithrift
DOCKER_CONFIG=/nobackup/nicching/.docker-sonic-build NOTRIXIE=1 BLDENV=bookworm \
    make -f Makefile.work target/debs/bookworm/libsaithrift-dev_0.9.4_amd64.deb
```

### Step 3: Stage new packages to the Unit Test context
Once compilation completes, wipe old debs from the UT context and stage the fresh binary packages:
```bash
# Clear old staged files
rm -f src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/*saiserver* \
      src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/*saithrift* \
      src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/*sairedis* \
      src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/*saivs*

# Copy newly built packages
cp target/debs/bookworm/libsaivs_*.deb \
   target/debs/bookworm/libsaivs-dev_*.deb \
   target/debs/bookworm/libsairedis_*.deb \
   target/debs/bookworm/libsairedis-dev_*.deb \
   target/debs/bookworm/saiserver_*.deb \
   target/debs/bookworm/libsaithrift-dev_*.deb \
   target/debs/bookworm/python-saithrift_*.deb \
   src/sonic-sairedis/.azure-pipelines/docker-sai-test-vpp/debs/
```

### Step 4: Rebuild the test Docker image
```bash
cd /nobackup/nicching/sonic-buildimage/src/sonic-sairedis

DOCKER_CONFIG=/nobackup/nicching/.docker-sonic-build docker build \
    --network=host \
    --build-arg http_proxy=http://sonic-build-rtp.cisco.com:3128/ \
    --build-arg https_proxy=http://sonic-build-rtp.cisco.com:3128/ \
    --build-arg no_proxy=.cisco.com,.webex.com,localhost,127.0.0.1,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16 \
    --build-arg HTTP_PROXY=http://sonic-build-rtp.cisco.com:3128/ \
    --build-arg HTTPS_PROXY=http://sonic-build-rtp.cisco.com:3128/ \
    --build-arg NO_PROXY=.cisco.com,.webex.com,localhost,127.0.0.1,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16 \
    -f .azure-pipelines/docker-sai-test-vpp/Dockerfile \
    -t docker-sai-test-vpp:phase1 .
```

---

## 2. Spinning Up a Debug Test Container

To run a test and leave VPP, Redis, and Saiserver alive for interactive shell inspection (such as running `vppctl`), run the container with the `--debug` parameter and specify a custom container name:

```bash
cd /nobackup/nicching/sonic-buildimage

# Remove previous instances
docker rm -f officesai-debug 2>/dev/null

# Start the debug container at 4 ports (safe-scale to prevent socket event storms)
DOCKER_CONFIG=/nobackup/nicching/.docker-sonic-build docker run \
    --name officesai-debug \
    --privileged \
    -e PORT_COUNT=4 \
    docker-sai-test-vpp:phase1 \
    --debug sai_sanity_test.SaiSanityTest
```

---

## 3. Pulling Saiserver / High-Level SAI Logs

The `saiserver` log captures high-level SAI RPC API events (including `create_switch`, `create_ports`, `stub_create_hostif` entrances, configuration parameters, and return statuses).

Once the run completes or is paused in debug mode, copy the saiserver log directly off the container:

```bash
# Copy /var/log/saiserver.log off the target debug container
docker cp officesai-debug:/var/log/saiserver.log ./saiserver_latest.log
```

---

## 4. Pulling VPP Binary API Logs / Traces

VPP binary API traces record low-level socket messages exchanged between Saiserver/VPP-SAI driver and VPP (such as `lcp_itf_pair_add_del`, `sw_interface_set_flags`, etc.).

During test cleanups, the test runner automatically dumps a decoded human-readable trace into `/var/log/vpp-api-trace.txt`. Copy it to the host with:

```bash
# Retrieve the decoded API trace file
docker cp officesai-debug:/var/log/vpp-api-trace.txt ./vpp_api_trace.txt

# (Optional) Retrieve the raw binary API trace output
docker cp officesai-debug:/tmp/vpp-sai-api-trace.api ./vpp-sai-api-trace.api
```
