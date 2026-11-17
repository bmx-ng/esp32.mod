# ESP32.Network.ESPNow

`ESP32.Network.ESPNow` provides direct, connectionless communication between
devices using the ESP32 Wi-Fi radio. Devices do not need to join an access
point, obtain an IP address, or run an IP protocol.

ESP-NOW is useful for sensor nodes, remote controls, local device discovery,
small telemetry networks, and low-latency commands. It is an Espressif-specific
protocol, so this module belongs to `ESP32.Network` rather than the portable
`Embedded.Network` namespace.

## Requirements and target support

An application needs:

- an ESP32 target with a supported 2.4 GHz Wi-Fi radio;
- `Embedded.Network.WiFi` initialized in station mode;
- the same Wi-Fi channel on every communicating device; and
- the destination station MAC registered as a peer before unicast transmission.

Targets without an onboard Wi-Fi radio, such as the ESP32-H2 and bare ESP32-P4,
cannot use this module. Importing the module includes the ESP-IDF ESP-NOW
component only when the application needs it.

The implementation has been validated bidirectionally between an original
ESP32-WROOM-32D and an ESP32-C6 using ESP-IDF 6.1. Testing covered broadcast,
encrypted unicast, 250-byte v1 payloads, 1,470-byte v2 payloads, delivery
reports, peer counts, queueing, and shutdown ordering.

## First device: identity and version

Build `examples/espnow_identity.bmx` on each device first. It prints the station
MAC address needed to register that device as a peer:

```sh
bmk makeapp -a -r -board xiao_esp32c6 -x examples/espnow_identity.bmx
```

The essential initialization is:

```blitzmax
Import Embedded.Network.WiFi
Import ESP32.Network.ESPNow

Local result:Int = WiFiInitialize()
If result <> 0 Then RuntimeError ESPNowResultName(result)

result = ESPNowSetChannel(1)
If result <> 0 Then RuntimeError ESPNowResultName(result)

result = ESPNowInitialize()
If result <> 0 Then RuntimeError ESPNowResultName(result)

Local address:Byte[ESPNowAddressLength]
ESPNowLocalAddress(address)
Print ESPNowFormatAddress(address)
```

`WiFiInitialize()` starts the radio but does not require an access-point
connection. ESP-NOW is usable immediately after initialization and peer setup.

## Wi-Fi channels and access-point coexistence

Every peer must transmit and listen on the same primary Wi-Fi channel. Select a
channel before initializing peers:

```blitzmax
ESPNowSetChannel(6)
```

Channels available to an application depend on its configured Wi-Fi country.
When a station joins an access point, the station follows the access point's
channel. In that configuration, register ESP-NOW peers on the access point's
channel instead of trying to keep the radio on a different one.

Read the active channel when needed:

```blitzmax
Local channel:Int
Local result:Int = ESPNowGetChannel(channel)
```

## Protocol versions and payload sizes

ESP-NOW has two relevant payload limits:

| Protocol | Maximum application payload | Constant |
| --- | ---: | --- |
| ESP-NOW v1 | 250 bytes | `ESPNowMaximumPayloadV1` |
| ESP-NOW v2 | 1,470 bytes | `ESPNowMaximumPayloadV2` |

The module defaults to the v2 limit. A v2 device can receive v1 and v2 traffic.
For dependable interoperability with a v1 peer, keep application payloads at or
below 250 bytes. The module does not automatically fragment larger messages.

Query the native protocol version implemented by ESP-IDF:

```blitzmax
Local version:UInt
Local result:Int = ESPNowVersion(version)
```

## Broadcast

Broadcast is convenient for discovery, announcements, and data intended for
every nearby listener on the channel. Register the broadcast peer once before
sending:

```blitzmax
ESPNowAddBroadcastPeer(1)

Local payload:Byte[] = [$42, $4d, $58]
Local result:Int = ESPNowBroadcast(payload)
```

