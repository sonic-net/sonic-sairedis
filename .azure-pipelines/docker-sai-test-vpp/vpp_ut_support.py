# VPP UT harness helpers — kernel netdev and VPP CLI setup that production SONiC
# provides via teamd / IntfMgr / linux_nl.  Imported from sai_test setUp paths
# when SAI_VPP_UT_HARNESS=1 (set by docker-sai-test-vpp/run_test.sh).

import os
import subprocess
import time


def enabled():
    return os.environ.get("SAI_VPP_UT_HARNESS", "0") == "1"


def _run(cmd, check=False):
    return subprocess.run(
        cmd, check=check, capture_output=True, text=True)


def _mtu():
    return int(os.environ.get("MTU", "9100"))


def _vppctl_timeout():
    return int(os.environ.get("VPPCTL_TIMEOUT", "5"))


def ensure_portchannel(lag_index):
    """Create PortChannel<lag_index> if missing (teamd stand-in for VPP SAI LAG)."""
    if not enabled():
        return

    pc_if = "PortChannel{}".format(lag_index)
    mtu = _mtu()

    if _run(["ip", "link", "show", pc_if]).returncode != 0:
        if _run(["ip", "link", "add", pc_if, "type", "bond"]).returncode != 0:
            _run(["ip", "link", "add", pc_if, "type", "dummy"], check=False)

    _run(["ip", "link", "set", "dev", pc_if, "mtu", str(mtu)], check=False)
    _run(["ip", "link", "set", "dev", pc_if, "up"], check=False)


def assign_lag_rif_ips(lag_index, retries=20, interval=0.3):
    """Assign connected IPs on the BondEthernet LCP tap (be<N>) for a LAG RIF."""
    if not enabled() or os.environ.get("LAG_RIF_IPS", "1") != "1":
        return

    be_prefix = os.environ.get("LAG_BE_TAP_PREFIX", "be")
    v4_pattern = os.environ.get("LAG_RIF_IPV4_PATTERN", "10.1.%d.1/24")
    v6_pattern = os.environ.get("LAG_RIF_IPV6_PATTERN", "fc00:1::%d:1/112")
    subnet_id = lag_index + 1
    be_if = "{}{}".format(be_prefix, lag_index)
    v4_addr = v4_pattern % subnet_id
    v6_addr = v6_pattern % subnet_id

    for _ in range(retries):
        if _run(["ip", "link", "show", be_if]).returncode == 0:
            break
        time.sleep(interval)
    else:
        return

    disable_v6 = "/proc/sys/net/ipv6/conf/{}/disable_ipv6".format(be_if)
    accept_dad = "/proc/sys/net/ipv6/conf/{}/accept_dad".format(be_if)
    try:
        with open(disable_v6, "w", encoding="ascii") as fh:
            fh.write("0")
        with open(accept_dad, "w", encoding="ascii") as fh:
            fh.write("0")
    except OSError:
        pass

    _run(["ip", "addr", "add", v4_addr, "dev", be_if], check=False)
    _run(["ip", "-6", "addr", "add", v6_addr, "dev", be_if, "nodad"], check=False)


def _svi_group_id(vlan_id):
    for entry in os.environ.get("SVI_RIF_VLANS", "10:1 20:2").split():
        vid, gid = entry.split(":", 1)
        if int(vid) == int(vlan_id):
            return int(gid)
    return int(vlan_id)


def assign_svi_rif_ips(vlan_id, retries=20, interval=0.3):
    """Assign connected IPs on a VLAN BVI (no linux_cp host-if to mirror from)."""
    if not enabled() or os.environ.get("SVI_RIF_IPS", "1") != "1":
        return

    bvi_prefix = os.environ.get("SVI_BVI_PREFIX", "bvi")
    v4_pattern = os.environ.get("SVI_RIF_IPV4_PATTERN", "192.168.%d.1/24")
    v6_pattern = os.environ.get("SVI_RIF_IPV6_PATTERN", "fc02::%d:1/112")
    group_id = _svi_group_id(vlan_id)
    bvi_if = "{}{}".format(bvi_prefix, vlan_id)
    v4_addr = v4_pattern % group_id
    v6_addr = v6_pattern % group_id
    timeout = str(_vppctl_timeout())

    for _ in range(retries):
        if _run(
            ["timeout", timeout, "vppctl", "show", "interface", bvi_if]
        ).returncode == 0:
            break
        time.sleep(interval)
    else:
        return

    _run(
        ["timeout", timeout, "vppctl", "set", "interface", "ip", "address",
         bvi_if, v4_addr],
        check=False,
    )
    _run(
        ["timeout", timeout, "vppctl", "set", "interface", "ip", "address",
         bvi_if, v6_addr],
        check=False,
    )
