#ifndef _PRST_DATA_H_
#define _PRST_DATA_H_

#include <cstddef>
#include <cstdint>
#include <stdint.h>
#include <string>

#include "battery_util.h"

struct mac_addr_t {
  uint8_t bytes[6];

  std::string to_str() const
  {
    char mac_str[18];
    snprintf(mac_str, 18, "%02x-%02x-%02x-%02x-%02x-%02x", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
    return std::string(mac_str);
  };
};
inline bool operator==(const mac_addr_t& lhs, const mac_addr_t& rhs)
{
  for (int i = 0; i < 6; ++i) {
    if (lhs.bytes[i] != rhs.bytes[i]) {
      return false;
    }
  }
  return true;
}
inline bool operator!=(const mac_addr_t& lhs, const mac_addr_t& rhs)
{
  return !(lhs == rhs);
}

struct prst_sensor_data_t {
  uint16_t batt_mv;
  float temp_c;
  uint16_t humi;
  uint16_t soil_moisture;
  uint16_t light;
  uint8_t run_counter;
  mac_addr_t mac_addr;
  bool has_light_sensor;
  uint8_t protocol_version;
  std::string alias;
  unsigned long timestamp;

  const uint8_t supported_protocol_version = 2;

public:
  prst_sensor_data_t()
      : batt_mv(0)
      , temp_c(0)
      , humi(0)
      , soil_moisture(0)
      , light(0)
      , run_counter(0)
      , mac_addr({ 0, 0, 0, 0, 0, 0 })
      , has_light_sensor(false)
      , protocol_version(supported_protocol_version)
      , alias("")
      , timestamp(millis())
  {
  }
  prst_sensor_data_t(const prst_sensor_data_t& other)
      : batt_mv(other.batt_mv)
      , temp_c(other.temp_c)
      , humi(other.humi)
      , soil_moisture(other.soil_moisture)
      , light(other.light)
      , run_counter(other.run_counter)
      , mac_addr(other.mac_addr)
      , has_light_sensor(other.has_light_sensor)
      , protocol_version(other.protocol_version)
      , alias(other.alias)
      , timestamp(other.timestamp)
  {
  }
  prst_sensor_data_t(prst_sensor_data_t&& other)
      : batt_mv(other.batt_mv)
      , temp_c(other.temp_c)
      , humi(other.humi)
      , soil_moisture(other.soil_moisture)
      , light(other.light)
      , run_counter(other.run_counter)
      , mac_addr(other.mac_addr)
      , has_light_sensor(other.has_light_sensor)
      , protocol_version(other.protocol_version)
      , alias(other.alias)
      , timestamp(other.timestamp)
  {
    other.timestamp = 0;
    other.alias = "";
    other.mac_addr = { 0, 0, 0, 0, 0, 0 };
  }
  prst_sensor_data_t& operator=(const prst_sensor_data_t& other)
  {
    if (&other != this) {
      batt_mv = other.batt_mv;
      temp_c = other.temp_c;
      humi = other.humi;
      soil_moisture = other.soil_moisture;
      light = other.light;
      run_counter = other.run_counter;
      mac_addr = other.mac_addr;
      has_light_sensor = other.has_light_sensor;
      protocol_version = other.protocol_version;
      alias = other.alias;
      timestamp = other.timestamp;
    }
    return *this;
  }
  prst_sensor_data_t& operator=(prst_sensor_data_t&& other)
  {
    if (&other != this) {
      batt_mv = other.batt_mv;
      temp_c = other.temp_c;
      humi = other.humi;
      soil_moisture = other.soil_moisture;
      light = other.light;
      run_counter = other.run_counter;
      mac_addr = other.mac_addr;
      has_light_sensor = other.has_light_sensor;
      protocol_version = other.protocol_version;
      alias = other.alias;
      timestamp = other.timestamp;
    }
    return *this;
  }
  ~prst_sensor_data_t()
  {
  }

  float battery_pct() const
  {
    float batt_max = 3200.0;
    float batt_min = 2200.0;
    float pct = (batt_mv - batt_min) / (batt_max - batt_min) * 100.0;
    return pct;
  }

  static prst_sensor_data_t from_servicedata(const uint8_t* service_data)
  {
    prst_sensor_data_t sensor;

    // Four bits for the protocol version.
    sensor.protocol_version = service_data[0] >> 4;
    if (sensor.protocol_version != sensor.supported_protocol_version) {
      // Unsupported protocol version. Bail out now.
      return sensor;
    }

    // Bit 0 of byte 0 specifies whether or not ambient light data exists in the
    // payload.
    sensor.has_light_sensor = service_data[0] & 0x01;

    // 4 bits for a small wrap-around counter for deduplicating messages on the
    // receiver.
    sensor.run_counter = service_data[1] & 0x0f;

    sensor.batt_mv = service_data[2] << 8;
    sensor.batt_mv |= service_data[3];

    uint16_t temp_centicelsius = 0;
    temp_centicelsius = service_data[4] << 8;
    temp_centicelsius |= service_data[5];
    sensor.temp_c = temp_centicelsius / 100.0f;

    sensor.humi = service_data[6] << 8;
    sensor.humi |= service_data[7];

    sensor.soil_moisture = service_data[8] << 8;
    sensor.soil_moisture |= service_data[9];

    // Bytes 10-15 (inclusive) contain the whole MAC address in big-endian.
    for (int i = 0; i < 6; i++) {
      sensor.mac_addr.bytes[i] = service_data[10 + i];
    }

    if (sensor.has_light_sensor) {
      sensor.light = service_data[16] << 8;
      sensor.light |= service_data[17];
    }

    sensor.timestamp = millis();

    return sensor;
  };

  void to_str(char* str, size_t maxlen) const
  {
    std::string name = alias.empty() ? mac_addr.to_str() : alias;
    std::string batt_icon = battery_icon(battery_pct()).c_str();
    float soil_pct = soil_moisture / 655.35;
    float temp_f = (temp_c * 1.8f) + 32.0;
    float hum_pct = humi / 1000.0;
    float lux = light * 1.0f;

    if (has_light_sensor) {
      snprintf(str, maxlen, "%1s %-18s  —  %2.0f%%, %3.1f°F, %2.1f%%RH, %.0flux", batt_icon.c_str(), name.c_str(),
          soil_pct, temp_f, hum_pct, lux);
    } else {
      snprintf(str, maxlen, "%1s %-18s  —  %2.0f%%, %3.1f°F, %2.1f%%RH", batt_icon.c_str(), name.c_str(), soil_pct,
          temp_f, hum_pct);
    }
  };
};

#endif // _PRST_DATA_H_