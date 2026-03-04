# LoRaWan_Sensor01 V1.1 (CubeCell AB01, DS18B20 + DHT22 + Reed)

## Hardware
- Heltec CubeCell AB01
- DS18B20
- DHT22
- Reed contact (LOW = closed)

## PCB Layout
- The PCB design is located in the `Fritzing` folder in `LoRaWan_Sensor01.fzz`.
- Direct order (Aisler): https://aisler.net/p/UXGEYAMX
- Connector `Sensor[1-5]` = `DHT22, DS18B20 or Reed contact`
- Connector `Send` = `Immediate send trigger`
- Connector `Solar` = `Solar module for battery charging`
- All resistors are `4.7k` pull-ups

![LoRaWan_Sensor01 PCB](Fritzing/LoRaWan_Sensor01.png)

## Wiring (suggestion / used on PCB)
> Adjust pins in the sketch if needed.

- Sensor1 -> `GPIO3`
- Sensor2 -> `GPIO2`
- Sensor3 -> `GPIO1`
- Sensor4 -> `GPIO5`
- Sensor5 -> `GPIO0`
- Immediate send trigger -> `GPIO7`

Additionally:
- DS18B20: 4.7k pull-up between Data and 3V3
- DHT22: depending on module, a pull-up is often already present; otherwise use 10k between Data and 3V3
- Reed contacts each connected between GPIO and GND (sketch uses `INPUT_PULLUP`, therefore active LOW)

## Software (PlatformIO)
1. Install VS Code + PlatformIO IDE extension
2. Open the project (folder containing `platformio.ini`)
3. Set all project variables in `platformio.ini` (under `build_flags`):
  - LoRa keys (`LORA_DEV_EUI`, `LORA_APP_EUI`, `LORA_APP_KEY`)
  - Sensor pins (`PIN_SENSOR1..5`)
  - Default sensor type per pin (`SENSOR1_TYPE..SENSOR5_TYPE`, default: all `SENSOR_TYPE_REED`)
  - Downlink config port (`CFG_CONFIG_PORT`, default `100`)
  - Timing (`CFG_TX_DUTY_MS`, `REED_EVENT_COOLDOWN_MS`)
4. Set LoRa region in `platformio.ini` (e.g. `board_build.arduino.lorawan.region = EU868`)
5. Build/Flash/Monitor:
  - Build: `pio run`
  - Upload: `pio run -t upload`
  - Monitor: `pio device monitor -b 115200`

Board configuration used:
- `platform = heltec-cubecell`
- `board = cubecell_board` (HTCC-AB01)
- `framework = arduino`

## WSL: Connect USB and test upload

If the board does not appear in WSL as `/dev/ttyUSB*` or `/dev/ttyACM*`:

1. Under Windows (PowerShell as Administrator), attach the device to WSL:
  - `usbipd list`
  - `usbipd bind --busid <BUSID>`
  - `usbipd attach --wsl --busid <BUSID>`
2. In WSL, load serial drivers:
  - `sudo modprobe usbserial`
  - `sudo modprobe cp210x`
3. Check port:
  - `ls /dev/ttyUSB* /dev/ttyACM*`

Useful helper scripts in this project:
- Upload (auto port): `./upload.sh`
- Upload (explicit): `./upload.sh LoRaWan_Sensor01 /dev/ttyUSB0`
- Monitor (auto port, 115200): `./monitor.sh`
- Monitor (explicit): `./monitor.sh 115200 /dev/ttyUSB0`

Note: If you get `Permission denied` on `/dev/ttyUSB0`, temporary workaround:
- `sudo chmod 666 /dev/ttyUSB0`

## Uplink Payload (Port 2)
27 bytes total:

