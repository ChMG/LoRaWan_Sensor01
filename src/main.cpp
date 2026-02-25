#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

#ifndef LORA_DEV_EUI
#define LORA_DEV_EUI 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
#endif

#ifndef LORA_APP_EUI
#define LORA_APP_EUI 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
#endif

#ifndef LORA_APP_KEY
#define LORA_APP_KEY 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
#endif

#ifndef LORA_NWK_S_KEY
#define LORA_NWK_S_KEY 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
#endif

#ifndef LORA_APP_S_KEY
#define LORA_APP_S_KEY 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
#endif

#ifndef LORA_DEV_ADDR
#define LORA_DEV_ADDR 0x00000000
#endif

#ifndef LORA_KEEP_NET
#define LORA_KEEP_NET 0
#endif

#ifndef CFG_TX_DUTY_MS
#define CFG_TX_DUTY_MS 300000
#endif

#ifndef CFG_APP_PORT
#define CFG_APP_PORT 2
#endif

#ifndef CFG_CONFIRMED_TRIALS
#define CFG_CONFIRMED_TRIALS 4
#endif

#ifndef PIN_DS18B20
#define PIN_DS18B20 GPIO4
#endif

#ifndef PIN_DHT22
#define PIN_DHT22 GPIO0
#endif

#ifndef PIN_REED_1
#define PIN_REED_1 GPIO3
#endif

#ifndef PIN_REED_2
#define PIN_REED_2 GPIO2
#endif

#ifndef PIN_REED_3
#define PIN_REED_3 GPIO1
#endif

#ifndef PIN_IMMEDIATE_TX
#define PIN_IMMEDIATE_TX GPIO7
#endif

#ifndef REED_DEBOUNCE_MS
#define REED_DEBOUNCE_MS 80
#endif

#ifndef REED_EVENT_COOLDOWN_MS
#define REED_EVENT_COOLDOWN_MS 30000
#endif

#ifndef IMMEDIATE_TX_DEBOUNCE_MS
#define IMMEDIATE_TX_DEBOUNCE_MS 80
#endif

#ifndef IMMEDIATE_TX_MIN_INTERVAL_MS
#define IMMEDIATE_TX_MIN_INTERVAL_MS 2000
#endif

/*
  CubeCell AB01 LoRaWAN Sensor
  - DS18B20 temperature
  - DHT22 temperature + humidity
  - 3x reed contacts

  Required libraries:
  - Heltec CubeCell Framework (Boardpaket)
  - OneWire
  - DallasTemperature
  - DHT sensor library
*/

/******************** LoRaWAN Keys (OTAA) ********************/
uint8_t devEui[] = { LORA_DEV_EUI };
uint8_t appEui[] = { LORA_APP_EUI };
uint8_t appKey[] = { LORA_APP_KEY };
uint8_t nwkSKey[] = { LORA_NWK_S_KEY };
uint8_t appSKey[] = { LORA_APP_S_KEY };
uint32_t devAddr =  (uint32_t)LORA_DEV_ADDR;
bool keepNet = (LORA_KEEP_NET != 0);

/******************** Regional settings ********************/
uint16_t userChannelsMask[6] = { 0x00FF, 0, 0, 0, 0, 0 };

/*
 * IMPORTANT:
 * ACTIVE_REGION is set in PlatformIO via `platformio.ini`
 * (board_build.arduino.lorawan.region), e.g. EU868.
 */
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
DeviceClass_t loraWanClass = CLASS_A;
uint32_t appTxDutyCycle = CFG_TX_DUTY_MS;
bool overTheAirActivation = true;
bool loraWanAdr = true;
bool isTxConfirmed = false;
uint8_t appPort = CFG_APP_PORT;
uint8_t confirmedNbTrials = CFG_CONFIRMED_TRIALS;

#define DHTTYPE DHT22

OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);
DHT dht(PIN_DHT22, DHTTYPE);

static DeviceAddress ds18b20Address;
static bool ds18b20Present = false;

static volatile bool reedInterruptFired = false;
static volatile bool immediateTxInterruptFired = false;
static bool reedUplinkRequested = false;
static bool immediateUplinkRequested = false;
static bool reedEventInFlight = false;
static uint8_t lastReedMask = 0;
static uint32_t lastReedProcessMs = 0;
static uint32_t lastReedUplinkMs = 0;
static uint32_t lastReedPollMs = 0;
static bool reedInterruptsEnabled = false;
static bool immediateInterruptEnabled = false;
static bool joinStatusLogged = false;
static bool immediateTriggerArmed = true;
static uint32_t lastImmediateProcessMs = 0;
static uint32_t lastImmediateUplinkMs = 0;

extern bool autoLPM;

void onReedInterrupt();
void onImmediateTxInterrupt();

static bool isNetworkJoined() {
  return IsLoRaMacNetworkJoined;
}

static void enableReedInterruptsIfNeeded() {
  if (reedInterruptsEnabled) {
    return;
  }

  attachInterrupt(PIN_REED_1, onReedInterrupt, CHANGE);
  attachInterrupt(PIN_REED_2, onReedInterrupt, CHANGE);
  attachInterrupt(PIN_REED_3, onReedInterrupt, CHANGE);
  reedInterruptsEnabled = true;
}

