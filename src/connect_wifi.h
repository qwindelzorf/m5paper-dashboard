// Copyright (c) 2021 HeRoMo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <WiFi.h>

extern WiFiClient wifiClient;

bool connectWifi(const char* ssid, const char* password, unsigned timeout = 0);
