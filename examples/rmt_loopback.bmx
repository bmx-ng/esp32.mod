SuperStrict

Framework BRL.StandardIO
Import "rmt_loopback_common.bmx"

' Connect GPIO4 (TX) to GPIO5 (RX) with a short jumper wire.
RunRMTLoopback(4, 5)
