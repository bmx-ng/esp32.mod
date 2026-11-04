SuperStrict

Framework BRL.StandardIO
Import "adc_pwm_common.bmx"

' GPIO32 is an ADC1 input. GPIO25 and GPIO26 are output-capable DAC/ADC pins
' that are otherwise uncommitted on the module.
RunADCPWMTests(32, 25, 26)
