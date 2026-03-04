#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <EEPROM.h>
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

#ifndef CFG_CONFIG_PORT
#define CFG_CONFIG_PORT 100
#endif

static const uint8_t CMD_SET_SENSOR_TYPES = 0x01;

#ifndef SENSOR_TYPE_NONE
#define SENSOR_TYPE_NONE 0
#endif

#ifndef SENSOR_TYPE_DHT22
#define SENSOR_TYPE_DHT22 1
#endif

#ifndef SENSOR_TYPE_DS18B20
#define SENSOR_TYPE_DS18B20 2
#endif

#ifndef SENSOR_TYPE_REED
#define SENSOR_TYPE_REED 3
#endif

#ifndef PIN_SENSOR1
#define PIN_SENSOR1 GPIO3
#endif

#ifndef PIN_SENSOR2
#define PIN_SENSOR2 GPIO2
#endif

#ifndef PIN_SENSOR3
#define PIN_SENSOR3 GPIO1
#endif

#ifndef PIN_SENSOR4
#define PIN_SENSOR4 GPIO5
#endif

#ifndef PIN_SENSOR5
#define PIN_SENSOR5 GPIO0
#endif

#ifndef SENSOR1_TYPE
#define SENSOR1_TYPE SENSOR_TYPE_REED
#endif

#ifndef SENSOR2_TYPE
#define SENSOR2_TYPE SENSOR_TYPE_REED
#endif

#ifndef SENSOR3_TYPE
#define SENSOR3_TYPE SENSOR_TYPE_REED
#endif

#ifndef SENSOR4_TYPE
#define SENSOR4_TYPE SENSOR_TYPE_REED
#endif

#ifndef SENSOR5_TYPE
#define SENSOR5_TYPE SENSOR_TYPE_REED
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
  - reed contacts

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

static const uint8_t SENSOR_SLOT_COUNT = 5;
static const uint8_t SENSOR_BLOCK_SIZE = 5;
static const uint8_t PAYLOAD_HEADER_SIZE = 2;
static const uint16_t EEPROM_STORAGE_SIZE = 64;
static const uint8_t SENSOR_CFG_MAGIC_0 = 0x53;
static const uint8_t SENSOR_CFG_MAGIC_1 = 0x54;
static const uint8_t SENSOR_CFG_LAYOUT_VERSION = 1;
static const uint8_t SENSOR_CFG_OFFSET_MAGIC_0 = 0;
static const uint8_t SENSOR_CFG_OFFSET_MAGIC_1 = 1;
static const uint8_t SENSOR_CFG_OFFSET_VERSION = 2;
static const uint8_t SENSOR_CFG_OFFSET_TYPES = 3;
static const uint8_t SENSOR_CFG_OFFSET_CHECKSUM = SENSOR_CFG_OFFSET_TYPES + SENSOR_SLOT_COUNT;

static const uint8_t SENSOR_PINS[SENSOR_SLOT_COUNT] = {
  PIN_SENSOR1,
  PIN_SENSOR2,
  PIN_SENSOR3,
  PIN_SENSOR4,
  PIN_SENSOR5
};

static const uint8_t SENSOR_TYPES_DEFAULT[SENSOR_SLOT_COUNT] = {
  SENSOR1_TYPE,
  SENSOR2_TYPE,
  SENSOR3_TYPE,
  SENSOR4_TYPE,
  SENSOR5_TYPE
};

static uint8_t sensorTypesActive[SENSOR_SLOT_COUNT] = {
  SENSOR_TYPE_REED,
  SENSOR_TYPE_REED,
  SENSOR_TYPE_REED,
  SENSOR_TYPE_REED,
  SENSOR_TYPE_REED
};

struct SensorSlotRuntime {
  OneWire *oneWire = nullptr;
  DallasTemperature *ds18b20 = nullptr;
  DHT *dht = nullptr;
  DeviceAddress dsAddress;
  bool dsPresent = false;
};

static SensorSlotRuntime sensorSlots[SENSOR_SLOT_COUNT];
static uint8_t reedSensorSlots[SENSOR_SLOT_COUNT];
static uint8_t reedCount = 0;

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
static void detectDs18b20(uint8_t slot);
static uint8_t readReedBitmask();