Broadcast is not encrypted by ESP-NOW's peer encryption. Add application-level
authentication or encryption if broadcast authenticity or confidentiality is
required.

`examples/espnow_broadcast_chat.bmx` is a complete symmetric example: flash it
onto two or more boards and each board periodically announces itself while
printing messages from the others.

## Unencrypted unicast

Unicast targets one registered station address:

```blitzmax
Local peer:Byte[] = ESPNowParseAddress("24:6f:28:12:34:56")
If Not peer Then RuntimeError "Invalid peer address"

Local result:Int = ESPNowAddPeer(peer, 1)
If result <> 0 Then RuntimeError ESPNowResultName(result)

Local payload:Byte[32]
result = ESPNowSend(peer, payload)
```

The default channel argument of zero means “use the current local channel”:

```blitzmax
ESPNowAddPeer(peer)
```

An existing peer is updated when `ESPNowAddPeer` is called again for the same
address. `ESPNowPeerExists`, `ESPNowDeletePeer`, and `ESPNowPeerCounts` support
peer lifecycle and diagnostics.

`examples/espnow_unicast.bmx` demonstrates a paced bidirectional link. Change
`PeerAddressText` on each device to the station MAC of the other device.

## Encrypted unicast

ESP-NOW protects unicast peers with CCMP. Encryption uses two 16-byte values:

- the primary master key (PMK), shared by the participating devices and used to
  protect local master keys; and
- a local master key (LMK), supplied when a particular peer is registered and
  used to protect that peer's application frames.

Set the PMK first, then register the peer with an LMK:

```blitzmax
Local pmk:Byte[ESPNowKeyLength]
Local lmk:Byte[ESPNowKeyLength]
' Fill both arrays from secure configuration or generated key material.

ESPNowSetPrimaryMasterKey(pmk)
ESPNowAddPeer(peer, 1, lmk)
```

Both ends of the link must use the same PMK and matching LMK. Never reuse the
demonstration keys from the examples in deployed firmware, and avoid committing
production keys to source control. ESP-IDF supports at most 20 registered peers;
the encrypted-peer limit is controlled by the ESP-IDF configuration and is
normally lower.

`examples/espnow_encrypted_peer.bmx` demonstrates encrypted 1,470-byte v2
traffic and reports the total and encrypted peer counts. Change its peer MAC on
each device before running it.

## Receiving packets

The ESP-IDF receive callback runs in a high-priority Wi-Fi task. It must not
enter managed code, allocate managed objects, or perform application work.
This module therefore copies each native frame into a bounded native queue and
wakes the runtime. Application code polls and creates managed arrays later:

```blitzmax
Local packet:SESPNowReceivedPacket
While ESPNowPollReceived(packet)
	Print "From " + ESPNowFormatAddress(packet.sourceAddress)
	Print "Bytes: " + packet.data.length
	Print "RSSI: " + packet.rssi + " dBm"
	Print "Channel: " + packet.channel
Wend
```

Each returned packet owns its source address, destination address, and payload
arrays. Retaining one packet does not block the queue or make its arrays change
on the next poll.

## Delivery reports and application reliability

`ESPNowSend` and `ESPNowBroadcast` return when ESP-IDF accepts a send request.
The eventual MAC-layer result arrives separately:

```blitzmax
Local report:SESPNowDeliveryReport
While ESPNowPollDelivery(report)
	If report.delivered Then
		Print "MAC delivery acknowledged"
	Else
		Print "MAC delivery failed"
	End If
Wend
```

A successful delivery report does not prove that the peer application processed
or stored the message. ESP-NOW also does not give the application automatic
ordering, duplicate suppression, end-to-end acknowledgements, retries, or
fragmentation. A reliable application protocol should normally add:

- a message type and protocol version;
- a monotonically increasing sequence number;
- an application acknowledgement for important commands;
- a bounded timeout and retry policy; and
- duplicate detection at the receiver.

