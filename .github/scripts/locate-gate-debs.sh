#!/usr/bin/env bash
# Locate bookworm- or trixie-compatible SONiC debs for the sonic-sairedis gate.
set -euo pipefail

has_sairedis_debs() {
  local dir="$1"
  ls "$dir"/libswsscommon_1.0.0*.deb 2>/dev/null | head -1 | grep -q . \
    && ls "$dir"/libswsscommon-dev_1.0.0*.deb 2>/dev/null | head -1 | grep -q . \
    && ls "$dir"/libyang_1.0.73*.deb 2>/dev/null | head -1 | grep -q .
}

deb_is_trixie() {
  local deb="$1"
  local depends
  depends=$(dpkg-deb -f "$deb" Depends 2>/dev/null || echo "")
  echo "$depends" | grep -qE 'libpython3\.13|libc6 \(>= 2\.3[89]|libstdc\+\+6 \(>= 1[4-9]'
}

gate_slave_flavor() {
  local dir="$1"
  local swss dev
  swss=$(ls "$dir"/libswsscommon_1.0.0*.deb 2>/dev/null | head -1 || true)
  dev=$(ls "$dir"/libswsscommon-dev_1.0.0*.deb 2>/dev/null | head -1 || true)
  if [ -z "$swss" ] || [ -z "$dev" ]; then
    echo "unknown"
    return
  fi
  if deb_is_trixie "$swss" || deb_is_trixie "$dev"; then
    echo "trixie"
  else
    echo "bookworm"
  fi
}

publish_debs_dir() {
  local dir="$1"
  local flavor
  flavor=$(gate_slave_flavor "$dir")
  if [ "$flavor" = "unknown" ]; then
    echo "::error::Could not determine slave flavor for debs in $dir"
    exit 1
  fi
  {
    echo "debs_dir=${dir}"
    echo "slave_flavor=${flavor}"
  } >> "${GITHUB_OUTPUT:?}"
  echo "Using SONiC debs from: $dir (sonic-slave-${flavor})"
  ls "$dir"/*.deb 2>/dev/null | head -20 || true
}

ARTIFACTS_DIR="/var/cache/sonic/artifacts"
EXTRACT_DIR="${RUNNER_TEMP}/sonic-sairedis-debs-${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}"
BOOKWORM_CACHE="/var/cache/sonic/debs/bookworm"
RUNNER_BASE=$(dirname "$(dirname "${GITHUB_WORKSPACE:?}")")

try_dir() {
  local dir="$1"
  [ -d "$dir" ] || return 1
  has_sairedis_debs "$dir" || return 1
  publish_debs_dir "$dir"
  exit 0
}

for candidate in \
  "$BOOKWORM_CACHE" \
  "$RUNNER_BASE"/*/caspian-sonic-buildimage/target/debs/bookworm \
  /home/*/org-action-runners/actions-runner-*/_work/caspian-sonic-buildimage/caspian-sonic-buildimage/target/debs/bookworm \
  "$ARTIFACTS_DIR/mellanox/gating/target/debs/bookworm" \
  "$ARTIFACTS_DIR/mellanox/prod/target/debs/bookworm" \
  "$RUNNER_BASE"/*/caspian-sonic-buildimage/target/debs/trixie \
  /home/*/org-action-runners/actions-runner-*/_work/caspian-sonic-buildimage/caspian-sonic-buildimage/target/debs/trixie \
  "$ARTIFACTS_DIR/mellanox/gating/target/debs/trixie" \
  "$ARTIFACTS_DIR/mellanox/prod/target/debs/trixie"; do
  try_dir "$candidate" || true
done

scratch="${EXTRACT_DIR}.scratch"
rm -rf "$EXTRACT_DIR" "$scratch"
mkdir -p "$EXTRACT_DIR" "$scratch"

if [ -d "$ARTIFACTS_DIR" ]; then
  for pattern in \
    "libnl-3-200_*.tgz" "libnl-genl-3-200_*.tgz" \
    "libnl-route-3-200_*.tgz" "libnl-nf-3-200_*.tgz" \
    "libnl-cli-3-200_*.tgz" \
    "libyang_1.0.73_amd64.deb-*.tgz" "libyang-cpp_1.0.73*.tgz" \
    "libyang-dev_1.0.73*.tgz" "libhiredis*.tgz" \
    "libswsscommon_1.0.0*.tgz" "libswsscommon-dev_1.0.0*.tgz"; do
    tgz=$(ls "$ARTIFACTS_DIR"/${pattern} 2>/dev/null | sort -V | tail -1 || true)
    if [ -n "$tgz" ]; then
      tar xzf "$tgz" -C "$scratch" || echo "::warning::failed to extract $tgz"
    fi
  done
fi

if [ -d "$BOOKWORM_CACHE" ]; then
  for pattern in \
    libhiredis*.deb \
    libnl-3-200_*.deb libnl-genl-3-200_*.deb \
    libnl-route-3-200_*.deb libnl-nf-3-200_*.deb libnl-cli-3-200_*.deb \
    libnl-3-dev_*.deb libnl-genl-3-dev_*.deb \
    libnl-route-3-dev_*.deb libnl-nf-3-dev_*.deb libnl-cli-3-dev_*.deb \
    libyang_1.0.73*.deb libyang-cpp_1.0.73*.deb libyang-dev_1.0.73*.deb \
    libswsscommon_1.0.0*.deb libswsscommon-dev_1.0.0*.deb; do
    for deb in "$BOOKWORM_CACHE"/${pattern}; do
      [ -f "$deb" ] || continue
      cp -f "$deb" "$scratch/"
    done
  done
fi

while IFS= read -r deb; do
  base=$(basename "$deb")
  case "$base" in
    libswsscommon_*|libswsscommon-dev_*)
      if deb_is_trixie "$deb"; then
        echo "Skipping trixie swsscommon deb (bookworm pass): $base"
        continue
      fi
      ;;
  esac
  cp -n "$deb" "$EXTRACT_DIR/" 2>/dev/null || cp "$deb" "$EXTRACT_DIR/"
done < <(find "$scratch" -name '*.deb' ! -name '*dbgsym*')

if has_sairedis_debs "$EXTRACT_DIR"; then
  publish_debs_dir "$EXTRACT_DIR"
  exit 0
fi

echo "Bookworm swsscommon debs unavailable; falling back to trixie artifact debs"
while IFS= read -r deb; do
  base=$(basename "$deb")
  case "$base" in
    libswsscommon_*|libswsscommon-dev_*)
      cp -f "$deb" "$EXTRACT_DIR/"
      ;;
  esac
done < <(find "$scratch" -name '*.deb' ! -name '*dbgsym*')

if has_sairedis_debs "$EXTRACT_DIR"; then
  publish_debs_dir "$EXTRACT_DIR"
  exit 0
fi

echo "::error::No libswsscommon/libyang debs found for sonic-sairedis gate."
exit 1