static bool isValidSensorType(uint8_t type) {
  return type == SENSOR_TYPE_NONE ||
         type == SENSOR_TYPE_DHT22 ||
         type == SENSOR_TYPE_DS18B20 ||
         type == SENSOR_TYPE_REED;
}

static void loadDefaultSensorTypes() {
  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    sensorTypesActive[slot] = isValidSensorType(SENSOR_TYPES_DEFAULT[slot])
                                ? SENSOR_TYPES_DEFAULT[slot]
                                : SENSOR_TYPE_REED;
  }
}

static uint8_t computeTypesChecksum(const uint8_t *types) {
  uint8_t checksum = SENSOR_CFG_LAYOUT_VERSION ^ 0xA5;
  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    checksum ^= (uint8_t)(types[slot] + (slot * 17));
  }
  return checksum;
}

static bool loadSensorTypesFromStorage() {
  uint8_t magic0 = EEPROM.read(SENSOR_CFG_OFFSET_MAGIC_0);
  uint8_t magic1 = EEPROM.read(SENSOR_CFG_OFFSET_MAGIC_1);
  uint8_t version = EEPROM.read(SENSOR_CFG_OFFSET_VERSION);

  if (magic0 != SENSOR_CFG_MAGIC_0 || magic1 != SENSOR_CFG_MAGIC_1 || version != SENSOR_CFG_LAYOUT_VERSION) {
    return false;
  }

  uint8_t loadedTypes[SENSOR_SLOT_COUNT];
  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    loadedTypes[slot] = EEPROM.read(SENSOR_CFG_OFFSET_TYPES + slot);
    if (!isValidSensorType(loadedTypes[slot])) {
      return false;
    }
  }

  uint8_t storedChecksum = EEPROM.read(SENSOR_CFG_OFFSET_CHECKSUM);
  uint8_t expectedChecksum = computeTypesChecksum(loadedTypes);
  if (storedChecksum != expectedChecksum) {
    return false;
  }

  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    sensorTypesActive[slot] = loadedTypes[slot];
  }
  return true;
}

static void saveSensorTypesToStorage() {
  EEPROM.write(SENSOR_CFG_OFFSET_MAGIC_0, SENSOR_CFG_MAGIC_0);
  EEPROM.write(SENSOR_CFG_OFFSET_MAGIC_1, SENSOR_CFG_MAGIC_1);
  EEPROM.write(SENSOR_CFG_OFFSET_VERSION, SENSOR_CFG_LAYOUT_VERSION);

  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    EEPROM.write(SENSOR_CFG_OFFSET_TYPES + slot, sensorTypesActive[slot]);
  }

  EEPROM.write(SENSOR_CFG_OFFSET_CHECKSUM, computeTypesChecksum(sensorTypesActive));
  EEPROM.commit();
}

static void clearSlotRuntime(uint8_t slot) {
  SensorSlotRuntime &runtime = sensorSlots[slot];
  if (runtime.ds18b20 != nullptr) {
    delete runtime.ds18b20;
    runtime.ds18b20 = nullptr;
  }
  if (runtime.oneWire != nullptr) {
    delete runtime.oneWire;
    runtime.oneWire = nullptr;
  }
  if (runtime.dht != nullptr) {
    delete runtime.dht;
    runtime.dht = nullptr;
  }
  runtime.dsPresent = false;
}

static void detachReedInterrupts() {
  if (!reedInterruptsEnabled) {
    return;
  }

  for (uint8_t i = 0; i < reedCount; i++) {
    uint8_t slot = reedSensorSlots[i];
    detachInterrupt(SENSOR_PINS[slot]);
  }

  reedInterruptsEnabled = false;
}

static void printActiveSensorTypes() {
  Serial.printf("Active sensor types: [%u,%u,%u,%u,%u]\n",
                sensorTypesActive[0], sensorTypesActive[1], sensorTypesActive[2],
                sensorTypesActive[3], sensorTypesActive[4]);
}

