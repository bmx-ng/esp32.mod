# ESP32 board profiles

Each directory is one board profile. bmk loads these files only when targeting
ESP32; applications obtain the selected defaults through the normal ESP32 and
Embedded APIs.

`board.ini` uses the versioned `format=1` schema. Its stable sections are:

- `[board]`: display name, vendor, profile kind, aliases, ESP-IDF target,
  module, and documentation links.
- `[build]`: flash, PSRAM, optional `sdkconfig_defaults`, and optional partition
  table settings.
- `[console]`: the default console transport.
- `[bus.<type>.<name>]`: named I2C, SPI, or UART defaults.
- `[resource.<name>]`: onboard LEDs, buttons, storage, connectors, and similar
  developer-visible facilities.
- `[constraint.<name>]`: occupied, shared, strapping, input-only, or otherwise
  noteworthy pins.

`pins.csv` is optional and has exactly four columns:

```text
name,gpio,connector,position
```

Names are stable symbolic references used by buses, resources, constraints,
and the generated build header. `connector` and `position` describe the
physical board location when the manufacturer publishes an unambiguous map.

Build-side files such as `sdkconfig.defaults` and partition CSVs belong in the
same profile directory and are referenced by safe relative paths from
`board.ini`.

A `partitions` entry in `[build]` selects the board's partition CSV. Persistent
internal storage uses a data partition named `storage` with subtype
`littlefs`. An SD resource names its controller and signal pins; this is board
wiring metadata, not evidence that a card is present or formatted.

An OTA-capable table has an `otadata` data partition and at least two `ota_n`
application slots. Application slots must be large enough for the complete
firmware image. Enabling rollback in `sdkconfig.defaults` makes a newly booted
image provisional until it explicitly confirms that it is working.

Use `bmk boardinfo -l esp32 -board <profile>` to inspect the resolved data.
Extra board roots can be configured with `esp32.board.dirs` in `custom.bmk` or
the platform path-list environment variable `ESP32_BOARD_DIRS`. A custom
profile name or alias must not collide with an installed profile.

Generic profiles describe ESP-IDF SoC targets, not particular retail boards.
Their initial target set follows the current MicroPython ESP32 port catalogue;
specific board metadata should be added only from reliable board or module
documentation. A matching chip and flash size is not sufficient to infer a
retail board automatically.
