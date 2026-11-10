This directory source code is merged here from
https://github.com/sonic-net/sonic-platform-vpp/tree/main/saivpp/src and is
UNCHECKED for ay bugs or completeness

TODO for future is to gradually bring it back to vslib/ directory
with slow refactoring to standard

## Route next-hop updates

Changing an IPv4 or IPv6 route between individual IP or tunnel-encapsulation
next hops uses VPP's replace-on-add operation without withdrawing the old
route. This keeps prefix-route dual-ToR mux switching hitless, including when
the neighbor has `NO_HOST_ROUTE`. A failed replacement leaves the committed
route and its statistics index intact. ECMP, MPLS, and other attribute updates
retain their existing delete/add behavior.

The VPP-enabled unit tests cover replacement, failure preservation, and the
unchanged packet-action and ECMP paths:

```bash
./unittest/vslib/tests --gtest_filter='*SwitchVppRouteTest*'
```