static void initSensorRouting() {
  detachReedInterrupts();

  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    clearSlotRuntime(slot);
  }

  reedCount = 0;

  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    uint8_t type = sensorTypesActive[slot];
    uint8_t pin = SENSOR_PINS[slot];

    if (type == SENSOR_TYPE_DHT22) {
      sensorSlots[slot].dht = new DHT(pin, DHTTYPE);
      continue;
    }

    if (type == SENSOR_TYPE_DS18B20) {
      sensorSlots[slot].oneWire = new OneWire(pin);
      sensorSlots[slot].ds18b20 = new DallasTemperature(sensorSlots[slot].oneWire);
      continue;
    }

    if (type == SENSOR_TYPE_REED && reedCount < SENSOR_SLOT_COUNT) {
      reedSensorSlots[reedCount++] = slot;
    }
  }
}

static void initializeSensorHardware() {
  for (uint8_t i = 0; i < reedCount; i++) {
    uint8_t slot = reedSensorSlots[i];
    pinMode(SENSOR_PINS[slot], INPUT_PULLUP);
  }

  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    SensorSlotRuntime &runtime = sensorSlots[slot];
    if (runtime.ds18b20 != nullptr) {
      runtime.ds18b20->begin();
      detectDs18b20(slot);
    }
    if (runtime.dht != nullptr) {
      runtime.dht->begin();
    }
  }

  lastReedMask = readReedBitmask();
}

static bool isNetworkJoined() {
  return IsLoRaMacNetworkJoined;
}

static void enableReedInterruptsIfNeeded() {
  if (reedInterruptsEnabled || reedCount == 0) {
    return;
  }

  for (uint8_t i = 0; i < reedCount; i++) {
    uint8_t slot = reedSensorSlots[i];
    attachInterrupt(SENSOR_PINS[slot], onReedInterrupt, CHANGE);
  }
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

static void detectDs18b20(uint8_t slot) {
  if (slot >= SENSOR_SLOT_COUNT) {
    return;
  }

  SensorSlotRuntime &runtime = sensorSlots[slot];
  if (runtime.ds18b20 == nullptr) {
    runtime.dsPresent = false;
    return;
  }

  uint8_t deviceCount = runtime.ds18b20->getDeviceCount();
  bool found = runtime.ds18b20->getAddress(runtime.dsAddress, 0);
  runtime.dsPresent = found;

  if (found) {
    runtime.ds18b20->setResolution(runtime.dsAddress, 12);
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

  for (uint8_t i = 0; i < reedCount; i++) {
    uint8_t slot = reedSensorSlots[i];
    if (digitalRead(SENSOR_PINS[slot]) == LOW) {
      mask |= (1 << slot);
    }
  }

  return mask;
}

static bool readReedStateForSlot(uint8_t slot) {
  if (slot >= SENSOR_SLOT_COUNT || sensorTypesActive[slot] != SENSOR_TYPE_REED) {
    return false;
  }
  return digitalRead(SENSOR_PINS[slot]) == LOW;
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
  uint8_t reedMask = readReedBitmask();
  uint16_t battMv = readBatteryMillivolts();

  appData[0] = (uint8_t)(battMv >> 8);
  appData[1] = (uint8_t)(battMv & 0xFF);

  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    const uint8_t type = sensorTypesActive[slot];
    const uint8_t base = PAYLOAD_HEADER_SIZE + slot * SENSOR_BLOCK_SIZE;

    int16_t tempEnc = INT16_MIN;
    uint8_t dataEnc = 0xFF;
    uint8_t status = 0;

    if (type == SENSOR_TYPE_DS18B20) {
      SensorSlotRuntime &runtime = sensorSlots[slot];
      if (!runtime.dsPresent) {
        detectDs18b20(slot);
      }

      float temp = NAN;
      if (runtime.dsPresent && runtime.ds18b20 != nullptr) {
        runtime.ds18b20->requestTemperaturesByAddress(runtime.dsAddress);
        temp = runtime.ds18b20->getTempC(runtime.dsAddress);

        if (temp <= -126.0f || temp >= 125.0f || temp == DEVICE_DISCONNECTED_C) {
          delay(120);
          runtime.ds18b20->requestTemperaturesByAddress(runtime.dsAddress);
          temp = runtime.ds18b20->getTempC(runtime.dsAddress);
        }

        if (temp <= -126.0f || temp >= 125.0f || temp == DEVICE_DISCONNECTED_C) {
          runtime.dsPresent = false;
          temp = NAN;
        }
      }

      tempEnc = encodeTempC(temp);
      if (!isnan(temp)) {
        status |= (1 << 0);
      }
    } else if (type == SENSOR_TYPE_DHT22) {
      SensorSlotRuntime &runtime = sensorSlots[slot];
      float temp = NAN;
      float humidity = NAN;

      if (runtime.dht != nullptr) {
        for (uint8_t attempt = 0; attempt < 3; attempt++) {
          temp = runtime.dht->readTemperature();
          humidity = runtime.dht->readHumidity();
          if (!isnan(temp) && !isnan(humidity)) {
            break;
          }
          delay(120);
        }
      }

      tempEnc = encodeTempC(temp);
      dataEnc = encodeHumidity(humidity);
      if (!isnan(temp)) {
        status |= (1 << 0);
      }
      if (!isnan(humidity)) {
        status |= (1 << 1);
      }
    } else if (type == SENSOR_TYPE_REED) {
      bool reedClosed = readReedStateForSlot(slot);
      dataEnc = reedClosed ? 1 : 0;
      status |= (1 << 2);
      if (reedClosed) {
        status |= (1 << 3);
      }
    }

    appData[base + 0] = type;
    appData[base + 1] = status;
    appData[base + 2] = (uint8_t)(tempEnc >> 8);
    appData[base + 3] = (uint8_t)(tempEnc & 0xFF);
    appData[base + 4] = dataEnc;
  }

  appDataSize = PAYLOAD_HEADER_SIZE + SENSOR_SLOT_COUNT * SENSOR_BLOCK_SIZE;

  Serial.printf("Payload built, ReedMask: 0x%02X, VBAT: %u mV\n", reedMask, battMv);
}

static bool applySensorTypes(const uint8_t *newTypes, bool persist) {
  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    if (!isValidSensorType(newTypes[slot])) {
      return false;
    }
  }

  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    sensorTypesActive[slot] = newTypes[slot];
  }

  initSensorRouting();
  initializeSensorHardware();

  if (persist) {
    saveSensorTypesToStorage();
  }

  printActiveSensorTypes();
  immediateUplinkRequested = true;
  return true;
}

