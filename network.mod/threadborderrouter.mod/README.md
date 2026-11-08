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
`ThreadBorderRouterState()` reports OpenThread's actual routing state:
uninitialized (0), disabled (1), stopped (2), or active (3). This distinguishes
a configured border router from one temporarily stopped by backbone link loss.
While border routing runs, the module keeps a backbone IPv6 address in the
favored on-link prefix. It uses the interface MAC to form the address when no
address in that prefix already exists. It tracks prefix changes and removes
only the address it added when border routing stops. On link recovery it
requests a new link-local IPv6 address once and waits for address validation.
The Ethernet example logs link, routing, and address transitions. If Ethernet
remains down for 30 seconds, it calls `EthernetRestart()` to restart ESP-IDF's
Ethernet driver and PHY autonegotiation without restarting the Thread mesh.

On the S31 Function-CoreBoard-1, Ethernet border routing has been verified to
connect by DHCP, advertise the `_meshcop._udp` service, send a route for its
Thread OMR prefix, and continue UDP exchanges with a C6 mesh peer. An nRF52
peer with Thread SLAAC enabled acquired an OMR address; a Mac on the LAN reached
it through the S31 with IPv6 ping and a UDP echo. The Mac initially retained
the route from an earlier Wi-Fi test. After that stale route was removed and
the Ethernet route was installed, the end-to-end exchange succeeded. The S31
then acquired `fd38:f2c:193a:291c:1c29:4ff:fed0:737f` on Ethernet from the
favored on-link prefix. The Mac reached this address with IPv6 ping and received
a UDP echo from the S31's own Thread address. IPv6 ping and UDP echo to the nRF
also continued to work through the S31.

An Ethernet cable interruption was tested on the S31. The link went down,
OpenThread routing changed from active to stopped, and the backbone IPv6
addresses were removed. After reconnection, the link-local and on-link IPv6
addresses returned, routing became active, and the Mac received UDP echoes
from both the S31 and nRF again. The 30-second Ethernet driver restart fallback
was not needed in the successful recovery test.
