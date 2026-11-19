# Zigbee hardware validation

This is the dated hardware test record for [the Zigbee developer guide](zigbee-core.md).

## Hardware checks on 18 November 2026

The Door Lock, Window Covering, IAS Zone, and long-string additions compile for
both the C6 `zczr` and H2 `zed` example builds. On the C6,
`zigbee_extended_smoke` started with all three server layouts, updated their
local state attributes, and read back the lock state (2), covering lift (42%),
and IAS Zone status (1). It registered a 72-byte long string, read back its
78-byte local update, and read back an 80-byte long octet value through the
Zigbee event path. The same firmware wrote a 79-byte string and 80-byte
octets through Zigbee and read both back with successful responses.

IAS Zone was also exercised across both physical boards: the H2 joined the C6,
sent an enrollment request, accepted zone ID 3, and updated its status to
Alarm1 plus Tamper. The C6 read the H2's enrolled state (1) and zone ID (3),
and received a status notification with bitmap 5 and zone ID 3. Door Lock,
Window Covering, and long custom values remain C6 loopback checks. A C6
custom cluster served array and structure values to the H2. The H2 wrote both
types and read their changed values back. A 153-byte encoded array also made
the round trip, exercising Zigbee fragmentation in both directions.

After the tests, the C6 light and H2 switch images were restored. The pair
regression passed with typed scalar, custom cluster, and binary octet checks.

- The H2 deep-sleep sensor joined the C6 network, stopped Zigbee cleanly,
  woke on its ten-second timer, and restored the saved network on consecutive
  wakes. The UART bridge remained observable while native USB did not. The
  `tests/zigbee_deep_sleep.py --uart` check passed.
- The C6 exposed a heating-only Thermostat server on endpoint 12. The H2
  discovered it, wrote the occupied heating setpoint to 21.50 °C, and read
  back both that setpoint and the 23.00 °C local temperature. The full pair
  regression passed with `--require-thermostat` and every earlier check.
- The C6 exposed x/y Color Control on its light endpoint. The H2 discovered
  it, sent Move To Color for (20000, 30000), and read both coordinates back.
  The full pair regression passed with `--require-color` and all earlier checks.
- The C6 exposed Level Control on its light endpoint. The H2 discovered the
  cluster, sent Move To Level With On/Off, and read back CurrentLevel 180.
  The full pair regression passed with `--require-level` and all earlier checks.
- The C6 exposed Electrical Measurement with 230 V, 250 mA (current divisor
  1000), and 58 to 60 W. The H2 discovered the cluster, read all three values
  and the divisor, and received a changed active-power report. The full pair
  regression passed with `--require-electrical` alongside all earlier checks.
- The C6 exposed Power Configuration battery voltage and percentage on its
  sensor endpoint. The H2 discovered the cluster, read 3.0 V and 74 to 75
  percent, and received a changed percentage report. The full pair regression
  passed with `--require-battery` alongside all earlier checks.
- The C6 exposed Pressure Measurement (0x0403) and Occupancy Sensing (0x0406)
  on endpoint 12. The H2 discovered both, read pressure as `Int16` and
  occupancy as `Bitmap8`, and received changed reports. A custom octet-string
  attribute carried the binary sequence `01 00 FE FF` through a remote write
  and readback; the light's local setter restored `00 80 FF`. The full
  two-board regression passed with `--require-pressure --require-occupancy
  --require-octets` alongside all earlier checks.
- With a fresh Zigbee storage partition on the H2 end device, the C6
  coordinator accepted its provisioned 18-byte install code and the H2 joined
  and sent bound On/Off toggles. The coordinator returned success from both
  `ZigbeeAddJoinerInstallCode` and `ZigbeeRequireInstallCodes`.
- With the same coordinator requiring install codes, an H2 flashed without
  install-code setup and with fresh Zigbee storage reported `joined=0` on six
  consecutive network checks. This verified the required-code policy blocked
  ordinary joining in the test setup.
- After restoring the normal examples and clearing saved Zigbee network state
  on both boards, the C6 and H2 formed and joined an ordinary network. The
  full `tests/zigbee_core_pair.py` regression passed, including On/Off,
  reporting, groups, custom clusters and profiles, UTF-8 strings, and
  temperature, humidity, and illuminance readings and reports.
