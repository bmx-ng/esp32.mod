# ESP32 Thread border router

Import `ESP32.Network.ThreadBorderRouter` on an ESP32 with an IEEE 802.15.4
radio and a Wi-Fi or Ethernet backbone. The import selects a separate ESP-IDF OpenThread build
profile with border routing and the mDNS component enabled. Builds without this
import continue to use their own smaller Thread profile.

Connect Wi-Fi with `WiFiInitialize` and `WiFiConnectWait`, then call
`ThreadBorderRouterUseWiFi` **before** `ThreadStartSaved` or `ThreadForm`.
For a board with Ethernet, import `ESP32.Network.Ethernet`, call
`EthernetInitialize` and `EthernetWaitForIP`, then select it with
`ThreadBorderRouterUseEthernet` before starting Thread.
Once Thread has attached, call `ThreadBorderRouterStart`. The selection call binds
the chosen network interface as OpenThread's backbone; `ThreadBorderRouterStart` initializes
ESP-IDF's border router services. `ThreadBorderRouterStop` deinitializes those
services. All return ESP-IDF error codes, with zero for success.

The S31 examples are `examples/thread_border_router_wifi.bmx` and
`examples/thread_border_router_ethernet.bmx`. The Wi-Fi example's local
`examples/thread_border_router_wifi.local.bmx` settings file is ignored by Git.
Both resume the saved mesh if present and provide UDP echo on port 12345.
`ThreadBorderRouterBackboneIPv6Address(index)` lists the backbone interface's
current IPv6 addresses for diagnostics. An empty string ends the list.

On the S31 Function-CoreBoard-1, Ethernet border routing has been verified to
connect by DHCP, advertise the `_meshcop._udp` service, send a route for its
Thread OMR prefix, and continue UDP exchanges with a C6 mesh peer. An nRF52
peer with Thread SLAAC enabled acquired an OMR address; a Mac on the LAN reached
it through the S31 with IPv6 ping and a UDP echo. The Mac initially retained
the route from an earlier Wi-Fi test. After that stale route was removed and
the Ethernet route was installed, the end-to-end exchange succeeded. The S31's
own backbone interface had only a link-local IPv6 address in this test, so a
UDP echo hosted locally on the S31 did not reply to a LAN ULA sender.
