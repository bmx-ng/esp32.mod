SuperStrict

Framework BRL.StandardIO
Import "rmt_loopback_common.bmx"

' Connect D3/GPIO21 (TX) to D4/GPIO22 (RX) on the XIAO ESP32-C6.
RunRMTLoopback(21, 22)
