' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP-NOW connectionless peer-to-peer networking for ESP32 devices.
about: ESP-NOW uses the ESP32 Wi-Fi radio without requiring an access point.
Initialize Embedded.Network.WiFi before this module. Native radio callbacks copy
received packets and delivery reports into bounded queues; managed objects are
only created when the application polls those queues.
End Rem
Module ESP32.Network.ESPNow
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Network.WiFi

Rem
bbdoc: Number of bytes in an ESP-NOW peer address.
End Rem
Const ESPNowAddressLength:Int = 6

Rem
bbdoc: Required number of bytes in a PMK or LMK encryption key.
End Rem
Const ESPNowKeyLength:Int = 16

Rem
bbdoc: Maximum payload accepted by ESP-NOW v1 peers, in bytes.
End Rem
Const ESPNowMaximumPayloadV1:Int = 250

Rem
bbdoc: Maximum payload accepted by ESP-NOW v2 peers, in bytes.
End Rem
Const ESPNowMaximumPayloadV2:Int = 1470

Rem
bbdoc: Maximum payload accepted by this module, equal to the ESP-NOW v2 limit.
End Rem
Const ESPNowMaximumPayload:Int = ESPNowMaximumPayloadV2

Rem
bbdoc: Default number of complete received packets retained by the native queue.
End Rem
Const ESPNowDefaultReceiveQueueCapacity:Int = 8

Rem
bbdoc: Default number of send-delivery reports retained by the native queue.
End Rem
Const ESPNowDefaultDeliveryQueueCapacity:Int = 16

Rem
bbdoc: Largest receive or delivery queue capacity accepted by ESPNowInitialize.
End Rem
Const ESPNowMaximumQueueCapacity:Int = 64

Rem
bbdoc: Text representation of the ESP-NOW broadcast address.
End Rem
Const ESPNowBroadcastAddress:String = "ff:ff:ff:ff:ff:ff"

Rem
bbdoc: Selects the station Wi-Fi interface for an ESP-NOW peer.
End Rem
Const ESPNowInterfaceStation:Int = 0

Rem
bbdoc: Selects the SoftAP Wi-Fi interface for an ESP-NOW peer.
End Rem
Const ESPNowInterfaceAccessPoint:Int = 1

Rem
bbdoc: Selects Espressif long-range PHY mode for a peer rate configuration.
End Rem
Const ESPNowPHYModeLongRange:Int = 0

Rem
bbdoc: Selects 802.11b PHY mode for a peer rate configuration.
End Rem
Const ESPNowPHYMode80211B:Int = 1

Rem
bbdoc: Selects 802.11g PHY mode for a peer rate configuration.
End Rem
Const ESPNowPHYMode80211G:Int = 2

Rem
bbdoc: Selects 802.11a PHY mode for a peer rate configuration.
End Rem
Const ESPNowPHYMode80211A:Int = 3

Rem
bbdoc: Selects 20 MHz 802.11n PHY mode for a peer rate configuration.
End Rem
Const ESPNowPHYModeHT20:Int = 4

Rem
bbdoc: Selects 40 MHz 802.11n PHY mode for a peer rate configuration.
End Rem
Const ESPNowPHYModeHT40:Int = 5

Rem
bbdoc: Selects 20 MHz 802.11ax PHY mode for a peer rate configuration.
End Rem
Const ESPNowPHYModeHE20:Int = 6

Rem
bbdoc: Selects 20 MHz 802.11ac PHY mode for a peer rate configuration.
End Rem
Const ESPNowPHYModeVHT20:Int = 7

Rem
bbdoc: Selects 1 Mbps with a long preamble.
End Rem
Const ESPNowRate1MbpsLong:Int = 0

Rem
bbdoc: Selects 2 Mbps with a long preamble.
End Rem
Const ESPNowRate2MbpsLong:Int = 1

Rem
bbdoc: Selects 5.5 Mbps with a long preamble.
End Rem
Const ESPNowRate5Point5MbpsLong:Int = 2

Rem
bbdoc: Selects 11 Mbps with a long preamble.
End Rem
Const ESPNowRate11MbpsLong:Int = 3

Rem
bbdoc: Selects 2 Mbps with a short preamble.
End Rem
Const ESPNowRate2MbpsShort:Int = 4

