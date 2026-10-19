SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.ADC
Import ESP32.Hardware.PWM
Import ESP32.Random

Function RunADCPWMTests(ADCPin:UInt, PWMPin:UInt, SecondPWMPin:UInt, ..
		DefaultAttenuation:Int = ADCAttenuation12dB, ..
		AlternateAttenuation:Int = ADCAttenuation6dB, UnsupportedAttenuation:Int = -1)
	Local raw:UInt
	Local millivolts:Int
	Local passed:Int = ADCIsValidPin(ADCPin) And ADCInitPin(ADCPin)
	ESP32RandomEnableInternalEntropy()
	passed :& Not ESP32RandomInternalEntropyEnabled()
	passed :& ADCUnitForPin(ADCPin) >= 0 And ADCChannelForPin(ADCPin) >= 0
	passed :& ADCGetAttenuation(ADCPin) = DefaultAttenuation
	passed :& ADCSetAttenuation(ADCPin, AlternateAttenuation)
	If UnsupportedAttenuation >= 0 Then
		passed :& Not ADCSetAttenuation(ADCPin, UnsupportedAttenuation)
		passed :& ADCGetAttenuation(ADCPin) = AlternateAttenuation
	End If
	passed :& ADCReadRaw(ADCPin, raw) And raw <= ADCMaximumValueForPin(ADCPin)
	If ADCIsCalibrationSupported() Then
		passed :& ADCReadMilliVolts(ADCPin, millivolts) And millivolts >= 0
	Else
		passed :& Not ADCReadMilliVolts(ADCPin, millivolts)
	End If
	passed :& ADCDeinitPin(ADCPin)
	ESP32RandomEnableInternalEntropy()
	passed :& ESP32RandomInternalEntropyEnabled() And Not ADCInitPin(ADCPin)
	ESP32RandomDisableInternalEntropy()

	Local achieved:UInt = PWMInitPin(PWMPin, 1000, PWMDutyMaximum / 2)
	passed :& achieved > 0 And PWMChannelForPin(PWMPin) >= 0
	passed :& PWMTimerForPin(PWMPin) >= 0 And PWMResolutionBitsForPin(PWMPin) > 0
	passed :& PWMSetPinDuty(PWMPin, PWMDutyMaximum / 4)
	passed :& PWMInitPin(SecondPWMPin, 1000, PWMDutyMaximum / 2) = achieved
	passed :& PWMTimerForPin(SecondPWMPin) = PWMTimerForPin(PWMPin)
	passed :& PWMSetPinFrequency(PWMPin, 2000) = 0
	passed :& PWMDeinitPin(SecondPWMPin)
	passed :& PWMSetPinFrequency(PWMPin, 2000) > 0
	passed :& PWMDeinitPin(PWMPin)

	If passed Then
		Print "ESP32 ADC and PWM checks passed"
	Else
		Print "ESP32 ADC and PWM checks failed"
	End If
End Function
