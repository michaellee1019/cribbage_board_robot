# Player-board power bench

Standalone firmware for the supplied PlayerBoardV9 and TimmonsLeePPSV2
schematics. It does not initialize BLE/Wi-Fi or write game settings.

| Signal | ESP32-S3 pin | Interpretation |
| --- | --- | --- |
| BAT_SENSE | GPIO1 / ADC1 channel 0 | BAT terminal voltage = ADC voltage × 2 |
| VBUS_SENSE | GPIO2 / ADC1 channel 1 | USB voltage = ADC voltage × 5/3 |
| 5V_ENN | GPIO47 | Direct MT3608 enable: **high enables boosting** |
| Display I2C | GPIO17 SDA, GPIO18 SCL | HT16K33 at 0x70 |
| RGB data | GPIO48 | Eight SK6812SIDE RGB LEDs |

Build from the repository root:

```sh
pio run -d test/hardware/power
```

Before flashing, back up the application sectors that will be erased and
check the board's partition table and selected OTA slot. The November 20 bench
used the installed bootloader and 8 MB partition map on a 16 MB flash chip;
app0 at 0x10000 was selected. Only the temporary application image was written,
and the original affected sectors were restored afterward. Do not assume those
offsets or partition settings apply to another board.

Once this diagnostic is running, capture a sequence with Python and pyserial:

```sh
python3 test/hardware/power/run_power_probe.py \
  --port /dev/cu.usbmodem21101 --log /tmp/power-results.jsonl
```

The runner opens a fresh log, handles native USB startup resets, requests fresh
firmware identity, and requires all 72 measurements plus the completion marker.
Each measurement averages 64 conversions per channel, after eight discarded
conversions, using 12-bit ADC1, 11 dB attenuation, and eFuse calibration when
available. `*_saturated` counts conversions at or above 4080 out of 4095.

The sequence compares floating/high/low boost control, lights off, dim and
bright loads, recovery, and five low/high cycles. Dim uses display brightness
1/15 and RGB white 4/255; bright uses 15/15 and RGB white 32/255. Load settings
describe commands sent to the devices; current and optical output are not
measured. The final state releases GPIO47 and turns the lights off.

Commands over an already open serial connection: `info`, `status`, `quiet`,
`stream`, `high`, `low`, `float`, `off`, `dim`, `bright`, and `run`.

Restore the backed-up application sectors after the bench run and verify them
against a fresh flash read. This capture runner does not upload or restore
firmware.

The dated JSONL file contains the successful November 20 capture. Findings and
restoration evidence are in `docs/power-management-bench-2026-11-20.md`.