Rem
bbdoc: Selects 5.5 Mbps with a short preamble.
End Rem
Const ESPNowRate5Point5MbpsShort:Int = 5

Rem
bbdoc: Selects 11 Mbps with a short preamble.
End Rem
Const ESPNowRate11MbpsShort:Int = 6

Rem
bbdoc: Selects the legacy 48 Mbps rate.
End Rem
Const ESPNowRate48Mbps:Int = 7

Rem
bbdoc: Selects the legacy 24 Mbps rate.
End Rem
Const ESPNowRate24Mbps:Int = 8

Rem
bbdoc: Selects the legacy 12 Mbps rate.
End Rem
Const ESPNowRate12Mbps:Int = 9

Rem
bbdoc: Selects the legacy 6 Mbps rate.
End Rem
Const ESPNowRate6Mbps:Int = 10

Rem
bbdoc: Selects the legacy 54 Mbps rate.
End Rem
Const ESPNowRate54Mbps:Int = 11

Rem
bbdoc: Selects the legacy 36 Mbps rate.
End Rem
Const ESPNowRate36Mbps:Int = 12

Rem
bbdoc: Selects the legacy 18 Mbps rate.
End Rem
Const ESPNowRate18Mbps:Int = 13

Rem
bbdoc: Selects the legacy 9 Mbps rate.
End Rem
Const ESPNowRate9Mbps:Int = 14

Rem
bbdoc: Selects MCS0 with a long guard interval.
End Rem
Const ESPNowRateMCS0Long:Int = 15
Rem
bbdoc: Selects MCS1 with a long guard interval.
End Rem
Const ESPNowRateMCS1Long:Int = 16
Rem
bbdoc: Selects MCS2 with a long guard interval.
End Rem
Const ESPNowRateMCS2Long:Int = 17
Rem
bbdoc: Selects MCS3 with a long guard interval.
End Rem
Const ESPNowRateMCS3Long:Int = 18
Rem
bbdoc: Selects MCS4 with a long guard interval.
End Rem
Const ESPNowRateMCS4Long:Int = 19
Rem
bbdoc: Selects MCS5 with a long guard interval.
End Rem
Const ESPNowRateMCS5Long:Int = 20
Rem
bbdoc: Selects MCS6 with a long guard interval.
End Rem
Const ESPNowRateMCS6Long:Int = 21
Rem
bbdoc: Selects MCS7 with a long guard interval.
End Rem
Const ESPNowRateMCS7Long:Int = 22
Rem
bbdoc: Selects MCS8 with a long guard interval on HE-capable targets.
End Rem
Const ESPNowRateMCS8Long:Int = 23
Rem
bbdoc: Selects MCS9 with a long guard interval on HE-capable targets.
End Rem
Const ESPNowRateMCS9Long:Int = 24

Rem
bbdoc: Selects MCS0 with a short guard interval.
End Rem
Const ESPNowRateMCS0Short:Int = 25
Rem
bbdoc: Selects MCS1 with a short guard interval.
End Rem
Const ESPNowRateMCS1Short:Int = 26
Rem
bbdoc: Selects MCS2 with a short guard interval.
End Rem
Const ESPNowRateMCS2Short:Int = 27
Rem
bbdoc: Selects MCS3 with a short guard interval.
End Rem
Const ESPNowRateMCS3Short:Int = 28
Rem
bbdoc: Selects MCS4 with a short guard interval.
End Rem
Const ESPNowRateMCS4Short:Int = 29
Rem
bbdoc: Selects MCS5 with a short guard interval.
End Rem
Const ESPNowRateMCS5Short:Int = 30
Rem
bbdoc: Selects MCS6 with a short guard interval.
End Rem
Const ESPNowRateMCS6Short:Int = 31
Rem
bbdoc: Selects MCS7 with a short guard interval.
End Rem
Const ESPNowRateMCS7Short:Int = 32
Rem
bbdoc: Selects MCS8 with a short guard interval on HE-capable targets.
End Rem
Const ESPNowRateMCS8Short:Int = 33
Rem
bbdoc: Selects MCS9 with a short guard interval on HE-capable targets.
End Rem
Const ESPNowRateMCS9Short:Int = 34

Rem
bbdoc: Selects Espressif long-range mode at 250 Kbps.
End Rem
Const ESPNowRateLongRange250Kbps:Int = 35

