// Copyright (c) 2021 HeRoMo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef _BATTERY_UTIL_H_
#define _BATTERY_UTIL_H_

#include <M5EPD.h>
#include <string>

void showBattery();
std::string battery_icon(float pct);

#endif