- Byte 0..1: battery voltage as `uint16`, unit: `mV`, big endian
- Then 5 sensor blocks of 5 bytes each (for `Sensor1..Sensor5`):
  - `B0`: sensor type (`0=None`, `1=DHT22`, `2=DS18B20`, `3=REED`)
  - `B1`: per-slot status bits
    - `bit0`: temperature valid
    - `bit1`: humidity valid
    - `bit2`: reed value valid
    - `bit3`: reed closed
  - `B2..B3`: temperature as `int16`, unit `°C * 100`, big endian (`INT16_MIN` when unused/invalid)
  - `B4`: slot data
    - DHT22: humidity as `uint8`, `% * 2` (0.5% steps), `0xFF` invalid
    - DS18B20: `0xFF` (reserved)
    - REED: `0` (open) or `1` (closed)

## Decoder (e.g. TTN v3)
The full decoder is in [ttn/decoder.js](ttn/decoder.js).

For TTN v3, copy this file content into **Payload Formatters → Uplink**.

This gives each sensor slot its own block, so decoding stays correct even if all 5 slots are configured as DHT22, DS18B20, or REED.

## Change sensor types via TTN downlink (persistent)
- First boot default: all 5 sensor slots are `SENSOR_TYPE_REED`.
- Downlink port: `CFG_CONFIG_PORT` (default `100`).

- Payload format to set all 5 types:
  - Variant A (direct): 5 bytes = `type1,type2,type3,type4,type5`
  - Variant B (with command): 6 bytes = `0x01,type1,type2,type3,type4,type5`

Type value mapping:

| Type value | Sensor type |
|---|---|
| `0` | `NONE` |
| `1` | `DHT22` |
| `2` | `DS18B20` |
| `3` | `REED` |

- Configuration is stored in internal memory and remains available after reboot.

| Hex | Effect | Example payload |
|---|---|---|
| `0x01` | Set sensor types | `01 03 03 03 02 01` |

Note: Single-byte commands like `02` or `03` are disabled and ignored by the device (serial log: `single-byte commands disabled`).

TTN downlink examples (port `100`, **Hex** format):
- All 5 sensors as REED: `03 03 03 03 03`
- All 5 sensors as DHT22: `01 01 01 01 01`
- All 5 sensors as DS18B20: `02 02 02 02 02`
- Mixed (Sensor1..3=REED, Sensor4=DS18B20, Sensor5=DHT22): `03 03 03 02 01`
- With command byte (same mixed setup): `01 03 03 03 02 01`

TTN Console click path (send downlink):
1. Open your **Application** in TTN.
2. Go to **End devices** and open your device.
3. Open the **Messaging** tab.
4. In the **Downlink** section:
  - Set **FPort** to `100`
  - Set **Payload type** to **Hex**
  - Paste an example payload (e.g. `03 03 03 02 01`)
5. Click **Schedule downlink**.
6. The downlink is delivered after the next uplink; following uplinks show the new `type_name` values.

Example (shortened) in TTN Live Data:
```json
{
  "battery_mv": 4012,
  "sensor_1": { "type_name": "REED", "closed": true },
  "sensor_2": { "type_name": "REED", "closed": false },
  "sensor_4": { "type_name": "DS18B20", "temperature_c": 21.75 },
  "sensor_5": { "type_name": "DHT22", "temperature_c": 22.1, "humidity_pct": 56.5 }
}
```

## OTAA Byte Order (important for CubeCell)
- Enter `DevEUI`, `JoinEUI/AppEUI`, and `AppKey` exactly as shown in the TTN Console.
- For join issues, first compare values between TTN and `platformio.ini` 1:1.

## Notes
- `appTxDutyCycle` is currently set to 15 minutes.
- For battery applications, you can increase the interval (e.g. 15–60 minutes).
- DS18B20 conversion can take up to ~750 ms depending on resolution.
- Reed contacts are additionally handled via interrupt (`CHANGE`) with debounce (~80 ms).
- Reed state changes (open/close) trigger a timely uplink (in addition to the periodic interval).

## Flexible Sensor Configuration
- New pin names:
  - `PIN_SENSOR1`, `PIN_SENSOR2`, `PIN_SENSOR3`, `PIN_SENSOR4`, `PIN_SENSOR5`