Rem
bbdoc: Selects Espressif long-range mode at 500 Kbps.
End Rem
Const ESPNowRateLongRange500Kbps:Int = 36

Rem
bbdoc: One packet copied from the native ESP-NOW receive queue.
about: Poll packets with ESPNowPollReceived. Every byte array belongs to the
returned value and remains valid after the next poll.
End Rem
Struct SESPNowReceivedPacket
	Rem
	bbdoc: Six-byte station MAC address of the sending device.
	End Rem
	Field sourceAddress:Byte[]

	Rem
	bbdoc: Six-byte destination address from the received frame.
	about: This is the local station address for unicast or the broadcast address
	for broadcast traffic.
	End Rem
	Field destinationAddress:Byte[]

	Rem
	bbdoc: Received signal strength in dBm.
	End Rem
	Field rssi:Int

	Rem
	bbdoc: Wi-Fi channel on which the frame was received.
	End Rem
	Field channel:Int

	Rem
	bbdoc: Secondary Wi-Fi channel, using the ESP32.Network.WiFi second-channel constants.
	End Rem
	Field secondaryChannel:Int

	Rem
	bbdoc: Native PHY rate code reported by the radio for this received frame.
	End Rem
	Field rate:Int

	Rem
	bbdoc: Radio noise floor in dBm.
	End Rem
	Field noiseFloor:Int

	Rem
	bbdoc: Radio receive timestamp in microseconds, wrapping at the UInt range.
	about: ESP-IDF notes that this timestamp is only precise while modem sleep and
	light sleep are disabled.
	End Rem
	Field timestamp:UInt

	Rem
	bbdoc: Application payload bytes, without ESP-NOW framing.
	End Rem
	Field data:Byte[]
End Struct

Rem
bbdoc: Completion report for one queued ESP-NOW send request.
about: A successful report confirms MAC-layer delivery, not that the peer
application processed or retained the payload.
End Rem
Struct SESPNowDeliveryReport
	Rem
	bbdoc: Six-byte destination address supplied to ESPNowSend.
	End Rem
	Field destinationAddress:Byte[]

	Rem
	bbdoc: Six-byte local source address used for the transmission.
	End Rem
	Field sourceAddress:Byte[]

	Rem
	bbdoc: Wi-Fi interface used for transmission.
	End Rem
	Field interfaceIndex:Int

	Rem
	bbdoc: Native PHY rate code used for transmission.
	End Rem
	Field rate:Int

	Rem
	bbdoc: True when the peer acknowledged the frame at the MAC layer.
	End Rem
	Field delivered:Int
End Struct

Rem
bbdoc: Configuration retained for one unicast ESP-NOW peer.
about: The local master key is sensitive configuration data. Avoid printing or
persisting it unless the application has a deliberate security policy.
End Rem
Struct SESPNowPeerInfo
	Rem
	bbdoc: Six-byte station or SoftAP MAC address of the peer.
	End Rem
	Field address:Byte[]

	Rem
	bbdoc: Sixteen-byte local master key, zero-filled for an unencrypted peer.
	End Rem
	Field localMasterKey:Byte[]

	Rem
	bbdoc: Peer channel, or zero to follow the selected Wi-Fi interface.
	End Rem
	Field channel:Int

	Rem
	bbdoc: ESPNowInterfaceStation or ESPNowInterfaceAccessPoint.
	End Rem
	Field interfaceIndex:Int

	Rem
	bbdoc: True when peer traffic is protected with the local master key.
	End Rem
	Field encrypted:Int
End Struct

Rem
bbdoc: Per-peer ESP-NOW PHY transmission configuration.
End Rem
Struct SESPNowRateConfig
	Rem
	bbdoc: One of the ESPNowPHYMode constants.
	End Rem
	Field phyMode:Int

	Rem
	bbdoc: One of the ESPNowRate constants compatible with phyMode.
	End Rem
	Field rate:Int

	Rem
	bbdoc: Enables ERSU extended-range transmission when supported by the target.
	End Rem
	Field extendedRange:Int

	Rem
	bbdoc: Enables dual-carrier modulation when supported by the selected PHY mode.
	End Rem
	Field dualCarrierModulation:Int
End Struct

Private

