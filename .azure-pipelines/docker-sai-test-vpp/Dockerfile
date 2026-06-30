FROM debian:trixie

ENV DEBIAN_FRONTEND=noninteractive \
    PIP_BREAK_SYSTEM_PACKAGES=1 \
    PYTHONUNBUFFERED=1 \
    SAI_PROFILE=/etc/sai/sai.profile \
    SAISERVER_PORTMAP=/etc/sai/port-map.ini \
    PTF_PORTMAP=/etc/sai/ptf-port-map.ini \
    SAI_TEST_DIR=/sai_test \
    TEST_RESULTS_DIR=/test-results

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Build from the sonic-sairedis repository root:
# docker build -f .azure-pipelines/docker-sai-test-vpp/Dockerfile -t docker-sai-test-vpp .
COPY .azure-pipelines/docker-sai-test-vpp /opt/docker-sai-test-vpp
COPY SAI/test/ptf /opt/ptf
COPY SAI/test/sai_test /sai_test

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        iproute2 \
        libboost-serialization1.83.0 \
        libpcap0.8 \
        libthrift-0.19.0t64 \
        libzmq5 \
        procps \
        python3 \
        python3-pip \
        python3-setuptools \
        python3-thrift \
        python3-wheel \
        redis-server \
        redis-tools \
        tcpdump && \
    rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --no-cache-dir scapy==2.5.0 unittest-xml-reporting==3.2.0 && \
    python3 -m pip install --no-cache-dir /opt/ptf

RUN set -eux; \
    deb_dir=/opt/docker-sai-test-vpp/debs; \
    require_deb() { \
        local label="$1"; \
        shift; \
        local found=0; \
        local pattern; \
        for pattern in "$@"; do \
            if compgen -G "${deb_dir}/${pattern}" >/dev/null; then \
                found=1; \
            fi; \
        done; \
        if [[ "$found" -eq 0 ]]; then \
            echo "Missing ${label}; expected one of: $*" >&2; \
            exit 1; \
        fi; \
    }; \
    require_deb "VPP infra package" "libvppinfra_*.deb"; \
    require_deb "VPP package" "vpp_*.deb"; \
    require_deb "VPP core plugin package" "vpp-plugin-core_*.deb"; \
    require_deb "VPP DPDK plugin package" "vpp-plugin-dpdk_*.deb"; \
    require_deb "SAI virtual switch package" "libsaivs_*.deb"; \
    require_deb "SAI Redis package" "libsairedis_*.deb"; \
    require_deb "SAI metadata package" "libsaimetadata_*.deb"; \
    require_deb "SONiC SWSS common package" "libswsscommon_*.deb"; \
    require_deb "YANG runtime package" "libyang_*.deb" "libyang3_*.deb"; \
    require_deb "SAI thrift server package" "saiserver_*.deb" "saiserverv2_*.deb"; \
    require_deb "SAI Python thrift package" "python-saithrift_*.deb" "python-saithriftv2_*.deb"; \
    runtime_debs=(); \
    have_saiserverv2=0; \
    have_python_saithriftv2=0; \
    compgen -G "${deb_dir}/saiserverv2_*.deb" >/dev/null && have_saiserverv2=1; \
    compgen -G "${deb_dir}/python-saithriftv2_*.deb" >/dev/null && have_python_saithriftv2=1; \
    while IFS= read -r deb_file; do \
        deb_name="$(basename "$deb_file")"; \
        case "$deb_name" in \
            *-dbg_*.deb|*-dbgsym_*.deb|*-dev_*.deb) continue ;; \
            libyang3-tools_*.deb) continue ;; \
            saiserver_*.deb) [[ "$have_saiserverv2" -eq 1 ]] && continue ;; \
            python-saithrift_*.deb) [[ "$have_python_saithriftv2" -eq 1 ]] && continue ;; \
        esac; \
        runtime_debs+=("$deb_file"); \
    done < <(find "$deb_dir" -maxdepth 1 -type f -name '*.deb' | sort); \
    if [[ "${#runtime_debs[@]}" -eq 0 ]]; then \
        echo "No runtime .deb packages found under ${deb_dir}" >&2; \
        exit 1; \
    fi; \
    apt-get update; \
    cp /usr/sbin/sysctl /usr/sbin/sysctl.real; \
    cp /usr/bin/true /usr/sbin/sysctl; \
    apt-get install -y --no-install-recommends --allow-downgrades "${runtime_debs[@]}"; \
    find /usr/lib/python3/dist-packages -maxdepth 1 -type d -name 'saithrift-*.egg' -print > /usr/lib/python3/dist-packages/saithrift.pth; \
    test -s /usr/lib/python3/dist-packages/saithrift.pth; \
    python3 -c 'import sai_thrift'; \
    mv /usr/sbin/sysctl.real /usr/sbin/sysctl; \
    build_dev_debs=(); \
    for f in ${deb_dir}/libswsscommon-dev_*.deb; do \
        [[ -f "$f" ]] && build_dev_debs+=("$f"); \
    done; \
    if [[ ${#build_dev_debs[@]} -gt 0 ]]; then \
        apt-get install -y --no-install-recommends "${build_dev_debs[@]}"; \
    else \
        apt-get install -y --no-install-recommends libswsscommon-dev || true; \
    fi; \
    apt-get install -y --no-install-recommends g++ && \
    g++ -shared -fPIC -o /usr/local/lib/libswss_log_stdout.so \
        /opt/docker-sai-test-vpp/swss_log_stdout_preload.cpp -lswsscommon && \
    apt-get purge -y g++ && \
    if [[ ${#build_dev_debs[@]} -gt 0 ]]; then apt-get purge -y libswsscommon-dev; fi && \
    apt-get autoremove -y && \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*

RUN install -d \
        /etc/sai \
        /run/vpp \
        /var/run/redis \
        /test-results \
        /usr/share/sonic/hwsku \
        /var/log && \
    install -m 0644 /opt/docker-sai-test-vpp/sai.profile /etc/sai/sai.profile && \
    install -m 0644 /opt/docker-sai-test-vpp/lanemap.ini /etc/sai/lanemap.ini && \
    install -m 0644 /opt/docker-sai-test-vpp/port-map.ini /etc/sai/port-map.ini && \
    install -m 0644 /opt/docker-sai-test-vpp/ptf-port-map.ini /etc/sai/ptf-port-map.ini && \
    install -m 0755 /opt/docker-sai-test-vpp/run_test.sh /usr/local/bin/run_test.sh && \
    ln -sf /usr/local/bin/run_test.sh /run_test.sh

WORKDIR /

# This is a single-purpose, --privileged test harness: the entrypoint starts VPP,
# Redis and saiserver, creates/destroys veth + PortChannel netdevs, and uses raw
# AF_PACKET sockets, all of which require root inside the container. It is run
# manually/in CI as a disposable test container, never as a deployed service, so a
# non-root USER would break the harness without adding meaningful isolation.
# nosemgrep: dockerfile.security.missing-user-entrypoint.missing-user-entrypoint
ENTRYPOINT ["/usr/local/bin/run_test.sh"]