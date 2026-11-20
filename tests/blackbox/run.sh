#!/bin/bash
#
# Runs the syncd black-box benchmark against this tree, in throwaway
# containers of the sairedis build environment (build-env/).
#
#   tests/blackbox/run.sh                                   # every benchmark
#   tests/blackbox/run.sh bench/test_routes.py -k 1000      # some of them
#   tests/blackbox/run.sh --bench-comm zmq_sync --bench-label zmq
#   tests/blackbox/run.sh --bench-perf syncd,redis --bench-rounds 1   # with CPU profiles
#
# Arguments go to pytest. The first container builds, as the caller: the tree
# if syncd is not built yet (optimized, no coverage), then the mock SAI and
# the load client. The second runs the benchmark as root, since redis and
# /dev/log take their SONiC paths. Reports go to tests/blackbox/report, logs
# of each benchmark to tests/blackbox/work.
#
#   SYNCD_BB_IMAGE    build environment image (default: build-env-build:latest,
#                     built from build-env/ when missing)
#   SYNCD_BB_PERF_IMAGE  image of profiling runs (default: syncd-bench-perf:latest,
#                     built from SYNCD_BB_IMAGE with perf when missing)
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
PERF=
while [ $# -gt 0 ]; do
    case "$1" in
        --bench-*=*) ARGS+=("$1") ;;
        --bench-*) ARGS+=("$1=$2"); shift ;;
        *) ARGS+=("$1") ;;
    esac
    case "${ARGS[-1]}" in
        --bench-perf=?*) PERF=1 ;;
    esac
    shift
done

# Profiles need perf, CAP_SYS_ADMIN for it, and CAP_IPC_LOCK for its buffers,
# which otherwise count against the locked memory of the host's root: an image
# with perf (and FlameGraph, when it can be fetched) derived from the build
# environment's.
RUN_IMAGE=$IMAGE
DOCKER_OPTS=()
if [ -n "$PERF" ]; then
    RUN_IMAGE=${SYNCD_BB_PERF_IMAGE:-syncd-bench-perf:latest}
    if ! docker image inspect "$RUN_IMAGE" > /dev/null 2>&1; then
        docker build -t "$RUN_IMAGE" - << EOF
FROM $IMAGE
RUN apt-get update && apt-get install -y --no-install-recommends linux-perf && rm -rf /var/lib/apt/lists/* \
 && (git clone --depth 1 https://github.com/brendangregg/FlameGraph /opt/FlameGraph || true)
EOF
    fi
    DOCKER_OPTS=(--cap-add SYS_ADMIN --cap-add IPC_LOCK --security-opt seccomp=unconfined)
fi

docker run --rm --init "${DOCKER_OPTS[@]}" -e SYNCD_BB_ENABLE=1 -e PYTHONDONTWRITEBYTECODE=1 -v "$TOP:/workspace" \
    -w /workspace/tests/blackbox "$RUN_IMAGE" \
    bash -c 'python3 -m pytest --bench-workdir=/workspace/tests/blackbox/work "$@"; rc=$?
             chown -R '"$OWNER"' report work 2> /dev/null; exit $rc' bash "${ARGS[@]}"
