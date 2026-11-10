#!/usr/bin/env bash
set -euo pipefail

# Local builds and CI share pinned inputs and public mirror defaults.
# Pin the shared reader and configuration to a landed registry revision.
revision=0f459395f8852b5c89e9895afeeea01b95b281ea
config_digest=622830b8adf42d229a53b60ea0956de4a4a3c119d0639479be78817994d67327
reader_digest=9c1ad2a9548608d06e49de0c9c119ae19d97ab95cca2505fc1e7e233e62e9b3d
manifest_digest=1579ad34288945a28a2d77f96f4764b8910dd3d856d688971e46e63e58119af5

if [[ ! "$revision" =~ ^[0-9a-f]{40}$ ]]; then
    printf 'Sairedis download bootstrap needs a landed registry revision.\n' >&2
    exit 1
fi
base="https://raw.githubusercontent.com/securely1g/sonic-bazel-registry/$revision"
inputs="${RUNNER_TEMP:-$PWD/artifacts}/sairedis-github-inputs"
bootstrap="$inputs/bootstrap"
evidence="$PWD/artifacts/validation"
configuration="$evidence/sairedis-github-inputs.bazelrc"
mkdir -p "$bootstrap" "$evidence"

fetch_registry_file() {
    local relative="$1" digest="$2" destination="$bootstrap/${1##*/}"
    if [[ -f "$destination" && ! -L "$destination" ]] &&
        printf '%s  %s\n' "$digest" "$destination" | sha256sum --check --strict --status; then
        return
    fi
    local temporary status
    temporary="$(mktemp "$bootstrap/.download.XXXXXX")"
    # Disable curl's local configuration. Raw commit URLs need no redirects;
    # require HTTP 200 instead of following an unexpected Location header.
    if ! status="$(curl --disable --fail --silent --show-error --retry 3 --max-time 60 \
        --proto '=https' --max-redirs 0 --write-out '%{http_code}' \
        --output "$temporary" "$base/$relative")" || [[ "$status" != 200 ]]; then
        rm -f -- "$temporary"
        printf 'Could not fetch pinned GitHub input %s (HTTP %s); no redirect followed.\n' \
            "$relative" "${status:-unavailable}" >&2
        return 1
    fi
    if ! printf '%s  %s\n' "$digest" "$temporary" | sha256sum --check --strict --status; then
        rm -f -- "$temporary"
        printf 'SHA256 verification failed for %s; refusing the downloaded file.\n' "$relative" >&2
        return 1
    fi
    mv -fT -- "$temporary" "$destination"
}

fetch_registry_file ci/source_mirrors.cfg "$config_digest"
fetch_registry_file ci/github_sources.py "$reader_digest"
fetch_registry_file ci/github_sources/cosmos-4.0.2.json "$manifest_digest"

if ! python3 "$bootstrap/github_sources.py" \
    --manifest "$bootstrap/cosmos-4.0.2.json" \
    --distdir "$inputs/distdir" \
    --cache-dir "$inputs/cache" \
    --receipt "$evidence/github-sources.json"; then
    printf 'Sairedis build inputs failed verification; Bazel configuration was not changed.\n' >&2
    exit 1
fi

temporary_configuration="$(mktemp "$configuration.XXXXXX")"
trap 'rm -f -- "$temporary_configuration"' EXIT
printf 'common --downloader_config="%s"\ncommon --distdir="%s"\n' \
    "$bootstrap/source_mirrors.cfg" "$inputs/distdir" > "$temporary_configuration"
mv -fT -- "$temporary_configuration" "$configuration"
printf '%s  %s\n' "$config_digest" "$base/ci/source_mirrors.cfg" > "$evidence/source-mirrors.sha256"
printf '%s  %s\n' \
    "$reader_digest" "$base/ci/github_sources.py" \
    "$manifest_digest" "$base/ci/github_sources/cosmos-4.0.2.json" > "$evidence/github-bootstrap.sha256"
