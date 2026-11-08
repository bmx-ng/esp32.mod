# ESP32 Thread border router

Import `ESP32.Network.ThreadBorderRouter` on an ESP32 with an IEEE 802.15.4
radio and a Wi-Fi or Ethernet backbone. This selects the ESP-IDF border
routing and mDNS build profile for the application. The
[border router API declarations](threadborderrouter.bmx) document each call.

## Start with Ethernet

```blitzmax
Import ESP32.Network.ThreadBorderRouter
Import ESP32.Network.Ethernet

If EthernetInitialize() <> 0 Then RuntimeError "Ethernet failed"
If EthernetWaitForIP(20000) <> 0 Then RuntimeError "No backbone IP"
If ThreadBorderRouterUseEthernet() <> 0 Then RuntimeError "Backbone selection failed"

Local result:Int = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then result = ThreadForm("MyMesh", 15)
If result <> 0 Or ThreadWaitForAttached() < ThreadRoleChild Then
    RuntimeError "Thread attach failed"
End If
If ThreadBorderRouterStart() <> 0 Then RuntimeError "Border routing failed"
```

Select the backbone **before** starting Thread. For Wi-Fi, connect with
`WiFiInitialize` and `WiFiConnectWait`, then call
`ThreadBorderRouterUseWiFi()` at the same point. Call
`ThreadBorderRouterStop()` to release border routing services.

`ThreadBorderRouterState()` reports Uninitialized, Disabled, Stopped, or
Active. `ThreadBorderRouterRunning()` is true only while active.
`ThreadBorderRouterBackboneIPv6Address(index)` enumerates backbone addresses
for diagnostics. Link loss can move routing from Active to Stopped; the module
tracks backbone IPv6 prefix changes and removes only an address it added.

## Examples and tested configuration

- [Ethernet border router](../../examples/thread_border_router_ethernet.bmx):
  resumes the saved mesh, serves UDP echo, and monitors link recovery.
- [Wi-Fi border router](../../examples/thread_border_router_wifi.bmx):
  shows the same setup with Wi-Fi. Fill in its ignored local settings file
  before building.

On the S31 Function-CoreBoard-1, Ethernet routing has passed a LAN-to-Thread
IPv6 ping and UDP echo test with an nRF52840 peer, including an Ethernet cable
interruption and recovery. Wi-Fi station plus Thread on this S31 did not pass
an end-to-end LAN reachability test: incoming traffic stopped after the Thread
radio started. Use Ethernet for the tested S31 configuration. Espressif
[documents S31 Wi-Fi/Thread router coexistence as unstable](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-guides/coexist.html).

A border router provides IPv6 routing. IPv4-only destinations additionally
need NAT64 service; this module does not expose NAT64 configuration.