Extern "C"
	Function _ESPNowInitialize:Int(receiveCapacity:Int, deliveryCapacity:Int, maximumReceivedPayload:Int) = "bmx_esp32_espnow_initialize"
	Function _ESPNowDeinitialize:Int() = "bmx_esp32_espnow_deinitialize"
	Function _ESPNowInitialized:Int() = "bmx_esp32_espnow_initialized"
	Function _ESPNowResultName:String(result:Int) = "bmx_esp32_espnow_result_name"
	Function _ESPNowVersion:Int(version:UInt Var) = "bmx_esp32_espnow_version"
	Function _ESPNowLocalAddress:Int(address:Byte Ptr) = "bmx_esp32_espnow_local_address"
	Function _ESPNowSetChannel:Int(channel:Int) = "bmx_esp32_espnow_set_channel"
	Function _ESPNowGetChannel:Int(channel:Int Var) = "bmx_esp32_espnow_get_channel"
	Function _ESPNowSetPrimaryMasterKey:Int(key:Byte Ptr, length:Int) = "bmx_esp32_espnow_set_primary_master_key"
	Function _ESPNowAddPeer:Int(address:Byte Ptr, channel:Int, key:Byte Ptr, keyLength:Int, ..
		interfaceIndex:Int) = "bmx_esp32_espnow_add_peer"
	Function _ESPNowDeletePeer:Int(address:Byte Ptr) = "bmx_esp32_espnow_delete_peer"
	Function _ESPNowPeerExists:Int(address:Byte Ptr) = "bmx_esp32_espnow_peer_exists"
	Function _ESPNowPeerCounts:Int(total:Int Var, encrypted:Int Var) = "bmx_esp32_espnow_peer_counts"
	Function _ESPNowGetPeer:Int(peerAddress:Byte Ptr, address:Byte Ptr, key:Byte Ptr, channel:Int Var, ..
		interfaceIndex:Int Var, encrypted:Int Var) = "bmx_esp32_espnow_get_peer"
	Function _ESPNowFetchPeer:Int(fromBeginning:Int, address:Byte Ptr, key:Byte Ptr, channel:Int Var, ..
		interfaceIndex:Int Var, encrypted:Int Var) = "bmx_esp32_espnow_fetch_peer"
	Function _ESPNowSetPeerRate:Int(address:Byte Ptr, phyMode:Int, rate:Int, extendedRange:Int, ..
		dualCarrierModulation:Int) = "bmx_esp32_espnow_set_peer_rate"
	Function _ESPNowSetWakeWindow:Int(windowMilliseconds:Int) = "bmx_esp32_espnow_set_wake_window"
	Function _ESPNowSend:Int(address:Byte Ptr, data:Byte Ptr, length:Int) = "bmx_esp32_espnow_send"
	Function _ESPNowTakeReceived:Int(sourceAddress:Byte Ptr, destinationAddress:Byte Ptr, ..
		rssi:Int Var, channel:Int Var, secondaryChannel:Int Var, rate:Int Var, noiseFloor:Int Var, ..
		timestamp:UInt Var, data:Byte Ptr, capacity:Int, length:Int Var) = "bmx_esp32_espnow_take_received"
	Function _ESPNowTakeDelivery:Int(destinationAddress:Byte Ptr, sourceAddress:Byte Ptr, ..
		interfaceIndex:Int Var, rate:Int Var, delivered:Int Var) = "bmx_esp32_espnow_take_delivery"
	Function _ESPNowDroppedReceived:UInt() = "bmx_esp32_espnow_dropped_received"
	Function _ESPNowDroppedDeliveries:UInt() = "bmx_esp32_espnow_dropped_deliveries"
End Extern

Function _ESPNowValidAddress:Int(address:Byte[])
	Return address And address.length = ESPNowAddressLength
End Function

Function _ESPNowReadPeer:Int(peerAddress:Byte[], fromBeginning:Int, fetch:Int, info:SESPNowPeerInfo Var)
	Local address:Byte[ESPNowAddressLength]
	Local key:Byte[ESPNowKeyLength]
	Local result:Int
	If fetch Then
		result = _ESPNowFetchPeer(fromBeginning, address, key, info.channel, info.interfaceIndex, info.encrypted)
	Else
		result = _ESPNowGetPeer(peerAddress, address, key, info.channel, info.interfaceIndex, info.encrypted)
	End If
	If result <> 0 Then Return result
	info.address = New Byte[ESPNowAddressLength]
	info.localMasterKey = New Byte[ESPNowKeyLength]
	MemCopy(info.address, address, ESPNowAddressLength)
	MemCopy(info.localMasterKey, key, ESPNowKeyLength)
	Return result
