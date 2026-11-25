# Power-management bench — November 20, 2026

Tested the connected player board `76c3cfc0` (base MAC
`44:b1:76:c3:cf:c0`) using the supplied PlayerBoardV9 and TimmonsLeePPSV2
schematics. USB supplied power; the user confirmed that **no battery was
connected**. The diagnostic completed all 72 measurement batches across 18
stages, including five low/high boost-control cycles, with no unexpected reset.

The original application was restored and its affected flash sectors were
verified byte for byte against the backup. No production source was changed.

## Measurements

These are ADC estimates using nominal divider values, not multimeter readings.
Each table entry is the mean of the recorded 64-conversion batch averages.
ADC calibration source was `ESP_ADC_CAL_VAL_EFUSE_TP_FIT` (3).

| Stage | Batches | GPIO47 | Load | USB VBUS (V) | Empty BAT terminal (V) |
| --- | ---: | --- | --- | ---: | ---: |
| Initial | 4 | Floating, reads low | Off | 5.051 | 4.149 |
| Boost requested | 4 | High | Off | 5.052 | 4.152 |
| Boost disabled | 4 | Low | Off | 5.049 | 4.150 |
| Dim | 4 | High | Dim | 5.043 | 4.142 |
| Bright | 4 | High | Bright | 4.977 | 4.098 |
| Bright, boost disabled | 4 | Low | Bright | 5.006 | 4.125 |
| Bright recovery | 4 | High | Bright | 4.974 | 4.119 |
| Five cycles, low phases | 20 | Low | Dim | 5.046 | 4.149 |
| Five cycles, high phases | 20 | High | Dim | 5.043 | 4.146 |
| Final | 4 | Floating, reads low | Off | 5.052 | 4.155 |

Off disables the display clock and sends zero RGB values. Dim commands display
brightness 1/15 and RGB white 4/255; bright commands 15/15 and RGB white 32/255
to eight pixels. Load current and physical light output were not measured.

The HT16K33 acknowledged I2C at 0x70 in **all 72 batches**, including every
GPIO47-low batch. GPIO47 readback matched every requested high/low state;
floating read low, consistent with the schematic's external 10 kohm pulldown.

## What the results establish

**Both sense inputs provide plausible, load-sensitive voltage estimates.**
GPIO1 is BAT_SENSE (100k/100k divider, multiply ADC millivolts by 2).
GPIO2 is VBUS_SENSE (100k top/150k bottom, multiply by 5/3). The brighter load
reduced the USB estimate by approximately 75 mV relative to high/off; the final
unloaded estimate recovered. Absolute voltage accuracy needs a meter reference.

**BAT_SENSE alone cannot establish battery presence.** Despite the empty battery
connector, its terminal estimate stayed between 4.086 and 4.166 V across batch
averages. This node is connected to TP4056 BAT and its output capacitor, so a
voltage-based presence or “full battery” decision would produce a false positive
in this setup. The manufacturer's datasheet explicitly describes operation with
a BAT capacitor and no battery attached. [TP4056 datasheet](https://www.toppwr.com/uploadfile/file/20240307/65e993d6379ea.pdf)

**The `5V_ENN` name has misleading polarity.** GPIO47 connects directly to MT3608
EN. High enables the converter; low disables switching. The 75k/10k feedback
divider targets approximately 5.10 V, calculated from the nominal 0.6 V
reference; output voltage was not measured. Rename the signal to `BOOST_EN` or
document its active-high behavior. [MT3608 datasheet](https://www.mouser.com/datasheet/2/306/MT3608-3223743.pdf)

**Disabling boosting does not disconnect peripheral power.** The supplied
schematic retains a path from SYS through L_BOOST and D_BOOST to the nominal
5 V rail. The continued display ACKs are consistent with that topology. They do
not measure rail voltage or confirm correct operation at a reduced voltage.
If complete peripheral power removal is required, use output isolation such as
a load switch or a converter with load disconnect.

**The USB divider leaves little ADC headroom and the readings need filtering.**
At 5.00 V USB, its ADC input is 3.00 V; at 5.25 V it would be 3.15 V.
Espressif specifies a 3.10 V measurable upper limit for ESP32-S3 at 11 dB.
Four of 4,608 individual USB conversions reached 4095; the 64-conversion batch
means remained below full scale. This establishes occasional clipped samples,
not the physical amplitude or cause of input transients. The USB input has no
filter capacitor in the supplied schematic. Consider a 100k/100k USB divider
(2.50 V at 5 V), a local 100 nF capacitor, and averaging/outlier rejection.
Espressif recommends bypass capacitance and multisampling to reduce ADC noise.
[ESP32-S3 ADC documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.4/esp32s3/api-reference/peripherals/adc.html)

## Limits of this bench

No battery was available, so battery-only operation, USB/battery handoff,
charging current, charge termination, and discharge behavior remain untested.
There is no MCU sense connection to SYS, 3V3, the boost output, charge current,
or CHRG/STDBY in these schematics. Consequently, software-only testing cannot
verify those rail voltages, boost regulation, ripple, transient response,
power savings, or actual charging status. A meter/current measurement and a
connected battery are the next useful bench inputs.

## Artifacts and restoration

- Firmware: `test/hardware/power/power_probe.cpp`; standalone PlatformIO build passed.
- Capture runner: `test/hardware/power/run_power_probe.py`; successful hardware capture validated all 72 expected batches and the completion marker.
- Raw evidence: `test/hardware/power/results-2026-11-20.jsonl` (75 JSON events).
- Only application sectors `0x10000` through `0x56fff` were temporarily written.
  Bootloader, partition table, NVS and OTA selection were not flashed.
- Restored 290,816 bytes and compared a fresh flash read with the original
  backup: SHA-256 `c55210327699c7082c67f7eaa367e1591b1610de567656515fcd00e88de41bae`.
- A fresh boot then confirmed the original player-PCB startup, display at
  0x70, BLE advertising, restored game state, and normal worker startup.
- Backup and upload/restore logs remain locally in
  `/private/tmp/scorebot-power-20261120/`.
