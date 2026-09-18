SuperStrict

Framework BRL.StandardIO
Import "rmt_loopback_common.bmx"

' Connect D3/GPIO7 (TX) to D4/GPIO23 (RX) with a short jumper wire.
RunRMTLoopback(7, 23)
