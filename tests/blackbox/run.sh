#!/bin/bash
#
# Runs the syncd black-box benchmark against this tree, in throwaway
# containers of the sairedis build environment (build-env/).
#
#   tests/blackbox/run.sh                                   # every benchmark
#   tests/blackbox/run.sh bench/test_routes.py -k 1000      # some of them
#   tests/blackbox/run.sh --bench-comm zmq_sync --bench-label zmq
#
# Arguments go to pytest. The first container builds, as the caller: the tree
# if syncd is not built yet (optimized, no coverage), then the mock SAI and
# the load client. The second runs the benchmark as root, since redis and
# /dev/log take their SONiC paths. Reports go to tests/blackbox/report, logs
# of each benchmark to tests/blackbox/work.
#
#   SYNCD_BB_IMAGE    build environment image (default: build-env-build:latest,
#                     built from build-env/ when missing)
#   DEBIAN_VERSION    Debian of that image when it is built (default: bookworm)

set -e

HERE=$(cd "$(dirname "$0")" && pwd)
TOP=$(cd "$HERE/../.." && pwd)
IMAGE=${SYNCD_BB_IMAGE:-build-env-build:latest}

if ! docker image inspect "$IMAGE" > /dev/null 2>&1; then
    # build-env/ builds the default image only
    if [ -n "$SYNCD_BB_IMAGE" ]; then
        echo "image $IMAGE not found: build it, or unset SYNCD_BB_IMAGE to build build-env-build:latest" >&2
        exit 1
    fi
    (cd "$TOP/build-env" && DEBIAN_VERSION=${DEBIAN_VERSION:-bookworm} docker compose build build)
fi

if grep -qE "^CODE_COVERAGE_ENABLED = yes|^CFLAGS_ASAN = .*sanitize" "$TOP/syncd/Makefile" 2> /dev/null; then
    echo "WARNING: this tree is built with coverage or ASAN, syncd's numbers will not be those of a release build" >&2
fi

docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$TOP:/workspace" -w /workspace "$IMAGE" bash -ec '
    if [ ! -x syncd/.libs/syncd ]; then
        ./autogen.sh
        ./configure --with-sai=vs --disable-static --disable-python2
        make -j"$(nproc)"
    fi
    make -C tests/blackbox -j"$(nproc)"
'

OWNER="$(id -u):$(id -g)"

# Give pytest the benchmark's options as --bench-x=value: before it loads
# conftest.py, which defines them, pytest takes a separate value that is a path
# outside this directory for a test path, and then misses conftest.py.
ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --bench-*=*) ARGS+=("$1") ;;
        --bench-*) ARGS+=("$1=$2"); shift ;;
        *) ARGS+=("$1") ;;
    esac
    shift
done

docker run --rm --init -e SYNCD_BB_ENABLE=1 -e PYTHONDONTWRITEBYTECODE=1 -v "$TOP:/workspace" -w /workspace/tests/blackbox "$IMAGE" \
    bash -c 'python3 -m pytest --bench-workdir=/workspace/tests/blackbox/work "$@"; rc=$?
             chown -R '"$OWNER"' report work 2> /dev/null; exit $rc' bash "${ARGS[@]}"