End Function

Public

Rem
bbdoc: Initializes ESP-NOW and its bounded native event queues.
param: Number of complete received packets to retain, from 1 to ESPNowMaximumQueueCapacity.
param: Number of send-delivery reports to retain, from 1 to ESPNowMaximumQueueCapacity.
param: Largest received payload retained per queue slot, from 1 to ESPNowMaximumPayloadV2.
returns: ESP_OK on success or an ESP-IDF error code.
about: Embedded.Network.WiFi must already be initialized. The receive payload
storage is reserved eagerly and consumes approximately `receiveQueueCapacity *
maximumReceivedPayload` bytes plus queue metadata. Calling this function again
while ESP-NOW is active succeeds without changing the existing configuration.
End Rem
Function ESPNowInitialize:Int(receiveQueueCapacity:Int = ESPNowDefaultReceiveQueueCapacity, ..
		deliveryQueueCapacity:Int = ESPNowDefaultDeliveryQueueCapacity, ..
		maximumReceivedPayload:Int = ESPNowMaximumPayload)
	Return _ESPNowInitialize(receiveQueueCapacity, deliveryQueueCapacity, maximumReceivedPayload)
End Function

Rem
bbdoc: Stops ESP-NOW, unregisters its native callbacks, and releases its queues.
returns: ESP_OK on success or an ESP-IDF error code.
about: Stop ESP-NOW before deinitializing Embedded.Network.WiFi. Calling this
function when ESP-NOW is already stopped succeeds.
End Rem
Function ESPNowDeinitialize:Int()
	Return _ESPNowDeinitialize()
End Function

Rem
bbdoc: Reports whether ESP-NOW is initialized.
returns: True while ESP-NOW is active.
End Rem
Function ESPNowInitialized:Int()
	Return _ESPNowInitialized()
End Function

Rem
bbdoc: Returns the symbolic ESP-IDF name of a result code.
param: Result code returned by an ESP-NOW or Wi-Fi operation.
returns: A name such as `ESP_OK`, `ESP_ERR_INVALID_ARG`, or `ESP_ERR_ESPNOW_NOT_INIT`.
End Rem
Function ESPNowResultName:String(result:Int)
	Return _ESPNowResultName(result)
End Function

Rem
bbdoc: Queries the ESP-NOW protocol version implemented by ESP-IDF.
param: Receives the native version number.
returns: ESP_OK on success or an ESP-IDF error code.
End Rem
Function ESPNowVersion:Int(version:UInt Var)
	Return _ESPNowVersion(version)
End Function

Rem
bbdoc: Reads the local Wi-Fi station address used by ESP-NOW.
param: Six-byte array that receives the station MAC address.
returns: ESP_OK on success or an ESP-IDF error code.
End Rem
Function ESPNowLocalAddress:Int(address:Byte[])
	If Not _ESPNowValidAddress(address) Then Return -5
	Return _ESPNowLocalAddress(address)
End Function

Rem
bbdoc: Selects the primary Wi-Fi channel used by ESP-NOW.
param: Channel number from 1 to 14, subject to the configured Wi-Fi country.
returns: ESP_OK on success or an ESP-IDF error code.
about: Every communicating peer must use the same channel. A station connected
to an access point follows that access point's channel, so do not move it to an
unrelated channel while connected.
End Rem
Function ESPNowSetChannel:Int(channel:Int)
	Return _ESPNowSetChannel(channel)
End Function

Rem
bbdoc: Reads the current primary Wi-Fi channel.
param: Receives the channel number.
returns: ESP_OK on success or an ESP-IDF error code.
End Rem
Function ESPNowGetChannel:Int(channel:Int Var)
	Return _ESPNowGetChannel(channel)
End Function

Rem
bbdoc: Sets the 16-byte primary master key used to protect peer LMKs.
param: Exactly ESPNowKeyLength bytes shared by the communicating devices.
returns: ESP_OK on success or an ESP-IDF error code.
about: Set the PMK before registering encrypted peers. The PMK does not itself
encrypt application frames; each encrypted peer also requires a matching LMK.
End Rem
Function ESPNowSetPrimaryMasterKey:Int(key:Byte[])
	If Not key Or key.length <> ESPNowKeyLength Then Return -5
	Return _ESPNowSetPrimaryMasterKey(key, key.length)
