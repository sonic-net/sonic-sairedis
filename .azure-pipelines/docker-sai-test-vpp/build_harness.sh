#!/usr/bin/env bash
# Stage runtime .deb packages and build the docker-sai-test-vpp image.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAIREDIS_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILDIMAGE_ROOT="${SONIC_BUILDIMAGE:-$(cd "${SAIREDIS_ROOT}/../.." && pwd)}"
DEB_STAGING="${SCRIPT_DIR}/debs"
DOCKERFILE="${SAIREDIS_ROOT}/.azure-pipelines/docker-sai-test-vpp/Dockerfile"
BLDENV="${BLDENV:-trixie}"
IMAGE_TAG="${IMAGE_TAG:-docker-sai-test-vpp:phase1}"
STAGE_DEBS="${STAGE_DEBS:-1}"
BUILD_SAIREDIS_DEBS="${BUILD_SAIREDIS_DEBS:-0}"
AUTO_BUILD_SAIREDIS_DEBS="${AUTO_BUILD_SAIREDIS_DEBS:-1}"
VPP_DEB_DIR="${VPP_DEB_DIR:-}"

SAIREDIS_DEB_PATTERNS=(
    'libsaivs_*.deb' 'libsairedis_*.deb' 'libsaimetadata_*.deb'
    'saiserver_*.deb' 'saiserverv2_*.deb'
    'python-saithrift_*.deb' 'python-saithriftv2_*.deb'
)

usage()
{
    cat <<'EOF'
Usage: build_harness.sh [options]

Stage runtime .deb packages into docker-sai-test-vpp/debs/ (local-only,
git-ignored) and build the test image from the sonic-sairedis repo root.

Options:
  --no-stage-debs       Skip copying .debs (use what is already in debs/)
  --build-sairedis      Force-rebuild libsairedis/libsaivs (+ saithrift) in
                        sonic-buildimage before staging
  --no-auto-build       Do not invoke make when sonic-sairedis .debs are
                        missing (validate only; fail with hints)
  --bldenv <name>       Debian suite for target/debs (default: trixie)
  --image-tag <tag>     Docker image tag (default: docker-sai-test-vpp:phase1)
  --vpp-deb-dir <path>  Directory of VPP .debs (default: search buildimage
                        target/debs/<bldenv> and VPP_DEB_DIR env)
  -h, --help            Show this help

Environment:
  SONIC_BUILDIMAGE      Path to sonic-buildimage root (auto-detected if unset)
  VPP_DEB_DIR           Extra directory to copy VPP .debs from
  AUTO_BUILD_SAIREDIS_DEBS  1 (default): build sairedis .debs when missing
  http_proxy/https_proxy/no_proxy  Passed through to docker build when set

Examples:
  # Use-case 1 (local dev): stage, auto-build sairedis if needed, build image
  ./build_harness.sh

  # Force-rebuild vslib .debs, then stage + build
  ./build_harness.sh --build-sairedis

  # Rebuild image only (debs/ already populated and complete)
  ./build_harness.sh --no-stage-debs
EOF
}

log()
{
    printf '[build_harness] %s\n' "$*"
}

die()
{
    printf '[build_harness] ERROR: %s\n' "$*" >&2
    exit 1
}

