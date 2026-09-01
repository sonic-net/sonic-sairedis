#!/bin/bash
#
# Canonical build for sonic-sairedis.
#
# Single source of truth for "how to build this repo", used by BOTH CI
# (.azure-pipelines/build-template.yml) and local dev (build-env/compose.yaml).
# Must NOT depend on CI-only environment variables (only the optional ASAN flag).
#
# The build environment (apt/pip deps + upstream libnl/libyang/vpp/swss-common
# artifacts) is set up beforehand by `buildenv_setup` (see build-env/README.md);
# this script only runs the actual package build.
#
#   ASAN=true ./build-env/build.sh    # build with AddressSanitizer instead of gcov
set -ex

# Run from the repo root regardless of where we were invoked.
cd "$(dirname "$0")/.."

extraflags='--enable-code-coverage'
case "${ASAN:-false}" in
  [Tt]rue|1|yes) extraflags='--enable-asan' ;;
esac

rm -f ../*.deb || true
./autogen.sh
DEB_BUILD_OPTIONS=nocheck DEB_CONFIGURE_EXTRA_FLAGS="$extraflags" \
  dpkg-buildpackage -us -uc -b -Psyncd,vs,nopython2 -j"$(nproc)"

# Collect the built .debs at the repo root (where CI publishes from).
mv ../*.deb .