End Function

Rem
bbdoc: Adds or updates one unicast ESP-NOW peer.
param: Six-byte station MAC address of the peer.
param: Peer channel from 1 to 14, or zero to use the local current channel.
param: Optional 16-byte LMK; Null registers an unencrypted peer.
param: Wi-Fi interface used by the peer, normally ESPNowInterfaceStation.
returns: ESP_OK on success or an ESP-IDF error code.
about: If the address is already registered, its channel and encryption
configuration are replaced. Both ends of an encrypted link must use the same
PMK and the same per-peer LMK.
End Rem
Function ESPNowAddPeer:Int(address:Byte[], channel:Int = 0, localMasterKey:Byte[] = Null, ..
		interfaceIndex:Int = ESPNowInterfaceStation)
	If Not _ESPNowValidAddress(address) Then Return -5
	If localMasterKey And localMasterKey.length <> ESPNowKeyLength Then Return -5
	If localMasterKey Then Return _ESPNowAddPeer(address, channel, localMasterKey, ..
		localMasterKey.length, interfaceIndex)
	Return _ESPNowAddPeer(address, channel, Null, 0, interfaceIndex)
End Function

Rem
bbdoc: Adds or updates the special unencrypted broadcast peer.
param: Broadcast channel from 1 to 14, or zero to use the local current channel.
returns: ESP_OK on success or an ESP-IDF error code.
about: Register the broadcast peer before calling ESPNowBroadcast. ESP-NOW
broadcast traffic cannot use the peer encryption mechanism.
End Rem
Function ESPNowAddBroadcastPeer:Int(channel:Int = 0)
	Local address:Byte[] = ESPNowParseAddress(ESPNowBroadcastAddress)
	Return ESPNowAddPeer(address, channel)
End Function

Rem
bbdoc: Removes a registered ESP-NOW peer.
param: Six-byte peer address, including the broadcast address when applicable.
returns: ESP_OK on success or an ESP-IDF error code.
End Rem
Function ESPNowDeletePeer:Int(address:Byte[])
	If Not _ESPNowValidAddress(address) Then Return -5
	Return _ESPNowDeletePeer(address)
End Function

Rem
bbdoc: Tests whether an address is registered as an ESP-NOW peer.
param: Six-byte peer address.
returns: True when the peer exists; False for an invalid address or absent peer.
End Rem
Function ESPNowPeerExists:Int(address:Byte[])
	If Not _ESPNowValidAddress(address) Then Return False
	Return _ESPNowPeerExists(address)
End Function

Rem
bbdoc: Reads the number of registered peers.
param: Receives the total peer count, including a registered broadcast peer.
param: Receives the number of encrypted unicast peers.
returns: ESP_OK on success or an ESP-IDF error code.
End Rem
Function ESPNowPeerCounts:Int(total:Int Var, encrypted:Int Var)
	Return _ESPNowPeerCounts(total, encrypted)
End Function

Rem
bbdoc: Reads the retained configuration for a registered peer.
param: Six-byte unicast or broadcast peer address.
param: Receives the peer configuration.
returns: ESP_OK on success or an ESP-IDF error code.
End Rem
Function ESPNowGetPeer:Int(address:Byte[], info:SESPNowPeerInfo Var)
	If Not _ESPNowValidAddress(address) Then Return -5
	Return _ESPNowReadPeer(address, False, False, info)
End Function

Rem
bbdoc: Begins enumeration of registered unicast peers.
param: Receives the first peer configuration.
returns: ESP_OK on success or ESP_ERR_ESPNOW_NOT_FOUND when no unicast peer exists.
about: ESP-IDF deliberately omits the broadcast peer from enumeration. Continue
with ESPNowNextPeer until it returns a non-zero result.
End Rem
Function ESPNowFirstPeer:Int(info:SESPNowPeerInfo Var)
	Return _ESPNowReadPeer(Null, True, True, info)
End Function