deb_present()
{
    local dir="$1"
    shift
    local pattern

    [[ -d "$dir" ]] || return 1

    for pattern in "$@"; do
        shopt -s nullglob
        local matches=("${dir}"/${pattern})
        shopt -u nullglob
        if [[ ${#matches[@]} -gt 0 ]]; then
            return 0
        fi
    done

    return 1
}

copy_matching_debs()
{
    local src_dir="$1"
    shift
    local pattern

    [[ -d "$src_dir" ]] || return 0

    for pattern in "$@"; do
        shopt -s nullglob
        local matches=("${src_dir}"/${pattern})
        shopt -u nullglob
        if [[ "${#matches[@]}" -eq 0 ]]; then
            continue
        fi
        cp -v "${matches[@]}" "${DEB_STAGING}/"
    done
}

stage_vpp_debs()
{
    local src_dir="$1"
    copy_matching_debs "${src_dir}" \
        'libvppinfra_*.deb' 'vpp_*.deb' 'vpp-plugin-core_*.deb' 'vpp-plugin-dpdk_*.deb'
}

stage_sairedis_debs_from_buildimage()
{
    local deb_dir="${BUILDIMAGE_ROOT}/target/debs/${BLDENV}"

    [[ -d "${BUILDIMAGE_ROOT}" ]] || die "sonic-buildimage not found at ${BUILDIMAGE_ROOT}"
    mkdir -p "${DEB_STAGING}"

    log "Staging sonic-sairedis .debs from ${deb_dir} into ${DEB_STAGING}"
    copy_matching_debs "${deb_dir}" "${SAIREDIS_DEB_PATTERNS[@]}"
}

stage_debs_from_buildimage()
{
    local deb_dir="${BUILDIMAGE_ROOT}/target/debs/${BLDENV}"

    [[ -d "${BUILDIMAGE_ROOT}" ]] || die "sonic-buildimage not found at ${BUILDIMAGE_ROOT}"
    mkdir -p "${DEB_STAGING}"

    log "Staging runtime .debs from ${deb_dir} into ${DEB_STAGING}"

    stage_vpp_debs "${deb_dir}"
    copy_matching_debs "${deb_dir}" \
        'libsaivs_*.deb' 'libsairedis_*.deb' 'libsaimetadata_*.deb' \
        'libswsscommon_*.deb' 'libswsscommon-dev_*.deb' \
        'libyang_*.deb' 'libyang3_*.deb' 'libpcre3_*.deb' \
        'saiserver_*.deb' 'saiserverv2_*.deb' \
        'python-saithrift_*.deb' 'python-saithriftv2_*.deb'

    if [[ -n "${VPP_DEB_DIR}" ]]; then
        log "Staging VPP .debs from VPP_DEB_DIR=${VPP_DEB_DIR}"
        stage_vpp_debs "${VPP_DEB_DIR}"
    fi
}

sairedis_debs_satisfied()
{
    deb_present "${DEB_STAGING}" 'libsaivs_*.deb' \
        && deb_present "${DEB_STAGING}" 'libsairedis_*.deb' \
        && deb_present "${DEB_STAGING}" 'libsaimetadata_*.deb' \
        && deb_present "${DEB_STAGING}" 'saiserver_*.deb' 'saiserverv2_*.deb' \
        && deb_present "${DEB_STAGING}" 'python-saithrift_*.deb' 'python-saithriftv2_*.deb'
}

build_sairedis_debs()
{
    local force="${1:-0}"

    [[ -d "${BUILDIMAGE_ROOT}" ]] || die "sonic-buildimage not found at ${BUILDIMAGE_ROOT}"

    if [[ "${force}" == "1" ]]; then
        log "Force-rebuilding libsairedis/libsaivs in ${BUILDIMAGE_ROOT} (BLDENV=${BLDENV})"
        (
            cd "${BUILDIMAGE_ROOT}"
            rm -f "target/debs/${BLDENV}/libsairedis_"*.deb "target/debs/${BLDENV}/libsairedis-dev_"*.deb \
                  "target/debs/${BLDENV}/libsaivs_"*.deb "target/debs/${BLDENV}/libsaivs-dev_"*.deb
        )
    else
        log "Building libsairedis/libsaivs in ${BUILDIMAGE_ROOT} (BLDENV=${BLDENV})"
    fi

    (
        cd "${BUILDIMAGE_ROOT}"
        if [[ "${BLDENV}" == bookworm ]]; then
            NOTRIXIE=1 make "target/debs/${BLDENV}/libsairedis_1.0.0_amd64.deb"
            NOTRIXIE=1 BLDENV="${BLDENV}" make -f Makefile.work \
                "target/debs/${BLDENV}/libsaithrift-dev_0.9.4_amd64.deb"
        else
            make "target/debs/${BLDENV}/libsairedis_1.0.0_amd64.deb"
            BLDENV="${BLDENV}" make -f Makefile.work \
                "target/debs/${BLDENV}/libsaithrift-dev_0.9.4_amd64.deb"
        fi
    )
}

report_missing_debs()
{
    local -a hints=()

    if ! sairedis_debs_satisfied; then
        hints+=(
            "sonic-sairedis (libsaivs, libsairedis, libsaimetadata, saiserverv2, python-saithriftv2):"
            "  ./build_harness.sh --build-sairedis"
            "  (or omit --no-auto-build to build automatically when missing)"
        )
    fi

    if ! deb_present "${DEB_STAGING}" 'libvppinfra_*.deb'; then
        hints+=(
            "VPP infra (libvppinfra_*.deb):"
            "  Build platform-vpp in sonic-buildimage, or download sonic-net.sonic-platform-vpp"
            "  pipeline artifact vpp-trixie, then re-run with --vpp-deb-dir <path> or VPP_DEB_DIR"
        )
    fi
    if ! deb_present "${DEB_STAGING}" 'vpp_*.deb'; then
        hints+=(
            "VPP (vpp_*.deb): same sources as libvppinfra (platform-vpp build or vpp-trixie artifact)"
        )
    fi
    if ! deb_present "${DEB_STAGING}" 'vpp-plugin-core_*.deb'; then
        hints+=(
            "VPP core plugin (vpp-plugin-core_*.deb): same sources as libvppinfra"
        )
    fi
    if ! deb_present "${DEB_STAGING}" 'vpp-plugin-dpdk_*.deb'; then
        hints+=(
            "VPP DPDK plugin (vpp-plugin-dpdk_*.deb): same sources as libvppinfra"
        )
    fi
    if ! deb_present "${DEB_STAGING}" 'libswsscommon_*.deb'; then
        hints+=(
            "SONiC SWSS common (libswsscommon_*.deb):"
            "  From sonic-buildimage: make target/debs/${BLDENV}/libswsscommon_1.0.0_amd64.deb"
            "  Or download Azure.sonic-swss-common pipeline artifact, then re-run ./build_harness.sh"
        )
    fi
    if ! deb_present "${DEB_STAGING}" 'libyang_*.deb' 'libyang3_*.deb'; then
        hints+=(
            "YANG runtime (libyang_*.deb or libyang3_*.deb):"
            "  From sonic-buildimage: make target/debs/${BLDENV}/libyang3_3.12.2-1_amd64.deb"
            "  Or download sonic-buildimage.common_libs / VS build artifact, then re-run ./build_harness.sh"
        )
    fi

    if [[ ${#hints[@]} -eq 0 ]]; then
        return 0
    fi

    printf '[build_harness] ERROR: Missing required .debs in %s\n' "${DEB_STAGING}" >&2
    local line
    for line in "${hints[@]}"; do
        printf '  %s\n' "$line" >&2
    done
    return 1
}

ensure_staged_debs()
{
    mkdir -p "${DEB_STAGING}"

    if ! sairedis_debs_satisfied && [[ "${AUTO_BUILD_SAIREDIS_DEBS}" == "1" ]] \
            && [[ "${BUILD_SAIREDIS_DEBS}" != "1" ]]; then
        log "sonic-sairedis .debs missing; auto-building in sonic-buildimage"
        build_sairedis_debs 0
        stage_sairedis_debs_from_buildimage
    fi

    report_missing_debs || die "cannot build image until required .debs are in ${DEB_STAGING}"
}

docker_build_args()
{
    local -a args=(docker build --no-cache -f "${DOCKERFILE}" -t "${IMAGE_TAG}" .)
    local var

    for var in http_proxy https_proxy HTTP_PROXY HTTPS_PROXY no_proxy NO_PROXY; do
        if [[ -n "${!var:-}" ]]; then
            args+=(--build-arg "${var}=${!var}")
        fi
    done

    printf '%s\0' "${args[@]}"
}

main()
{
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --no-stage-debs) STAGE_DEBS=0 ;;
            --build-sairedis) BUILD_SAIREDIS_DEBS=1 ;;
            --no-auto-build) AUTO_BUILD_SAIREDIS_DEBS=0 ;;
            --bldenv) shift; BLDENV="${1:?--bldenv requires a value}" ;;
            --image-tag) shift; IMAGE_TAG="${1:?--image-tag requires a value}" ;;
            --vpp-deb-dir) shift; VPP_DEB_DIR="${1:?--vpp-deb-dir requires a value}" ;;
            -h|--help) usage; exit 0 ;;
            *) die "unknown option: $1 (try --help)" ;;
        esac
        shift
    done

    if [[ "${BUILD_SAIREDIS_DEBS}" == "1" ]]; then
        build_sairedis_debs 1
        stage_sairedis_debs_from_buildimage
    fi

    if [[ "${STAGE_DEBS}" == "1" ]]; then
        stage_debs_from_buildimage
    fi

    ensure_staged_debs

    log "Building image ${IMAGE_TAG} from ${SAIREDIS_ROOT}"
    (
        cd "${SAIREDIS_ROOT}"
        readarray -d '' -t docker_args < <(docker_build_args)
        "${docker_args[@]}"
    )
    log "Done: ${IMAGE_TAG}"
}

main "$@"
