function decodeUplink(input) {
  const b = input.bytes;
  const SENSOR_COUNT = 5;
  const BLOCK_SIZE = 5;
  const HEADER_SIZE = 2;
  const expectedLen = HEADER_SIZE + SENSOR_COUNT * BLOCK_SIZE;

  if (b.length < expectedLen) {
    return { errors: ["Payload too short"] };
  }

  const i16 = (msb, lsb) => {
    let value = (msb << 8) | lsb;
    if (value & 0x8000) value -= 0x10000;
    return value;
  };

  const typeName = (type) => {
    if (type === 1) return "DHT22";
    if (type === 2) return "DS18B20";
    if (type === 3) return "REED";
    return "NONE";
  };

  const data = {
    battery_mv: (b[0] << 8) | b[1]
  };

  for (let i = 0; i < SENSOR_COUNT; i++) {
    const base = HEADER_SIZE + i * BLOCK_SIZE;
    const type = b[base + 0];
    const status = b[base + 1];
    const tempRaw = i16(b[base + 2], b[base + 3]);
    const slotData = b[base + 4];

    const tempValid = !!(status & 0x01);
    const humValid = !!(status & 0x02);
    const reedValid = !!(status & 0x04);
    const reedClosed = !!(status & 0x08);

    const sensor = {
      type,
      type_name: typeName(type)
    };

    if (type === 1) {
      sensor.temperature_c = tempValid ? tempRaw / 100 : null;
      sensor.humidity_pct = humValid && slotData !== 0xFF ? slotData / 2 : null;
    } else if (type === 2) {
      sensor.temperature_c = tempValid ? tempRaw / 100 : null;
    } else if (type === 3) {
      sensor.closed = reedValid ? reedClosed : slotData === 1;
    }

    data[`sensor_${i + 1}`] = sensor;
  }

  return { data };
}
