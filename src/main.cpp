#include "FS.h"
#include "NimBLEDevice.h"
#include "SPIFFS.h"
#include "battery_util.h"
#include "connect_wifi.h"
#include "init_mdns.h"
#include "prst_data.h"
#include "time_util.h"
#include <M5EPD.h>
#include <WiFi.h>
#include <cstddef>
#include <cstring>
#include <set>
#include <string>

#define FONT_SIZE 24
#define ROW_HEIGHT 60
#define ROW_PADDING 5
#define ROW_NUM(x) (x * (ROW_HEIGHT + ROW_PADDING))

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 540

rtc_time_t RTCtime;
rtc_date_t RTCDate;
char lastTime[23] = "0000/00/00 (000) 00:00";
char lastBattery[5] = "  0%";

void drawRow(int y, const char* text, int fontSize = 0, int fgcolor = 15, int bgcolor = 0)
{
    M5EPD_Canvas canvas(&M5.EPD);
    int margin = ROW_PADDING;
    if (fontSize == 0)
        fontSize = ROW_HEIGHT - margin * 2;

    canvas.loadFont("/font.ttf", SD);
    canvas.createRender(fontSize, 256);
    canvas.createCanvas(SCREEN_WIDTH, ROW_HEIGHT);
    canvas.fillCanvas(bgcolor);
    canvas.setTextSize(fontSize);
    canvas.setTextColor(fgcolor, bgcolor);
    canvas.drawString(text, 20, margin);
    canvas.pushCanvas(0, y, UPDATE_MODE_GLR16);
    canvas.deleteCanvas();
}
void drawHeader(int y, const char* title, int fgcolor = 15, int bgcolor = 0)
{
    drawRow(y, title, ROW_HEIGHT, fgcolor, bgcolor);
}

NimBLEScan* pBLEScan;
unsigned seen_devices = 0;
std::vector<prst_sensor_data_t> sensor_datas;

class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice)
    {
        seen_devices += 1;

        // This name decodes as 🌱
        if (advertisedDevice->getName() != "\xf0\x9f\x8c\xb1")
            return;

        auto numSDs = advertisedDevice->getServiceDataCount();
        if (numSDs != 1)
            return;

        char service_data[20];
        strncpy(service_data, advertisedDevice->getServiceData(0).c_str(), 20);
        auto sensor_data = prst_sensor_data_t::from_servicedata((uint8_t*)service_data);

        if (sensor_data.batt_mv == 0)
            return;
        if (sensor_data.mac_addr[0] < 0xd0)
            return;

        sensor_datas.push_back(sensor_data);
    }
};

void setup()
{
    M5.begin();
    M5.RTC.begin();
    M5.EPD.SetRotation(0);
    M5.EPD.Clear(true);

    drawHeader(ROW_NUM(0), "Loading...", 0, 15);

    if (SPIFFS.begin(false, "/")) {
        drawHeader(ROW_NUM(0), "SD Card mount failed!", 0, 15);
        delay(10000);
    }

    std::string tz_data = "MST7MDT,M3.2.0,M11.1.0";
    std::string ntp_server_1 = "pool.ntp.org";
    std::string ntp_server_2 = "time.nist.gov";
    std::string ntp_server_3 = "time.google.com";
    File tz_file = SPIFFS.open("/tz.txt", FILE_READ);
    tz_file.read();

    std::string wifi_ssid = "";
    std::string wifi_password = "";
    std::string hostname = "bprst-monitor";
    File wifi_file = SPIFFS.open("/wifi.txt", FILE_READ);
    connectWifi(wifi_ssid.c_str(), wifi_password.c_str());
    initMDNS(hostname);

    File sensor_file = SPIFFS.open("/sensors.txt", FILE_READ);

    configTime(0, 0, ntp_server_1.c_str(), ntp_server_2.c_str(), ntp_server_3.c_str());
    configTzTime(tz_data.c_str(), ntp_server_1.c_str(), ntp_server_2.c_str(), ntp_server_3.c_str());
    delay(2000);
    setupRTCTime();

    showDateTime(lastTime);
    showBattery(lastBattery);

    // "f1c4b4a2-2ccc-11ed-a261-0242ac120002"
    NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE);
    NimBLEDevice::setScanDuplicateCacheSize(200);
    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan(); //create new scan
    // Set the callback for when devices are discovered, no duplicates.
    pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(), false);
    pBLEScan->setActiveScan(true); // Set active scanning, this will get more data from the advertiser.
    pBLEScan->setInterval(97); // How often the scan occurs / switches channels; in milliseconds,
    pBLEScan->setWindow(37); // How long to scan during the interval; in milliseconds.
    pBLEScan->setMaxResults(0); // do not store the scan results, use callback only.

    drawHeader(ROW_NUM(1), "Devices", 0, 10);
}

const unsigned refresh_interval = 1000;

void showDeviceCounts()
{
    char seen_str[16];
    sprintf(seen_str, "%4d seen", (int)(seen_devices));
    char valid_str[16];
    sprintf(valid_str, "%4d valid", (int)(sensor_datas.size()));

    int width = 400;
    int height = 30;
    int bgcolor = 10;
    int fgcolor = 0;
    int fontSize = height;
    M5EPD_Canvas canvas(&M5.EPD);
    canvas.loadFont("/font.ttf", SD);
    canvas.createRender(fontSize, 256);
    canvas.createCanvas(width, height);
    canvas.fillCanvas(bgcolor);
    canvas.setTextSize(fontSize);
    canvas.setTextColor(fgcolor, bgcolor);

    canvas.drawString(seen_str, 0, 0);
    canvas.drawString(valid_str, width / 2, 0);

    canvas.pushCanvas(SCREEN_WIDTH - width - ROW_PADDING, ROW_HEIGHT + 25, UPDATE_MODE_A2);
    canvas.deleteCanvas();
}

void loop()
{
    M5.RTC.getTime(&RTCtime);
    showDateTime(lastTime);

    M5.RTC.getDate(&RTCDate);
    showBattery(lastBattery);

    showDeviceCounts();

    if (pBLEScan->isScanning() == false) {
        pBLEScan->start(refresh_interval, nullptr, false);
    }

    // Redraw device list
    unsigned idx = 0;
    for (const auto& sensor_data : sensor_datas) {
        const size_t line_len = 64;
        char line[line_len];
        sensor_data.to_str(line, line_len);
        drawRow(ROW_NUM(idx + 2), line, 30);
    }

    sensor_datas.clear();
    seen_devices = 0;
    delay(refresh_interval);
}
