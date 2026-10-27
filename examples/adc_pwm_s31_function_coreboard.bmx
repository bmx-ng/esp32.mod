SuperStrict

Framework BRL.StandardIO
Import "adc_pwm_common.bmx"

' GPIO42, GPIO43 and GPIO44 are free signals exposed on header J2.
RunADCPWMTests(42, 43, 44, ADCAttenuation0dB, ADCAttenuation0dB, ADCAttenuation6dB)