static void enableImmediateInterruptIfNeeded() {
  if (immediateInterruptEnabled) {
    return;
  }

  attachInterrupt(PIN_IMMEDIATE_TX, onImmediateTxInterrupt, FALLING);
  immediateInterruptEnabled = true;
}

void onReedInterrupt() {
  reedInterruptFired = true;
}

void onImmediateTxInterrupt() {
  immediateTxInterruptFired = true;
}

static void printDs18b20Address(const DeviceAddress address) {
  for (uint8_t i = 0; i < 8; i++) {
    Serial.printf("%02X", address[i]);
  }
}

static void detectDs18b20() {
  uint8_t deviceCount = ds18b20.getDeviceCount();
  bool found = ds18b20.getAddress(ds18b20Address, 0);
  ds18b20Present = found;

  if (found) {
    ds18b20.setResolution(ds18b20Address, 12);
  } else {
    (void)deviceCount;
  }
}

static int16_t encodeTempC(float tempC) {
  if (isnan(tempC)) {
    return INT16_MIN;
  }

  float scaled = tempC * 100.0f;
  if (scaled > 32767.0f) {
    scaled = 32767.0f;
  } else if (scaled < -32768.0f) {
    scaled = -32768.0f;
  }
  return (int16_t)lroundf(scaled);
}

static uint8_t encodeHumidity(float humidityPct) {
  if (isnan(humidityPct)) {
    return 0xFF;
  }

  if (humidityPct < 0.0f) {
    humidityPct = 0.0f;
  } else if (humidityPct > 100.0f) {
    humidityPct = 100.0f;
  }

  return (uint8_t)lroundf(humidityPct * 2.0f);  // 0.5% steps
}

static uint16_t readBatteryMillivolts() {
  return (uint16_t)getBatteryVoltage();
}

static uint8_t readReedBitmask() {
  uint8_t mask = 0;

  if (digitalRead(PIN_REED_1) == LOW) {
    mask |= (1 << 0);
  }
  if (digitalRead(PIN_REED_2) == LOW) {
    mask |= (1 << 1);
  }
  if (digitalRead(PIN_REED_3) == LOW) {
    mask |= (1 << 2);
  }

  return mask;
}

static void handleReedPollingFallback() {
  uint32_t now = millis();
  if (now - lastReedPollMs < 100) {
    return;
  }
  lastReedPollMs = now;

  uint8_t currentMask = readReedBitmask();
  if (currentMask != lastReedMask) {
    lastReedMask = currentMask;
    reedUplinkRequested = true;
    Serial.printf("Reed polling change detected, new mask: 0x%02X\n", currentMask);
  }
}

static void handleReedInterruptEvent() {
  if (!reedInterruptFired) {
    return;
  }

  noInterrupts();
  reedInterruptFired = false;
  interrupts();

  uint32_t now = millis();
  if (now - lastReedProcessMs < REED_DEBOUNCE_MS) {
    return;
  }
  lastReedProcessMs = now;

  uint8_t currentMask = readReedBitmask();
  if (currentMask != lastReedMask) {
    lastReedMask = currentMask;
    reedUplinkRequested = true;
    Serial.printf("Reed change detected, new mask: 0x%02X\n", currentMask);
  }
}

static void handleImmediateTxInterruptEvent() {
  if (!immediateTriggerArmed && digitalRead(PIN_IMMEDIATE_TX) == HIGH) {
    immediateTriggerArmed = true;
  }

  if (!immediateTxInterruptFired) {
    return;
  }

  noInterrupts();
  immediateTxInterruptFired = false;
  interrupts();

  uint32_t now = millis();
  if (now - lastImmediateProcessMs < IMMEDIATE_TX_DEBOUNCE_MS) {
    return;
  }
  lastImmediateProcessMs = now;

  if (digitalRead(PIN_IMMEDIATE_TX) != LOW) {
    return;
  }

  if (!immediateTriggerArmed) {
    return;
  }

  if (now - lastImmediateUplinkMs < IMMEDIATE_TX_MIN_INTERVAL_MS) {
    return;
  }

  immediateTriggerArmed = false;
  lastImmediateUplinkMs = now;
  immediateUplinkRequested = true;
  Serial.printf("Immediate send trigger detected on PIN %d\n", PIN_IMMEDIATE_TX);
}