Rem
bbdoc: Continues enumeration of registered unicast peers.
param: Receives the next peer configuration.
returns: ESP_OK on success or ESP_ERR_ESPNOW_NOT_FOUND after the final peer.
about: Call ESPNowFirstPeer before starting a new enumeration pass.
End Rem
Function ESPNowNextPeer:Int(info:SESPNowPeerInfo Var)
	Return _ESPNowReadPeer(Null, False, True, info)
End Function

Rem
bbdoc: Selects the PHY transmission configuration for one registered peer.
param: Six-byte address of an existing peer.
param: PHY mode, rate and optional 802.11ax transmission features.
returns: ESP_OK on success or an ESP-IDF error code.
about: Mode and rate support varies by ESP32 family. Configure the peer first;
ESP-IDF rejects combinations not supported by the target radio.
End Rem
Function ESPNowSetPeerRate:Int(address:Byte[], config:SESPNowRateConfig Var)
	If Not _ESPNowValidAddress(address) Then Return -5
	Return _ESPNowSetPeerRate(address, config.phyMode, config.rate, config.extendedRange, ..
		config.dualCarrierModulation)
End Function

Rem
bbdoc: Sets how long the station radio remains awake for ESP-NOW reception.
param: Receive window in milliseconds from zero through 65535.
returns: ESP_OK on success or an ESP-IDF error code.
about: The default maximum window keeps reception continuously available. For
periodic power saving, also set WiFiSetConnectionlessWakeInterval and enable an
appropriate ESP32 Wi-Fi modem-sleep mode.
End Rem
Function ESPNowSetWakeWindow:Int(windowMilliseconds:Int)
	Return _ESPNowSetWakeWindow(windowMilliseconds)
End Function

Rem
bbdoc: Queues one payload for transmission to a registered peer.
param: Six-byte unicast or broadcast destination address.
param: Payload of at most ESPNowMaximumPayload bytes.
returns: ESP_OK when ESP-IDF accepts the request, or an ESP-IDF error code.
about: Acceptance does not mean delivery. Poll ESPNowPollDelivery for the later
MAC-layer result. Wait for that report before repeatedly sending large packets
to preserve report ordering and avoid overloading the Wi-Fi task.
End Rem
Function ESPNowSend:Int(address:Byte[], data:Byte[])
	If Not _ESPNowValidAddress(address) Or Not data Or data.length > ESPNowMaximumPayload Then Return -5
	Return _ESPNowSend(address, data, data.length)
End Function

Rem
bbdoc: Queues one payload for transmission to the registered broadcast peer.
param: Payload of at most ESPNowMaximumPayload bytes.
returns: ESP_OK when ESP-IDF accepts the request, or an ESP-IDF error code.
End Rem
Function ESPNowBroadcast:Int(data:Byte[])
	Return ESPNowSend(ESPNowParseAddress(ESPNowBroadcastAddress), data)
End Function

Rem
bbdoc: Queues one payload for every registered peer.
param: Payload of at most ESPNowMaximumPayload bytes.
returns: ESP_OK when ESP-IDF accepts the requests, or an ESP-IDF error code.
about: This uses ESP-IDF's null-destination operation and is distinct from a
single broadcast frame. Poll one delivery report for each resulting unicast
transmission. The registered broadcast peer, when present, may also receive a
broadcast transmission as part of the operation.
End Rem
Function ESPNowSendToAll:Int(data:Byte[])
	If Not data Or data.length > ESPNowMaximumPayload Then Return -5
	Return _ESPNowSend(Null, data, data.length)
End Function

Rem
bbdoc: Removes the oldest packet from the native receive queue.
param: Receives the packet when one is available.
returns: True when a packet was returned; False when the queue is empty.
about: Native Wi-Fi callbacks never enter managed BlitzMax code. They copy
frames into the bounded queue, and this poll creates the managed arrays later
on the application thread.
End Rem
Function ESPNowPollReceived:Int(packet:SESPNowReceivedPacket Var)
	Local sourceAddress:Byte[ESPNowAddressLength]
	Local destinationAddress:Byte[ESPNowAddressLength]
	Local payload:Byte[ESPNowMaximumPayload]
	Local length:Int
	If Not _ESPNowTakeReceived(sourceAddress, destinationAddress, packet.rssi, packet.channel, ..
			packet.secondaryChannel, packet.rate, packet.noiseFloor, packet.timestamp, ..
			payload, payload.length, length) Then Return False
	packet.sourceAddress = New Byte[ESPNowAddressLength]
	packet.destinationAddress = New Byte[ESPNowAddressLength]
	packet.data = New Byte[length]
	MemCopy(Varptr packet.sourceAddress[0], Varptr sourceAddress[0], Size_T(sourceAddress.length))
	MemCopy(Varptr packet.destinationAddress[0], Varptr destinationAddress[0], Size_T(destinationAddress.length))
	If length Then MemCopy(Varptr packet.data[0], Varptr payload[0], Size_T(length))
	Return True