- The H2 sleepy switch joined its saved network and sent two bound On/Off
  toggles 15 seconds apart. The C6 coordinator/light observed both state
  changes, from Off to On and back to Off, while the switch continued to
  receive reports. Both the normal and sleepy end-device examples built with
  the new `zed` power-management configuration.

## Hardware checks on 17 November 2026

- The H2 coordinator/light and C6 end-device/switch pair toggled both states
  and read back the remote On/Off attribute through the public API.
- The H2 completed two start/stop cycles without rebooting.
- Both reverse role builds succeeded: H2 with `zed`, C6 with `zczr`. After
  clearing only their Zigbee storage partitions, the C6 coordinator/light and
  H2 end-device/switch pair toggled both states and read back the attribute.
- The H2 switch subsequently used match descriptor discovery and binding to
  toggle the C6 light through the binding table, then read back its state.
- The C6 light built and flashed with a local attribute initialization call.
  The two-board bound-toggle and readback regression still passed.
- After the C6 light bound its server endpoint back to the H2 switch,
  `tests/zigbee_core_pair.py --require-report` passed with a received On/Off
  report as well as toggles and a read response.
- `tests/zigbee_core_pair.py --require-report --require-group` passed after the
  H2 switch added the C6 light to a group and sent a group-addressed toggle.
- `tests/zigbee_core_pair.py --require-report --require-group --require-scalar
  --require-write` passed after the H2 switch read the C6 light's 16-bit
  cluster revision, wrote its 16-bit IdentifyTime attribute, and read back
  the changed value.
- The same pair test passed after the switch moved to general reporting,
  discovery, and binding calls.
- With the C6 light exposing endpoints 10 and 11, the H2 switch discovered
  both. `tests/zigbee_core_pair.py --require-extra-endpoint` passed alongside
  the reporting, group, scalar read, and write checks.
- With custom cluster 0xFC00 on endpoint 11, the H2 switch discovered the
  cluster and wrote then read back the 16-bit value 43.
- The C6 application then updated that local custom attribute to 44;
  `tests/zigbee_core_pair.py --require-local-update` confirmed the H2 read
  back 44 while the other pair checks still passed.

## Hardware check on 18 November 2026

- The C6 coordinator/light exposed a temperature sensor on endpoint 12 and
  set its measured value to 23.45 °C. The H2 end-device/switch discovered the
  Temperature Measurement server and read attribute zero as a signed 16-bit
  value of 2345. `tests/zigbee_core_pair.py --require-temperature` passed
  alongside the reporting, group, scalar, write, extra endpoint, custom
  cluster, and local update checks.
- The C6 bound its temperature server to the H2 client, which configured
  reporting. The H2 received a signed 16-bit report after the simulated
  measurement changed to 23.55 °C.
- With an optional Relative Humidity Measurement server on endpoint 12, the
  H2 discovered cluster 0x0405, read 45.50% as an unsigned 16-bit value, and
  received a report when the C6 changed its simulated value to 45.60%.
  `tests/zigbee_core_pair.py --require-humidity` passed alongside all earlier
  pair checks.
- The H2 queried group 4660 with View Group and requested the light endpoint's
  full membership list. Both responses confirmed membership; the list response
  reported one group and 15 remaining slots. The full pair check passed with
  `--require-group-query` alongside the earlier checks.
- The C6 registered manufacturer profile `0xC0DE` on endpoint 13 with custom
  cluster `0xFC01` and a 16-bit value of 77. The H2 registered a matching client
  endpoint under the same profile, discovered the server, and read back 77.
  `tests/zigbee_core_pair.py --require-custom-profile` passed alongside all
  earlier pair checks.
- The C6 custom cluster exposed a short string attribute. The H2 wrote
  `Zigbee é`, read back its nine UTF-8 bytes, then read `ready` after the C6
  changed the attribute locally. The full pair check passed with
  `--require-string` alongside all earlier checks.
- The H2 sent Remove All Groups to the C6 light, then queried membership and
  received a list with zero groups. The full pair check passed with
  `--require-group-clear` alongside all earlier checks.
- The C6 exposed Illuminance Measurement on its sensor endpoint. The H2
  discovered it, read encoded value 20001, and received a changed report of
  20101. The full pair check passed with `--require-illuminance` alongside
  all earlier checks.