static void buildPayload() {
  if (!ds18b20Present) {
    detectDs18b20();
  }

  float tempDs = NAN;
  if (ds18b20Present) {
    ds18b20.requestTemperaturesByAddress(ds18b20Address);
    tempDs = ds18b20.getTempC(ds18b20Address);

    if (tempDs <= -126.0f || tempDs >= 125.0f || tempDs == DEVICE_DISCONNECTED_C) {
      delay(120);
      ds18b20.requestTemperaturesByAddress(ds18b20Address);
      tempDs = ds18b20.getTempC(ds18b20Address);
    }

    if (tempDs <= -126.0f || tempDs >= 125.0f || tempDs == DEVICE_DISCONNECTED_C) {
      Serial.println("DS18B20 read failed, trying re-detect...");
      ds18b20Present = false;
      tempDs = NAN;
    }
  }

  float tempDht = NAN;
  float humidity = NAN;
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    tempDht = dht.readTemperature();
    humidity = dht.readHumidity();
    if (!isnan(tempDht) && !isnan(humidity)) {
      break;
    }
    delay(120);
  }

  int16_t tempDsEnc = encodeTempC(tempDs);
  int16_t tempDhtEnc = encodeTempC(tempDht);
  uint8_t humEnc = encodeHumidity(humidity);
  uint8_t reedMask = readReedBitmask();
  uint16_t battMv = readBatteryMillivolts();

  uint8_t status = 0;
  if (!isnan(tempDs)) {
    status |= (1 << 0);
  }
  if (!isnan(tempDht)) {
    status |= (1 << 1);
  }
  if (!isnan(humidity)) {
    status |= (1 << 2);
  }
  if (battMv > 0) {
    status |= (1 << 3);
  }

  appData[0] = (uint8_t)(tempDsEnc >> 8);
  appData[1] = (uint8_t)(tempDsEnc & 0xFF);
  appData[2] = (uint8_t)(tempDhtEnc >> 8);
  appData[3] = (uint8_t)(tempDhtEnc & 0xFF);
  appData[4] = humEnc;
  appData[5] = reedMask;
  appData[6] = status;
  appData[7] = (uint8_t)(battMv >> 8);
  appData[8] = (uint8_t)(battMv & 0xFF);
  appDataSize = 9;

  Serial.printf("DS18B20: %.2f C, DHT22: %.2f C, RH: %.2f %%, ReedMask: 0x%02X, VBAT: %u mV\n",
                tempDs, tempDht, humidity, reedMask, battMv);
}

void prepareTxFrame(uint8_t port) {
  (void)port;
  buildPayload();
}

void setup() {
  Serial.begin(115200);
  autoLPM = false;

  pinMode(PIN_REED_1, INPUT_PULLUP);
  pinMode(PIN_REED_2, INPUT_PULLUP);
  pinMode(PIN_REED_3, INPUT_PULLUP);
  pinMode(PIN_IMMEDIATE_TX, INPUT_PULLUP);

  lastReedMask = readReedBitmask();

  ds18b20.begin();
  detectDs18b20();
  dht.begin();

  deviceState = DEVICE_STATE_INIT;
  LoRaWAN.ifskipjoin();
}

void loop() {
  handleReedInterruptEvent();
  handleReedPollingFallback();
  handleImmediateTxInterruptEvent();

  if (isNetworkJoined()) {
    if (!joinStatusLogged) {
      joinStatusLogged = true;
      Serial.println("\nJoin successful, interrupts enabled");
    }
    enableReedInterruptsIfNeeded();
    enableImmediateInterruptIfNeeded();
  } else {
    joinStatusLogged = false;
  }

  if (immediateUplinkRequested &&
      isNetworkJoined() &&
      (deviceState == DEVICE_STATE_SLEEP || deviceState == DEVICE_STATE_CYCLE)) {
    immediateUplinkRequested = false;
    reedEventInFlight = false;
    deviceState = DEVICE_STATE_SEND;
  }

  if (reedUplinkRequested &&
      isNetworkJoined() &&
      (deviceState == DEVICE_STATE_SLEEP || deviceState == DEVICE_STATE_CYCLE)) {
    reedUplinkRequested = false;
    reedEventInFlight = true;
    deviceState = DEVICE_STATE_SEND;
  }

  switch (deviceState) {
    case DEVICE_STATE_INIT: {
#if (LORAWAN_DEVEUI_AUTO)
      LoRaWAN.generateDeveuiByChipID();
#endif
      printDevParam();
      LoRaWAN.init(loraWanClass, loraWanRegion);
      break;
    }

    case DEVICE_STATE_JOIN: {
      LoRaWAN.join();
      break;
    }

    case DEVICE_STATE_SEND: {
      if (!isNetworkJoined()) {
        deviceState = DEVICE_STATE_JOIN;
        break;
      }
      prepareTxFrame(appPort);
      bool sendFailed = SendFrame();
      if (sendFailed) {
        txDutyCycleTime = 2000;
        LoRaWAN.cycle(txDutyCycleTime);
        deviceState = DEVICE_STATE_SLEEP;
      } else {
        if (reedEventInFlight) {
          lastReedUplinkMs = millis();
          reedEventInFlight = false;
        }
        deviceState = DEVICE_STATE_CYCLE;
      }
      break;
    }

    case DEVICE_STATE_CYCLE: {
      txDutyCycleTime = appTxDutyCycle + randr(-APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND);
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;
    }

    case DEVICE_STATE_SLEEP: {
      LoRaWAN.sleep();
      break;
    }

    default: {
      deviceState = DEVICE_STATE_INIT;
      break;
    }
  }
}
