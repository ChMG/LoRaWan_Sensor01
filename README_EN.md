# LoRaWan_Sensor01 (CubeCell AB01, DS18B20 + DHT22 + 3x Reed)

## Hardware
- 1x Heltec CubeCell AB01
- 1x DS18B20
- 1x DHT22
- 3x Reed contact

## PCB Layout
- The PCB design is located in the `Fritzing` folder in `LoRaWan_Sensor01.fzz`.
- Direct order (Aisler): https://aisler.net/p/VXRWBTJL
- Connector `Temp1` = `DHT22`
- Connector `Temp2` = `DS18B20`
- Connector `SW1` = `Reed 1`
- Connector `SW2` = `Reed 2`
- Connector `SW3` = `Reed 3`
- Connector `Send` = `Immediate send trigger`
- Connector `Solar` = `Solar module for battery charging`
- All resistors are `4.7k` pull-ups

![LoRaWan_Sensor01 PCB](Fritzing/LoRaWan_Sensor01.png)

## Wiring (suggestion / used on PCB)
> Adjust pins in the sketch if needed.

- DS18B20 Data -> `GPIO5`
- DHT22 Data -> `GPIO0`
- Reed 1 -> `GPIO3`
- Reed 2 -> `GPIO2`
- Reed 3 -> `GPIO1`
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
  - Pins (`PIN_DHT22`, `PIN_DS18B20`, `PIN_REED_1..3`)
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
9 bytes total:

- Byte 0..1: DS18B20 temperature as `int16`, unit: `°C * 100`, big endian
- Byte 2..3: DHT22 temperature as `int16`, unit: `°C * 100`, big endian
- Byte 4: Humidity as `uint8`, unit: `% * 2` (0.5% steps), `0xFF` = invalid
- Byte 5: Reed bitmask (`bit0=Reed1`, `bit1=Reed2`, `bit2=Reed3`, 1 = closed)
- Byte 6: Status bits (`bit0=DS valid`, `bit1=DHT temp valid`, `bit2=DHT RH valid`, `bit3=VBAT valid`)
- Byte 7..8: Battery voltage as `uint16`, unit: `mV`, big endian

## Decoder (e.g. TTN v3)
```js
function decodeUplink(input) {
  const b = input.bytes;
  if (b.length < 9) {
    return { errors: ["Payload too short"] };
  }

  const i16 = (msb, lsb) => {
    let v = (msb << 8) | lsb;
    if (v & 0x8000) v -= 0x10000;
    return v;
  };

  const dsRaw = i16(b[0], b[1]);
  const dhtRaw = i16(b[2], b[3]);
  const humRaw = b[4];
  const reed = b[5];
  const status = b[6];
  const battMv = (b[7] << 8) | b[8];

  const dsValid = !!(status & 0x01);
  const dhtTempValid = !!(status & 0x02);
  const dhtHumValid = !!(status & 0x04);
  const battValid = !!(status & 0x08);

  return {
    data: {
      temperature_ds18b20_c: dsValid ? dsRaw / 100 : null,
      temperature_dht22_c: dhtTempValid ? dhtRaw / 100 : null,
      humidity_dht22_pct: dhtHumValid && humRaw !== 0xFF ? humRaw / 2 : null,
      battery_mv: battValid ? battMv : null,
      reed1_closed: !!(reed & 0x01),
      reed2_closed: !!(reed & 0x02),
      reed3_closed: !!(reed & 0x04),
      raw: { reed_mask: reed, status }
    }
  };
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

## Final Production Values (Checklist)
- LoRaWAN: `OTAA`, region `EU868`, class `A`, uplink `UNCONFIRMED`
- Uplink interval: `CFG_TX_DUTY_MS=900000` (15 minutes)
- Sensor/input pins:
  - `PIN_DS18B20=GPIO5`
  - `PIN_DHT22=GPIO0`
  - `PIN_REED_1=GPIO3`
  - `PIN_REED_2=GPIO2`
  - `PIN_REED_3=GPIO1`
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
  - Check `PIN_DS18B20`
  - Check `4.7k` pull-up between Data and `3V3`
  - Ensure common ground
3. Reed event not sending:
  - Check correct pin mapping (`PIN_REED_1..3`)
  - Watch monitor for `Reed ... detected, new mask`
  - Check TTN Live Data for uplink with changed `reed_mask`
