#ifndef _PRST_DATA_H_
#define _PRST_DATA_H_

#include <cstddef>
#include <cstdint>
#include <stdint.h>

#include "battery_util.h"

struct prst_sensor_data_t {
    uint16_t batt_mv;
    float temp_c;
    uint16_t humi;
    uint16_t soil_moisture;
    uint16_t lux;
    uint8_t run_counter;
    uint8_t mac_addr[6];
    bool has_light_sensor;
    uint8_t protocol_version;

    const uint8_t supported_protocol_version = 2;

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
            sensor.mac_addr[i] = service_data[10 + i];
        }

        if (sensor.has_light_sensor) {
            sensor.lux = service_data[16] << 8;
            sensor.lux |= service_data[17];
        }

        return sensor;
    };

    void to_str(char* str, size_t maxlen) const
    {
        if (has_light_sensor) {
            snprintf(str, maxlen, "%02x:%02x:%02x:%02x:%02x:%02x %s - Soil: %.0f%%, %.1fC, %.1f%%RH, %.0flux",
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
                battery_icon(battery_pct()).c_str(), soil_moisture / 655.35, temp_c, humi / 1000.0, lux * 1.0);
        } else {
            snprintf(str, maxlen, "%02x:%02x:%02x:%02x:%02x:%02x %s - Soil: %.0f%%, %.1fC, %.1f%%RH", mac_addr[0],
                mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], battery_icon(battery_pct()).c_str(),
                soil_moisture / 655.35, temp_c, humi / 1000.0);
        }
    };
};

#endif // _PRST_DATA_H_