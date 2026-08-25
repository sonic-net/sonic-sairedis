"""VPP-specific BFD fixture for the shared SAI notification tests."""

import os
import re
import subprocess
import threading
from unittest import SkipTest

from ptf import config as ptf_config

from sai_thrift.sai_adapter import *
from sai_utils import sai_ipaddress, sai_ipprefix


BFD_NOTIFICATION_TIMEOUT = 25.0
VPPCTL_TIMEOUT = 10
SNIFFER_START_TIMEOUT = 5.0
BFD_EPHEMERAL_SRC_PORT = 49152
BFD_STATE_INIT = 2
BFD_STATE_UP = 3
# A scapy responder cannot sustain a sub-second reply cadence reliably, and at
# the SAI default the negotiated interval drops the session within 300ms of a
# missed reply. One second with a multiplier of three keeps the session stable
# while still detecting a broken path in about three seconds.
BFD_INTERVAL_USEC = 1000000
BFD_MULTIPLIER = 3


def create_fixture(test_obj, multihop=False):
    if os.environ.get("SIMULATE_SONIC") != "1":
        raise SkipTest("VPP BFD fixture requires SIMULATE_SONIC=1")
    return VppBfdFixture(test_obj, multihop)


class BfdResponder:
    """Respond to a BFD session on every member of the VPP LAG."""

    def __init__(self, interface_names, local_ip, remote_ip, udp_port, discriminator):
        self.interface_names = list(interface_names)
        self.local_ip = local_ip
        self.remote_ip = remote_ip
        self.udp_port = udp_port
        self.discriminator = discriminator
        from scapy.all import get_if_hwaddr

        self.source_macs = {
            name: get_if_hwaddr(name) for name in self.interface_names
        }
        self.sniffers = []

    def start(self):
        import functools

        from scapy.all import AsyncSniffer

        for interface_name in self.interface_names:
            ready = threading.Event()
            sniffer = AsyncSniffer(
                iface=interface_name,
                filter="udp dst port {} and src host {}".format(
                    self.udp_port, self.local_ip
                ),
                store=False,
                prn=functools.partial(self._respond, interface_name),
                started_callback=ready.set,
            )
            sniffer.start()
            self.sniffers.append(sniffer)

            if not ready.wait(timeout=SNIFFER_START_TIMEOUT):
                self.stop()
                raise AssertionError(
                    "BFD responder failed to start capture on {}".format(
                        interface_name
                    )
                )

    def stop(self):
        for sniffer in self.sniffers:
            try:
                sniffer.stop()
            except Exception:
                # stop() raises if the sniffer never reached its run loop, which
                # happens when start() aborts partway and unwinds through here.
                pass
        self.sniffers = []

    def _respond(self, interface_name, packet):
        from scapy.all import Ether, IP, UDP, sendp
        from scapy.contrib.bfd import BFD

        if not packet.haslayer(Ether) or not packet.haslayer(IP):
            return
        if not packet.haslayer(UDP):
            return

        udp = packet[UDP]
        if udp.dport != self.udp_port:
            return

        bfd = packet.getlayer(BFD)
        if bfd is None:
            try:
                bfd = BFD(bytes(udp.payload))
            except Exception:
                return

        # Follow the RFC 5880 state machine. A peer sitting in Down only leaves
        # it when it hears Down or Init, so a responder that always advertises
        # Up leaves the session stuck with VPP Down and remote Up forever.
        response_state = (
            BFD_STATE_UP
            if bfd.sta in (BFD_STATE_INIT, BFD_STATE_UP)
            else BFD_STATE_INIT
        )

        response = (
            Ether(src=self.source_macs[interface_name], dst=packet[Ether].src)
            # Single-hop BFD is GTSM protected, so the reply has to arrive with
            # TTL 255. RFC 5880 also fixes the destination port at 3784 (4784 for
            # multihop) in both directions, with an ephemeral source port; VPP
            # silently ignores a reply that mirrors the ports instead.
            / IP(src=self.remote_ip, dst=self.local_ip, ttl=255)
            / UDP(sport=BFD_EPHEMERAL_SRC_PORT, dport=self.udp_port)
            / BFD(
                version=1,
                diag=0,
                sta=response_state,
                flags=0,
                detect_mult=BFD_MULTIPLIER,
                my_discriminator=self.discriminator,
                your_discriminator=bfd.my_discriminator,
                min_tx_interval=BFD_INTERVAL_USEC,
                min_rx_interval=BFD_INTERVAL_USEC,
                echo_rx_interval=0,
            )
        )
        sendp(response, iface=interface_name, verbose=False)


