# ESP32S3_BMV080

Firmware for an **ESP32-S3-WROOM** with a single **Bosch BMV080** particulate matter sensor.

Derived from `POLVERINE_FULL_BLE_DEMO`. Removed: BME690/BSEC, the BLE transport and the
serial link to the robot. Kept: the BMV080 driver, the particle feature frame (PFF) stream
and the Wi-Fi transport with the live web page.

## Files that are not in this repository

Some of what the firmware needs belongs to third parties and is therefore not committed,
see `.gitignore`. A fresh clone does **not** build until these are put back in place:

| Path | Where it comes from |
|------|---------------------|
| `deps/bosch-sensortec/BMV080-SDK/` | Bosch BMV080 SDK. Proprietary, ships without a licence file. |
| `src/bmv080_io.c`, `include/bmv080_io.h` | BlackIoT Sagl, the file headers read "All Rights Reserved. Confidential." |
| `include/diag.h` | Taken unmodified from `POLVERINE_FULL_BLE_DEMO`. |
| `pff_udp_listener.py`, `pff_http_listener.py` | Taken unmodified from `POLVERINE_FULL_BLE_DEMO`. |

Copy them over from a `POLVERINE_FULL_BLE_DEMO` working copy, keeping the paths above.
The SDK version this was built against is recorded in `platformio.ini` through the
library names `_bmv080` and `_postProcessor`.

The remaining sources in `src/` and `include/` are derived from the same demo and were
rewritten for this board; they are committed.

## Wiring

The BMV080 is wired up over **I2C**, the pins and the bus settings are defined at the top of
`include/bmv080_io.h`:

| BMV080     | ESP32-S3 |
|------------|----------|
| SDA        | GPIO 21  |
| SCL        | GPIO 17  |
| VDD, VDDIO | 3V3      |
| GND        | GND      |

7-bit device address `0x54`, 100 kHz, as in the Bosch ESP32 example. The driver polls the
sensor, no interrupt or reset line is needed.

Pull ups: the code follows the Bosch example and leaves the internal pull ups off, so the
breakout board has to bring its own (4.7 kOhm is a good value). If it does not, set
`BMV080_I2C_INTERNAL_PULLUP` to 1 in `include/bmv080_io.h` - the internal ones are weak
(about 45 kOhm) but usually carry 100 kHz over short wires.

SPI is still in the tree. Setting `BMV080_USE_I2C` to 0 switches back to it, the pins are then
SCK 12, MOSI 11, MISO 13, CS 10, as on the Polverine board.

Three optional status LEDs sit on GPIO 4 (red, sensor error), 5 (green, init steps) and
6 (blue, unused), see `include/peripherals.h`. Without LEDs the firmware behaves the same.

## Build and flash

```
pio run -e esp32s3 -t upload -t monitor
```

The partition table has no OTA slot and no file system, it ends at 0x310000 so that the
build also fits on a 4 MB module. On a module with more flash, raise
`board_upload.flash_size` in `platformio.ini`, `CONFIG_ESPTOOLPY_FLASHSIZE_*` in
`sdkconfig.defaults` and the `factory` size in `partitions.csv` together.

The path to the project must not contain spaces, the ESP-IDF build refuses those.

## Output

`NET_MODE_ACCESS_POINT` in `include/net_main.h` selects the mode.

**1 (default)** - the board opens the access point `BMV080_SENSOR` (password `12345678`)
and serves the page itself:

* `http://192.168.4.1/` live plot of the particle feature frames
* `http://192.168.4.1/status` uptime, free heap, frame counters and the last log lines
* `http://192.168.4.1/pff` the raw lines, everything newer than a given sequence number

**0** - the board joins the access point named by `NET_WIFI_SSID` and sends the same lines
as UDP datagrams to port 4210. `pff_udp_listener.py` receives them on a laptop.

Line formats, unchanged from the Polverine demo:

```
PFF,uptime,channel,frequency,snr
{"topic":"bmv080","data":{"ID":...,"PM10":...,"PM25":...,"PM1":...,"obst":...,"omr":...,"T":...}}
```

## Web page

`web/index.html` is the source, `before_build.py` turns it into `src/web_page.c` on every
build. Edit the HTML, not the generated C file.
