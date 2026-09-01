# Local-dev image for building sonic-sairedis.
#
# CI does NOT use this file: CI runs inside `container: sonic-slave-*` and invokes
# buildenv_setup directly (see .azure-pipelines/build-template.yml). This exists
# so a developer can reproduce the CI build environment locally.
#
# sonic-sairedis consumes the shared buildenv_setup tool from
# sonic-swss-common/ci/. The source tree is mounted at /workspace at runtime, so
# editing code does not invalidate this heavy dependency-setup layer.

ARG DEBIAN_VERSION=bookworm
FROM sonicdev-microsoft.azurecr.io:443/sonic-slave-${DEBIAN_VERSION}:latest

ARG DEBIAN_VERSION=bookworm
ARG SWSS_COMMON_REF=master
ARG BUILD_BRANCH=master

# Clone sonic-swss-common to obtain the branch-versioned shared tool.
RUN git clone --depth 1 --branch "${SWSS_COMMON_REF}" \
      https://github.com/sonic-net/sonic-swss-common /tmp/sw-common

# Copy ONLY the dependency declarations so ordinary source edits stay cached.
COPY build-env/ /workspace/build-env/

# The canonical Redis hook in the swss-common cascade invokes sudo, so install it
# explicitly even though this Docker build runs setup as root with --no-sudo.
RUN apt-get update && apt-get install -y python3-yaml python3-requests sudo \
 && PYTHONPATH=/tmp/sw-common/ci python3 -m buildenv_setup \
      --repo-dir /workspace \
      --scope build \
      --debian-version "${DEBIAN_VERSION}" \
      --branch "${BUILD_BRANCH}" \
      --org-url https://dev.azure.com/mssonic \
      --no-sudo

WORKDIR /workspace
