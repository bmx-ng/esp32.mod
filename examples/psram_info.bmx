SuperStrict

Import BRL.StandardIO
Import ESP32.Hardware.PSRAM
Import ESP32.Runtime

Print "PSRAM available: " + PSRAMAvailable()
Print "PSRAM capacity: " + PSRAMCapacity()
Print "PSRAM free: " + PSRAMFree()
Print "PSRAM largest free block: " + PSRAMLargestFreeBlock()
Print "Managed arena reserved: " + ManagedArenaReserved()
Print "Managed arena in PSRAM: " + ManagedArenaInPSRAM()

If ManagedArenaInPSRAM() And Not PSRAMAvailable() Then RuntimeError "managed arena reports unavailable PSRAM"
If Not ManagedArenaValid() Then RuntimeError "managed arena placement is invalid"
