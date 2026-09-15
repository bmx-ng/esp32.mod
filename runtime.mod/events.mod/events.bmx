' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: ESP32-facing deferred native-event services.
about: The portable implementation lives in Embedded.Runtime.Events. These
names provide the corresponding ESP32 module layer for target-specific code.
End Rem
Module ESP32.Runtime.Events
?esp32

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Runtime.Events

Const ESP32EventSourceCapacity:UInt = EmbeddedEventSourceCapacity

Rem
bbdoc: Registers a managed object as a deferred ESP32 event source.
End Rem
Function RegisterESP32EventSource:UInt(source:Object, eventId:Int, persistent:Int = False)
	Return RegisterEmbeddedEventSource(source, eventId, persistent)
End Function

Rem
bbdoc: Releases a previously registered deferred ESP32 event source.
End Rem
Function ReleaseESP32EventSource:Int(token:UInt)
	Return ReleaseEmbeddedEventSource(token)
End Function

Rem
bbdoc: Returns the number of native ESP32 events waiting to be dispatched.
End Rem
Function ESP32DeferredEventPending:UInt()
	Return EmbeddedDeferredEventPending()
End Function

Rem
bbdoc: Returns the number of ESP32 events discarded because the queue was full.
End Rem
Function ESP32DeferredEventDropped:UInt()
	Return EmbeddedDeferredEventDropped()
End Function
?
