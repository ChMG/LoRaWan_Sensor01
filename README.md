# LoRaWan_Sensor01 V1.0 (CubeCell AB01, DS18B20 + DHT22 + 3x Reed)

## Hardware
- 1x Heltec CubeCell AB01
- 1x DS18B20
- 1x DHT22
- 3x Reed Kontakt

## Platinenlayout
- Das Platinen-Design liegt im Ordner `Fritzing` in der Datei `LoRaWan_Sensor01.fzz`.
- Direkt bestellen (Aisler): https://aisler.net/p/VXRWBTJL
- Anschluss `Temp1` = `DHT22`
- Anschluss `Temp2` = `DS18B20`
- Anschluss `SW1` = `Reed 1`
- Anschluss `SW2` = `Reed 2`
- Anschluss `SW3` = `Reed 3`
- Anschluss `Send` = `Sofort-Senden Trigger`
- Anschluss `Solar` = `Solar Modul für Batterieladung`
- Alle Widerstände sind PullUp mit `4,7k`

![LoRaWan_Sensor01 Platine](Fritzing/LoRaWan_Sensor01.png)

## Verdrahtung (Vorschlag / Verwendet bei Platine)
> Passe die Pins bei Bedarf im Sketch an.

- DS18B20 Data -> `GPIO5`
- DHT22 Data -> `GPIO0`
- Reed 1 -> `GPIO3`
- Reed 2 -> `GPIO2`
- Reed 3 -> `GPIO1`
- Sofort-Senden Trigger -> `GPIO7`

Zusätzlich:
- DS18B20: 4.7k Pullup zwischen Data und 3V3
- DHT22: je nach Modul oft schon Pullup vorhanden, sonst 10k zwischen Data und 3V3
- Reedkontakte jeweils zwischen GPIO und GND (im Sketch `INPUT_PULLUP`, daher aktiv LOW)

## Software (PlatformIO)
1. VS Code + PlatformIO IDE Extension installieren
2. Projekt öffnen (Ordner mit `platformio.ini`)
3. Alle Projektvariablen in `platformio.ini` setzen (unter `build_flags`):
  - LoRa Keys (`LORA_DEV_EUI`, `LORA_APP_EUI`, `LORA_APP_KEY`)
  - Pins (`PIN_DHT22`, `PIN_DS18B20`, `PIN_REED_1..3`)
  - Timing (`CFG_TX_DUTY_MS`, `REED_EVENT_COOLDOWN_MS`)
4. LoRa-Region in `platformio.ini` setzen (z. B. `board_build.arduino.lorawan.region = EU868`)
5. Build/Flash/Monitor starten:
  - Build: `pio run`
  - Upload: `pio run -t upload`
  - Monitor: `pio device monitor -b 115200`

Genutzte Board-Konfiguration:
- `platform = heltec-cubecell`
- `board = cubecell_board` (HTCC-AB01)
- `framework = arduino`

## WSL: USB verbinden und Upload testen

Wenn das Board in WSL nicht als `/dev/ttyUSB*` oder `/dev/ttyACM*` erscheint:

1. Unter Windows (PowerShell als Administrator) Gerät an WSL anhängen:
  - `usbipd list`
  - `usbipd bind --busid <BUSID>`
  - `usbipd attach --wsl --busid <BUSID>`
2. In WSL serielle Treiber laden:
  - `sudo modprobe usbserial`
  - `sudo modprobe cp210x`
3. Port prüfen:
  - `ls /dev/ttyUSB* /dev/ttyACM*`

Praktische Helfer im Projekt:
- Upload (Auto-Port): `./upload.sh`
- Upload (explizit): `./upload.sh LoRaWan_Sensor01 /dev/ttyUSB0`
- Monitor (Auto-Port, 115200): `./monitor.sh`
- Monitor (explizit): `./monitor.sh 115200 /dev/ttyUSB0`

Hinweis: Bei `Permission denied` auf `/dev/ttyUSB0` hilft testweise:
- `sudo chmod 666 /dev/ttyUSB0`

## Uplink Payload (Port 2)
9 Byte insgesamt:

- Byte 0..1: DS18B20 Temperatur als `int16`, Einheit: `°C * 100`, Big Endian
- Byte 2..3: DHT22 Temperatur als `int16`, Einheit: `°C * 100`, Big Endian
- Byte 4: Luftfeuchte als `uint8`, Einheit: `% * 2` (0.5%-Schritte), `0xFF` = ungültig
- Byte 5: Reed-Bitmaske (`bit0=Reed1`, `bit1=Reed2`, `bit2=Reed3`, 1 = geschlossen)
- Byte 6: Statusbits (`bit0=DS gültig`, `bit1=DHT Temp gültig`, `bit2=DHT RH gültig`, `bit3=VBAT gültig`)
- Byte 7..8: Akku-Spannung als `uint16`, Einheit: `mV`, Big Endian

## Decoder (z. B. TTN v3)
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

## OTAA Byte-Reihenfolge (wichtig für CubeCell)
- `DevEUI`, `JoinEUI/AppEUI` und `AppKey` exakt wie in der TTN Console eintragen.
- Bei Join-Problemen zuerst immer Werte zwischen TTN und `platformio.ini` 1:1 vergleichen.

## Hinweise
- `appTxDutyCycle` ist aktuell auf 15 Minuten gesetzt.
- Für Batterieanwendungen kannst du das Intervall erhöhen (z. B. 15–60 Minuten).
- DS18B20 Wandlung dauert je nach Auflösung bis zu ~750 ms.
- Reedkontakte laufen zusätzlich über Interrupt (`CHANGE`) mit Entprellung (~80 ms).
- Bei Reed-Zustandsänderung (Öffnen/Schließen) wird ein zeitnaher Uplink ausgelöst (zusätzlich zum zyklischen Intervall).

## Finale Produktionswerte (Checkliste)
- LoRaWAN: `OTAA`, Region `EU868`, Class `A`, Uplink `UNCONFIRMED`
- Uplink Intervall: `CFG_TX_DUTY_MS=900000` (15 Minuten)
- Sensor-/Eingangspins:
  - `PIN_DS18B20=GPIO5`
  - `PIN_DHT22=GPIO0`
  - `PIN_REED_1=GPIO3`
  - `PIN_REED_2=GPIO2`
  - `PIN_REED_3=GPIO1`
  - `PIN_IMMEDIATE_TX=GPIO7`
- DS18B20 benötigt Pullup `4.7k` zwischen Data und `3V3`
- WSL Upload/Monitor:
  - Upload: `./upload.sh`
  - Monitor: `./monitor.sh`

## Troubleshooting (3 Checks)
1. Join klappt nicht:
  - TTN Credentials 1:1 mit `platformio.ini` vergleichen (`DevEUI`, `JoinEUI/AppEUI`, `AppKey`)
  - Region/Frequency Plan: `EU868`
2. DS18B20 zeigt `nan`:
  - `PIN_DS18B20` prüfen
  - `4.7k` Pullup zwischen Data und `3V3`
  - gemeinsame Masse sicherstellen
3. Reed-Event sendet nicht:
  - korrekte Pinzuordnung (`PIN_REED_1..3`) prüfen
  - im Monitor auf `Reed ... detected, new mask` achten
  - in TTN Live Data auf Uplink mit geändertem `reed_mask` prüfen