For sustained or large transfers, wait for the previous delivery report before
queuing the next frame. This reduces pressure on the Wi-Fi task and preserves a
simple correspondence between requests and reports.

## Queue sizing and memory

The default initialization is equivalent to:

```blitzmax
ESPNowInitialize(ESPNowDefaultReceiveQueueCapacity, ..
	ESPNowDefaultDeliveryQueueCapacity, ESPNowMaximumPayloadV2)
```

Receive payload storage is reserved when ESP-NOW starts. Its dominant cost is:

```text
receive queue capacity × maximum received payload
```

The default eight v2 slots reserve 11,760 payload bytes, plus small packet and
delivery metadata. An application that only interoperates with v1 devices can
save 1,220 bytes per receive slot:

```blitzmax
ESPNowInitialize(8, 16, ESPNowMaximumPayloadV1)
```

A very small receiver might choose:

```blitzmax
ESPNowInitialize(2, 4, ESPNowMaximumPayloadV1)
```

Queue capacities must be from 1 through `ESPNowMaximumQueueCapacity`. A packet
larger than the configured receive limit is discarded, as is a packet arriving
while the queue is full. Inspect the saturating counters:

```blitzmax
Print "RX drops: " + ESPNowDroppedReceived()
Print "Delivery-report drops: " + ESPNowDroppedDeliveries()
```

If either counter rises during ordinary operation, poll more frequently, reduce
the transmit rate, or enlarge the corresponding queue. See
`examples/espnow_low_memory_receiver.bmx` for a deliberately small v1 receiver.

## Binary and text payloads

ESP-NOW payloads are byte arrays and may contain any binary representation.
Define byte order, sizes, and a protocol version explicitly when devices may be
built with different firmware revisions. Do not transmit native structure
memory unless its layout and endianness are deliberately fixed.

Text can be encoded as UTF-8 before sending and decoded after receipt.
`examples/espnow_broadcast_chat.bmx` includes both conversions. Remember that
UTF-8 characters may occupy more than one payload byte.

## Errors

Most operations return `ESP_OK` (zero) or an ESP-IDF error code. Convert a code
to a useful symbolic name with `ESPNowResultName`:

```blitzmax
Local result:Int = ESPNowSend(peer, payload)
If result <> 0 Then
	Print "Send failed: " + ESPNowResultName(result) + " (" + result + ")"
End If
```

Common causes include ESP-NOW not being initialized, an invalid address or key
length, a missing peer, incompatible channels, a full native Wi-Fi send queue,
or insufficient memory.

## Shutdown and restart

Stop ESP-NOW before stopping Wi-Fi:

```blitzmax
ESPNowDeinitialize()
WiFiDeinitialize()
```

The Wi-Fi adapter refuses to deinitialize while ESP-NOW remains active. This
prevents native callbacks from outliving the radio and queues they depend on.
Calling `ESPNowDeinitialize` when already stopped is safe. Peers and keys belong
to the current ESP-NOW session and must be configured again after restart.

`examples/espnow_lifecycle_test.bmx` verifies the required ordering on hardware.

## Example catalogue

| Example | Purpose |
| --- | --- |
| `espnow_identity.bmx` | Print local station MAC, ESP-NOW version, and channel. |
| `espnow_broadcast_chat.bmx` | Exchange readable broadcast messages among several devices. |
| `espnow_unicast.bmx` | Register and pace an unencrypted bidirectional peer. |
| `espnow_encrypted_peer.bmx` | Configure PMK/LMK encryption and send maximum-size v2 packets. |
| `espnow_low_memory_receiver.bmx` | Use small queues and the 250-byte v1 receive limit. |
| `espnow_lifecycle_test.bmx` | Verify ESP-NOW and Wi-Fi shutdown ordering. |
| `espnow_peer_test.bmx` | Hardware regression between the validated WROOM-32D and XIAO C6. |

The peer examples contain an address constant that must be changed for the
devices being used. Run the identity example first, and ensure both devices use
the same channel.
