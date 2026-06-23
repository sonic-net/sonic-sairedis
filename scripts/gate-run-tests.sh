#!/usr/bin/env bash
# Run sonic-sairedis build, unit tests, and coverage inside sonic-slave (gate CI).
set -euo pipefail

DEBS_DIR="${DEBS_DIR:-/debs}"
REPO_ROOT="${REPO_ROOT:-/build/sonic-sairedis}"
SLAVE_FLAVOR="${SLAVE_FLAVOR:-bookworm}"

deb_is_trixie() {
  local deb="$1"
  local depends
  depends=$(dpkg-deb -f "$deb" Depends 2>/dev/null || echo "")
  echo "$depends" | grep -qE 'libpython3\.13|libc6 \(>= 2\.3[89]|libstdc\+\+6 \(>= 1[4-9]'
}

deb_is_bookworm_compatible() {
  local deb="$1"
  deb_is_trixie "$deb" && return 1
  return 0
}

select_deb_glob() {
  local pattern="$1"
  local matches=()
  mapfile -t matches < <(compgen -G "$pattern" || true)
  if [ "${#matches[@]}" -eq 0 ]; then
    return 1
  fi
  if [ "$SLAVE_FLAVOR" = "trixie" ]; then
    printf '%s\n' "${matches[@]}" | sort -V | tail -1
    return 0
  fi
  local deb compatible=()
  for deb in "${matches[@]}"; do
    if deb_is_bookworm_compatible "$deb"; then
      compatible+=("$deb")
    fi
  done
  if [ "${#compatible[@]}" -gt 0 ]; then
    printf '%s\n' "${compatible[@]}" | sort -V | tail -1
    return 0
  fi
  echo "::error::no bookworm-compatible .deb for pattern: ${pattern}" >&2
  return 1
}

install_deb_glob() {
  local pattern="$1"
  local deb
  deb=$(select_deb_glob "$pattern") || {
    echo "::error::no .deb match for pattern: ${pattern}"
    return 1
  }
  echo "Installing $deb"
  sudo dpkg -i "./$deb" || { echo "::error::dpkg install failed: $deb"; return 1; }
}

install_deb_glob_optional() {
  local pattern="$1"
  local deb
  deb=$(select_deb_glob "$pattern" 2>/dev/null) || {
    echo "Optional deb not found: ${pattern}"
    return 0
  }
  echo "Installing optional $deb"
  sudo dpkg -i "./$deb" || echo "::warning::optional dpkg install failed: $deb"
}

echo "=== Installing build dependencies ==="
sudo apt-get update -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
  libdbus-glib-1-dev \
  libpcsclite-dev \
  docbook-to-man \
  docbook-utils \
  aspell-en \
  libhiredis-dev \
  swig4.0 \
  libzmq3-dev \
  autoconf-archive \
  redis-server \
  rsyslog \
  gcovr \
  lcov

echo "=== Configuring redis ==="
sudo sed -ri 's/^# unixsocket/unixsocket/' /etc/redis/redis.conf
sudo sed -ri 's/^unixsocketperm .../unixsocketperm 777/' /etc/redis/redis.conf
sudo sed -ri 's/redis-server.sock/redis.sock/' /etc/redis/redis.conf
sudo service redis-server start
sudo mkdir -m 755 -p /var/run/sswsyncd

echo "=== Starting rsyslog ==="
sudo rsyslogd || true

cd "$DEBS_DIR"
echo "=== Installing SONiC debs ==="
shopt -s nullglob
for pattern in \
  libnl-3-200_*.deb libnl-genl-3-200_*.deb \
  libnl-route-3-200_*.deb libnl-nf-3-200_*.deb libnl-cli-3-200_*.deb \
  libnl-3-dev_*.deb libnl-genl-3-dev_*.deb \
  libnl-route-3-dev_*.deb libnl-nf-3-dev_*.deb libnl-cli-3-dev_*.deb \
  libyang_1.0.73*.deb libyang-cpp_1.0.73*.deb libyang-dev_1.0.73*.deb \
  libswsscommon_1.0.0*.deb libswsscommon-dev_1.0.0*.deb; do
  install_deb_glob "$pattern"
done
install_deb_glob_optional 'libhiredis*.deb'
sudo DEBIAN_FRONTEND=noninteractive apt-get -f install -y -qq

cd "$REPO_ROOT"
echo "=== Building sonic-sairedis with coverage ==="
./autogen.sh
./configure --with-sai=vs --enable-code-coverage --disable-python2
make -j"$(nproc)"

echo "=== Preparing unit tests ==="
if [ -f syncd/.libs/syncd_tests ]; then
  sudo setcap "cap_sys_time=eip" syncd/.libs/syncd_tests
fi
if [ -f unittest/syncd/.libs/tests ]; then
  sudo setcap "cap_dac_override,cap_ipc_lock,cap_ipc_owner,cap_sys_time=eip" unittest/syncd/.libs/tests
fi
if [ -f azsyslog.conf ]; then
  sudo cp azsyslog.conf /etc/rsyslog.conf
  sudo pkill -F /run/rsyslogd.pid 2>/dev/null || true
  sleep 2
  sudo rsyslogd
fi

echo "=== Running make check ==="
make check

echo "=== Generating coverage reports ==="
find SAI/meta -name "*.gc*" -delete 2>/dev/null || true
gcov_dirs=$(find . -path "*.libs*gcda" -printf '%h\n' 2>/dev/null | sort -u | sed 's|^./||' || true)
for dir in ${gcov_dirs}; do
  source_dir=$(dirname "$dir")
  output_file=$(echo "coverage-${source_dir}.json" | tr '/' '-')
  gcovr --exclude-unreachable-branches --json-pretty -o "$output_file" --object-directory "$dir" "$source_dir"
done
gcovr -r ./ \
  -e ".*/SAI/.*" -e ".+/json.hpp" -e "swss/.+" \
  -e ".*/.libs/.*" -e ".*/debian/.*" -e "vslib/vpp/.*" \
  --exclude-unreachable-branches --json-pretty -o coverage-all.json
gcovr -a "coverage-*.json" -x --xml-pretty -o coverage.xml

# Bookworm gcovr has no --lcov; use lcov + genhtml for the coverage dashboard.
lcov --capture --directory . --output-file coverage.raw.info --rc lcov_branch_coverage=1
lcov --remove coverage.raw.info \
  '/usr/*' \
  '*/SAI/*' \
  '*/debian/*' \
  '*/.libs/*' \
  'vslib/vpp/*' \
  --output-file coverage.info
rm -f coverage.raw.info
if [ -f /etc/lcovrc ]; then
  sudo sed -i 's/^#\s*genhtml_function_coverage\s*=\s*0/genhtml_function_coverage = 1/' /etc/lcovrc
  grep -q '^genhtml_function_coverage' /etc/lcovrc \
    || echo 'genhtml_function_coverage = 1' | sudo tee -a /etc/lcovrc >/dev/null
fi
genhtml coverage.info --output-directory html --legend --function-coverage

test -f coverage.xml || { echo "::error::coverage.xml missing"; exit 1; }
test -f coverage.info || { echo "::error::coverage.info missing"; exit 1; }
test -f html/index.html || { echo "::error::html/index.html missing"; exit 1; }
echo "=== build + unit tests + coverage OK ==="
