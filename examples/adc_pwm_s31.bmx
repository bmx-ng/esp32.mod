SuperStrict

Framework BRL.StandardIO
Import "adc_pwm_common.bmx"

' ESP32-S31 ADC1 starts at GPIO42. GPIO8 and GPIO9 are LCD data lines; the
' hardware test runs before an LCD driver is active and deinitializes both.
RunADCPWMTests(42, 8, 9, ADCAttenuation0dB, ADCAttenuation0dB, ADCAttenuation6dB)