- Type per sensor pin:
  - `SENSOR_TYPE_DHT22`
  - `SENSOR_TYPE_DS18B20`
  - `SENSOR_TYPE_REED`
- Mapping is configured through:
  - `SENSOR1_TYPE`, `SENSOR2_TYPE`, `SENSOR3_TYPE`, `SENSOR4_TYPE`, `SENSOR5_TYPE`
- Example (current setup):
  - `SENSOR1_TYPE=SENSOR_TYPE_REED`
  - `SENSOR2_TYPE=SENSOR_TYPE_REED`
  - `SENSOR3_TYPE=SENSOR_TYPE_REED`
  - `SENSOR4_TYPE=SENSOR_TYPE_REED`
  - `SENSOR5_TYPE=SENSOR_TYPE_REED`

### Example 1: 3x Reed + DS18B20 + DHT22
```ini
-D PIN_SENSOR1=GPIO3
-D PIN_SENSOR2=GPIO2
-D PIN_SENSOR3=GPIO1
-D PIN_SENSOR4=GPIO5
-D PIN_SENSOR5=GPIO0
-D SENSOR1_TYPE=SENSOR_TYPE_REED
-D SENSOR2_TYPE=SENSOR_TYPE_REED
-D SENSOR3_TYPE=SENSOR_TYPE_REED
-D SENSOR4_TYPE=SENSOR_TYPE_DS18B20
-D SENSOR5_TYPE=SENSOR_TYPE_DHT22
```

### Example 2: 5x Reed only
```ini
-D PIN_SENSOR1=GPIO3
-D PIN_SENSOR2=GPIO2
-D PIN_SENSOR3=GPIO1
-D PIN_SENSOR4=GPIO5
-D PIN_SENSOR5=GPIO0
-D SENSOR1_TYPE=SENSOR_TYPE_REED
-D SENSOR2_TYPE=SENSOR_TYPE_REED
-D SENSOR3_TYPE=SENSOR_TYPE_REED
-D SENSOR4_TYPE=SENSOR_TYPE_REED
-D SENSOR5_TYPE=SENSOR_TYPE_REED
```

## Final Production Values (Checklist)
- LoRaWAN: `OTAA`, region `EU868`, class `A`, uplink `UNCONFIRMED`
- Uplink interval: `CFG_TX_DUTY_MS=900000` (15 minutes)
- Sensor/input pins:
  - `PIN_SENSOR1=GPIO3` (`SENSOR_TYPE_REED`)
  - `PIN_SENSOR2=GPIO2` (`SENSOR_TYPE_REED`)
  - `PIN_SENSOR3=GPIO1` (`SENSOR_TYPE_REED`)
  - `PIN_SENSOR4=GPIO5` (`SENSOR_TYPE_REED`)
  - `PIN_SENSOR5=GPIO0` (`SENSOR_TYPE_REED`)
  - Type changes to DHT22/DS18B20 are done via TTN downlink
  - `PIN_IMMEDIATE_TX=GPIO7`
- DS18B20 requires `4.7k` pull-up between Data and `3V3`
- WSL upload/monitor:
  - Upload: `./upload.sh`
  - Monitor: `./monitor.sh`

## Troubleshooting (3 checks)
1. Join does not work:
  - Compare TTN credentials 1:1 with `platformio.ini` (`DevEUI`, `JoinEUI/AppEUI`, `AppKey`)
  - Region/Frequency plan: `EU868`
2. DS18B20 shows `nan`:
  - Check `PIN_SENSORx` and `SENSORx_TYPE=SENSOR_TYPE_DS18B20`
  - Check `4.7k` pull-up between Data and `3V3`
  - Ensure common ground
3. Reed event not sending:
  - Check correct pin mapping (`PIN_SENSORx` with `SENSOR_TYPE_REED`)
  - Watch monitor for `Reed ... detected, new mask`
  - Check TTN Live Data for changed `sensor_x.closed` fields on REED slots