void downLinkDataHandle(McpsIndication_t *mcpsIndication) {
  if (mcpsIndication == nullptr || mcpsIndication->Buffer == nullptr || mcpsIndication->BufferSize == 0) {
    return;
  }

  if (mcpsIndication->Port != CFG_CONFIG_PORT) {
    return;
  }

  const uint8_t *buffer = mcpsIndication->Buffer;
  uint8_t length = mcpsIndication->BufferSize;

  if (length == 1) {
    Serial.printf("Config downlink ignored: single-byte commands disabled (0x%02X)\n", buffer[0]);
    return;
  }

  uint8_t payloadOffset = 0;

  if (length == SENSOR_SLOT_COUNT + 1) {
    if (buffer[0] != CMD_SET_SENSOR_TYPES) {
      Serial.printf("Config downlink ignored: unknown command 0x%02X\n", buffer[0]);
      return;
    }
    payloadOffset = 1;
  } else if (length != SENSOR_SLOT_COUNT) {
    Serial.printf("Config downlink ignored: invalid length %u\n", length);
    return;
  }

  uint8_t newTypes[SENSOR_SLOT_COUNT];
  for (uint8_t slot = 0; slot < SENSOR_SLOT_COUNT; slot++) {
    newTypes[slot] = buffer[payloadOffset + slot];
  }

  if (!applySensorTypes(newTypes, true)) {
    Serial.println("Config downlink ignored: invalid sensor type value");
    return;
  }

  Serial.printf("Sensor type config updated via downlink on port %u\n", CFG_CONFIG_PORT);
}

void prepareTxFrame(uint8_t port) {
  (void)port;
  buildPayload();
}

void setup() {
  Serial.begin(115200);
  autoLPM = false;

  EEPROM.begin(EEPROM_STORAGE_SIZE);
  loadDefaultSensorTypes();
  bool loadedFromStorage = loadSensorTypesFromStorage();
  if (!loadedFromStorage) {
    saveSensorTypesToStorage();
    Serial.println("No valid stored sensor config found, using defaults");
  } else {
    Serial.println("Loaded sensor config from storage");
  }

  initSensorRouting();
  initializeSensorHardware();

  pinMode(PIN_IMMEDIATE_TX, INPUT_PULLUP);
  printActiveSensorTypes();

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
