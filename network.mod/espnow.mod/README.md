# ESP32.Network.ESPNow

`ESP32.Network.ESPNow` provides connectionless peer-to-peer communication using
the ESP32 Wi-Fi radio. Devices communicate directly, without joining an access
point or obtaining an IP address.

ESP-NOW is an Espressif-specific protocol, so this is deliberately an ESP32
module rather than a shared `Embedded.Network` API.

## Starting ESP-NOW

Initialize the portable Wi-Fi layer first, choose a common channel, and then
initialize ESP-NOW:

```blitzmax
Import Embedded.Network.WiFi
Import ESP32.Network.ESPNow

Local result:Int = WiFiInitialize()
If result <> 0 Then RuntimeError ESPNowResultName(result)

result = ESPNowSetChannel(1)
If result <> 0 Then RuntimeError ESPNowResultName(result)

result = ESPNowInitialize()
If result <> 0 Then RuntimeError ESPNowResultName(result)
```

All communicating devices must use the same Wi-Fi channel. A station connected
to an access point normally follows that access point's channel.

`ESPNowInitialize()` reserves native receive and delivery-report queues. Its
arguments let memory-sensitive applications choose their queue capacities and
maximum received packet size:

```blitzmax
' Eight queued packets with the ESP-NOW v1 interoperability limit.
ESPNowInitialize(8, 16, ESPNowMaximumPayloadV1)
```

The default accepts ESP-NOW v2 packets up to 1,470 bytes. Selecting the 250-byte
v1 limit saves 1,220 bytes for every receive queue slot. Packets larger than the
selected receive limit are dropped and counted by `ESPNowDroppedReceived()`.

## Addresses and peers

Addresses are six-byte Wi-Fi station MAC addresses:

```blitzmax
Local localAddress:Byte[ESPNowAddressLength]
ESPNowLocalAddress(localAddress)
Print ESPNowFormatAddress(localAddress)

Local peerAddress:Byte[] = ESPNowParseAddress("24:6f:28:12:34:56")
result = ESPNowAddPeer(peerAddress, 1)
```

Register the broadcast address before broadcasting:

```blitzmax
ESPNowAddBroadcastPeer(1)
ESPNowBroadcast(payload)
```

ESP-NOW supports at most 20 peers. ESP-IDF normally permits up to six encrypted
peers; the precise encrypted-peer limit is an SDK configuration choice.

## Encryption

Encrypted unicast uses a shared 16-byte primary master key (PMK) and a 16-byte
local master key (LMK) for each peer:

```blitzmax
ESPNowSetPrimaryMasterKey(primaryMasterKey)
ESPNowAddPeer(peerAddress, 1, localMasterKey)
```

The corresponding peer must use the same PMK and LMK. Broadcast and multicast
ESP-NOW traffic cannot be encrypted by the ESP-NOW peer mechanism.

## Sending and receiving

`ESPNowSend()` queues a packet for one registered peer. Successful return means
the request was accepted by ESP-IDF; poll the separate delivery queue to learn
whether the frame was acknowledged at the MAC layer:

```blitzmax
Local report:SESPNowDeliveryReport
While ESPNowPollDelivery(report)
	Print ESPNowFormatAddress(report.destinationAddress) + ": " + report.delivered
Wend
```

Received packets include source and destination addresses, RSSI, channel, and a
managed byte array containing the payload:

```blitzmax
Local packet:SESPNowReceivedPacket
While ESPNowPollReceived(packet)
	Print ESPNowFormatAddress(packet.sourceAddress) + ": " + packet.data.length + " bytes"
Wend
```

The native Wi-Fi callbacks never enter managed BlitzMax code. They only copy
records into bounded native queues and wake the runtime. Managed arrays are
created later, when application code calls a poll function.

A successful delivery report does not provide application-level reliability or
ordering. Protocols that require those properties should add sequence numbers,
acknowledgements, retries, and duplicate detection to their own payload format.

## Shutdown

Call `ESPNowDeinitialize()` before `WiFiDeinitialize()`. The Wi-Fi layer refuses
to shut down while ESP-NOW is active, preventing callbacks from outliving the
radio they depend on.
