SuperStrict

Framework BRL.StandardIO
Import "power_gpio_wake_common.bmx"

' GPIO4 is an exposed RTC-capable pin on the ESP32-WROOM-32D test board.
RunPowerGPIOWakeTest(4, "GPIO4")
