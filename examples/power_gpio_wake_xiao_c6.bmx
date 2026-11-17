SuperStrict

Framework BRL.StandardIO
Import "power_gpio_wake_common.bmx"

' The XIAO ESP32-C6 header labels RTC-capable GPIO0 as D0.
RunPowerGPIOWakeTest(0, "D0/GPIO0")
