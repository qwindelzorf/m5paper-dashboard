// Copyright (c) 2021 HeRoMo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "init_mdns.h"

void initMDNS(const char* hostname)
{
    while (!MDNS.begin(hostname)) {
        delay(300);
    }
}
