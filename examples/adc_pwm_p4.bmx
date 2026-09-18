SuperStrict

Framework BRL.StandardIO
Import "adc_pwm_common.bmx"

' ESP32-P4 ADC1 uses GPIO16–GPIO23; GPIO1 is not an ADC input.
RunADCPWMTests(16, 2, 3)