class VppBfdFixture:
    """Build and control the LAG-backed BFD topology used by the VPP harness."""

    notification_timeout = BFD_NOTIFICATION_TIMEOUT

    def __init__(self, test_obj, multihop=False):
        self.test_obj = test_obj
        self.multihop = multihop
        self.local_ip = "10.1.1.1"
        self.remote_ip = "10.1.2.2" if multihop else "10.1.1.2"
        self.gateway_ip = "10.1.1.2"
        self.local_discriminator = 0x1001
        self.remote_discriminator = 0x2001
        self.udp_port = 4784 if multihop else 3784
        self.bfd_session = None
        self.lag_rif = None
        self.owns_lag_rif = False
        self.neighbor_entry = None
        self.next_hop = None
        self.route_entry = None
        self.responder = None
        self.peers = []

    def common_config_kwargs(self):
        return {
            "is_remove_default_vlan": False,
            "is_create_vlan": False,
            "is_create_fdb": False,
            "is_create_default_route": False,
            "is_create_lag": True,
            "is_create_vlan_itf": False,
            "is_create_route_for_vlan_itf": False,
            "is_create_route_for_lag": False,
            "wait_sec": 1,
        }

    @staticmethod
    def peer_interface(port_index):
        for _, configured_port, interface_name in ptf_config.get("interfaces", []):
            if configured_port == port_index:
                if not re.fullmatch(r"OEth[0-9]+_peer", interface_name):
                    raise AssertionError(
                        "unexpected VPP PTF peer interface: {}".format(interface_name)
                    )
                return interface_name
        raise AssertionError(
            "PTF interface for port {} was not configured".format(port_index)
        )

    def setup(self):
        if not self.test_obj.dut.default_vrf:
            self.test_obj.route_configer.get_default_virtual_router()

        lag = self.test_obj.dut.lag_list[0]
        existing_rifs = set(lag.rif_list or [])
        self.lag_rif = self.test_obj.route_configer.create_router_interface(lag)
        self.owns_lag_rif = self.lag_rif not in existing_rifs
        self.peers = [
            self.peer_interface(port_index)
            for port_index in lag.member_port_indexs
        ]
        from scapy.all import get_if_hwaddr

        peer_mac = get_if_hwaddr(self.peers[0])
        self.neighbor_entry = sai_thrift_neighbor_entry_t(
            rif_id=self.lag_rif,
            ip_address=sai_ipaddress(self.gateway_ip),
        )
        status = sai_thrift_create_neighbor_entry(
            self.test_obj.client,
            self.neighbor_entry,
            dst_mac_address=peer_mac,
            no_host_route=False,
        )
        self.test_obj.assertEqual(status, SAI_STATUS_SUCCESS)

        if self.multihop:
            self.next_hop = sai_thrift_create_next_hop(
                self.test_obj.client,
                ip=sai_ipaddress(self.gateway_ip),
                router_interface_id=self.lag_rif,
                type=SAI_NEXT_HOP_TYPE_IP,
            )
            self.test_obj.assertEqual(self.test_obj.status(), SAI_STATUS_SUCCESS)
            self.route_entry = sai_thrift_route_entry_t(
                vr_id=self.test_obj.dut.default_vrf,
                destination=sai_ipprefix(self.remote_ip + "/32"),
            )
            status = sai_thrift_create_route_entry(
                self.test_obj.client,
                self.route_entry,
                next_hop_id=self.next_hop,
            )
            self.test_obj.assertEqual(status, SAI_STATUS_SUCCESS)

    def start_session(self):
        self.responder = BfdResponder(
            self.peers,
            self.local_ip,
            self.remote_ip,
            self.udp_port,
            self.remote_discriminator,
        )
        self.responder.start()

        try:
            self.bfd_session = sai_thrift_create_bfd_session(
                self.test_obj.client,
                type=SAI_BFD_SESSION_TYPE_ASYNC_ACTIVE,
                virtual_router=self.test_obj.dut.default_vrf,
                local_discriminator=self.local_discriminator,
                remote_discriminator=self.remote_discriminator,
                udp_src_port=BFD_EPHEMERAL_SRC_PORT,
                bfd_encapsulation_type=SAI_BFD_ENCAPSULATION_TYPE_NONE,
                iphdr_version=4,
                src_ip_address=sai_ipaddress(self.local_ip),
                dst_ip_address=sai_ipaddress(self.remote_ip),
                min_tx=BFD_INTERVAL_USEC,
                min_rx=BFD_INTERVAL_USEC,
                multiplier=BFD_MULTIPLIER,
                hw_lookup_valid=True,
                multihop=self.multihop,
                cbit=False,
                admin_state=True,
            )
            self.test_obj.assertNotEqual(self.bfd_session, SAI_NULL_OBJECT_ID)
            self.test_obj.assertEqual(self.test_obj.status(), SAI_STATUS_SUCCESS)
        except Exception:
            self.stop_peer()
            raise

        return self.bfd_session

    def stop_peer(self):
        if self.responder is not None:
            self.responder.stop()
            self.responder = None

    @staticmethod
    def vppctl(*command):
        result = subprocess.run(
            ["vppctl"] + list(command),
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=VPPCTL_TIMEOUT,
        )
        return result.stdout.decode("utf-8", "replace")

    def assert_external_state(self, expected_state_word):
        """Verify the BFD transition in VPP independently of the SAI object."""
        output = self.vppctl("show", "bfd", "sessions")
        print("vppctl show bfd sessions:\n{}".format(output))
        self.test_obj.assertIn(
            self.remote_ip,
            output,
            "VPP has no BFD session towards {}".format(self.remote_ip),
        )
        self.test_obj.assertRegex(
            output,
            r"(?i)\b{}\b".format(expected_state_word),
            "VPP did not report BFD state {}".format(expected_state_word),
        )

    def teardown(self):
        self.stop_peer()
        client = getattr(self.test_obj, "client", None)
        if client is None:
            return

        if self.bfd_session is not None:
            sai_thrift_remove_bfd_session(client, self.bfd_session)
            self.bfd_session = None
        if self.route_entry is not None:
            sai_thrift_remove_route_entry(client, self.route_entry)
            self.route_entry = None
        if self.next_hop is not None:
            sai_thrift_remove_next_hop(client, self.next_hop)
            self.next_hop = None
        if self.neighbor_entry is not None:
            sai_thrift_remove_neighbor_entry(client, self.neighbor_entry)
            self.neighbor_entry = None
        if self.owns_lag_rif and self.lag_rif is not None:
            sai_thrift_remove_router_interface(client, self.lag_rif)
            self.lag_rif = None
            self.owns_lag_rif = False