End Function

Rem
bbdoc: Removes the oldest send result from the native delivery queue.
param: Receives the delivery report when one is available.
returns: True when a report was returned; False when the queue is empty.
End Rem
Function ESPNowPollDelivery:Int(report:SESPNowDeliveryReport Var)
	Local destinationAddress:Byte[ESPNowAddressLength]
	Local sourceAddress:Byte[ESPNowAddressLength]
	If Not _ESPNowTakeDelivery(destinationAddress, sourceAddress, report.interfaceIndex, ..
			report.rate, report.delivered) Then Return False
	report.destinationAddress = New Byte[ESPNowAddressLength]
	report.sourceAddress = New Byte[ESPNowAddressLength]
	MemCopy(Varptr report.destinationAddress[0], Varptr destinationAddress[0], Size_T(destinationAddress.length))
	MemCopy(Varptr report.sourceAddress[0], Varptr sourceAddress[0], Size_T(sourceAddress.length))
	Return True
End Function

Rem
bbdoc: Returns the cumulative number of received packets discarded locally.
returns: Number dropped since ESPNowInitialize, saturating at the UInt maximum.
about: Packets are dropped when the receive queue is full or their payload is
larger than the configured maximum received payload.
End Rem
Function ESPNowDroppedReceived:UInt()
	Return _ESPNowDroppedReceived()
End Function

Rem
bbdoc: Returns the cumulative number of send reports discarded locally.
returns: Number dropped since ESPNowInitialize, saturating at the UInt maximum.
about: Increase the delivery queue capacity or poll more frequently if this is
non-zero.
End Rem
Function ESPNowDroppedDeliveries:UInt()
	Return _ESPNowDroppedDeliveries()
End Function

Rem
bbdoc: Formats a six-byte address as lower-case colon-separated hexadecimal.
param: Six-byte MAC address.
returns: Text such as `24:6f:28:12:34:56`, or an empty string for an invalid array.
End Rem
Function ESPNowFormatAddress:String(address:Byte[])
	If Not _ESPNowValidAddress(address) Then Return ""
	Local result:String
	For Local index:Int = 0 Until address.length
		If index Then result :+ ":"
		Local value:Int = address[index] & $ff
		result :+ Chr(_ESPNowHexCharacter(value Shr 4))
		result :+ Chr(_ESPNowHexCharacter(value & $f))
	Next
	Return result.ToLower()
End Function

Rem
bbdoc: Parses a hexadecimal ESP-NOW address.
param: Twelve hexadecimal digits, optionally separated by colons or hyphens.
returns: A six-byte address, or Null when the text is invalid.
End Rem
Function ESPNowParseAddress:Byte[](value:String)
	Local compact:String = value.Replace(":", "").Replace("-", "")
	If compact.length <> ESPNowAddressLength * 2 Then Return Null
	Local address:Byte[ESPNowAddressLength]
	For Local index:Int = 0 Until ESPNowAddressLength
		Local high:Int = _ESPNowHexDigit(compact[index * 2])
		Local low:Int = _ESPNowHexDigit(compact[index * 2 + 1])
		If high < 0 Or low < 0 Then Return Null
		address[index] = Byte((high Shl 4) | low)
	Next
	Return address
End Function

Private

Function _ESPNowHexDigit:Int(value:Int)
	If value >= Asc("0") And value <= Asc("9") Then Return value - Asc("0")
	If value >= Asc("a") And value <= Asc("f") Then Return value - Asc("a") + 10
	If value >= Asc("A") And value <= Asc("F") Then Return value - Asc("A") + 10
	Return -1
End Function

Function _ESPNowHexCharacter:Int(value:Int)
	If value < 10 Then Return Asc("0") + value
	Return Asc("a") + value - 10
End Function
?
