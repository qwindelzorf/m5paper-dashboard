// Copyright (c) 2021 HeRoMo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
#include "connect_wifi.h"

WiFiClient wifiClient;

bool connectWifi(const char* ssid, const char* password, unsigned timeout)
{
  auto start = millis();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    if (timeout > 0 && (millis() - start) > timeout) {
      return false;
    }
  }
  return true;
}
