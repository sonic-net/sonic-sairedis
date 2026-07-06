# Local-dev image for building sonic-sairedis.
#
# CI does NOT use this file: CI runs inside `container: sonic-slave-*` and invokes
# buildenv_setup directly (see .azure-pipelines/build-template.yml). This exists
# so a developer can reproduce the CI build environment locally.
#
# sonic-sairedis is a CONSUMER of the shared buildenv_setup tool, which lives in
# sonic-swss-common/ci/. So (unlike sonic-swss-common) this image clones
# sonic-swss-common to obtain the tool.
#
# Layer ordering (design finding F7): clone the tool + copy ONLY the dependency
# declarations, run setup, and do NOT copy the source. build-env/compose.yaml
# mounts the working tree at /workspace at runtime, so editing source never
# invalidates the (heavy) dependency-setup layer below.

ARG DEBIAN_VERSION=bookworm
FROM sonicdev-microsoft.azurecr.io:443/sonic-slave-${DEBIAN_VERSION}:latest

ARG DEBIAN_VERSION=bookworm
# TEMPORARY (PoC): the buildenv_setup tool currently lives on sonic-swss-common's
# lawlee/ci-unify branch (not master yet). Pin the clone to that branch until it
# merges, after which this can track the matching build branch.
ARG SWSS_COMMON_REF=lawlee/ci-unify
ARG BUILD_BRANCH=master

# Clone sonic-swss-common to obtain the shared buildenv_setup tool.
RUN git clone --depth 1 --branch ${SWSS_COMMON_REF} \
      https://github.com/sonic-net/sonic-swss-common /tmp/sw-common

# Copy ONLY the dependency declarations so the heavy setup layer stays cached.
COPY build-env/ /workspace/build-env/

# Bootstrap the tool's own runtime deps, then set up the build environment.
# --org-url points at the public SONiC Azure DevOps org for artifact download
# (in CI this comes from $SYSTEM_COLLECTIONURI). Set AZURE_DEVOPS_EXT_PAT, or use
# --upstream-staged-dir, if your environment needs auth.
RUN apt-get update && apt-get install -y python3-yaml python3-requests \
 && PYTHONPATH=/tmp/sw-common/ci python3 -m buildenv_setup \
      --repo-dir /workspace \
      --scope build \
      --debian-version "${DEBIAN_VERSION}" \
      --branch "${BUILD_BRANCH}" \
      --org-url https://dev.azure.com/mssonic \
      --no-sudo

WORKDIR /workspace
