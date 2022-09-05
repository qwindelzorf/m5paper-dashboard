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
#include <map>
#include <set>
#include <string>

#define ROW_NUM(x) (x * (ROW_HEIGHT + ROW_PADDING))

using namespace std;

string FONT_FACE = "/font.ttf";
int FONT_SIZE = 42;
int ROW_HEIGHT = 60;
int ROW_PADDING = 5;
unsigned REFRESH_INTERVAL = 1000;

const int SCREEN_WIDTH = 960;
const int SCREEN_HEIGHT = 540;

rtc_time_t RTCtime;
rtc_date_t RTCDate;

void drawRow(int y, const char* text, int fontSize = 0, int fgcolor = 15, int bgcolor = 0)
{
    M5EPD_Canvas canvas(&M5.EPD);
    int margin = ROW_PADDING;
    if (fontSize == 0) fontSize = ROW_HEIGHT - margin * 2;

    canvas.loadFont(FONT_FACE.c_str(), SD);
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
vector<prst_sensor_data_t> sensor_datas;

class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice)
    {
        seen_devices += 1;

        // This name decodes as 🌱
        if (advertisedDevice->getName() != "\xf0\x9f\x8c\xb1") return;

        auto numSDs = advertisedDevice->getServiceDataCount();
        if (numSDs != 1) return;

        char service_data[20];
        strncpy(service_data, advertisedDevice->getServiceData(0).c_str(), 20);
        auto sensor_data = prst_sensor_data_t::from_servicedata((uint8_t*)service_data);

        if (sensor_data.batt_mv == 0) return;
        if (sensor_data.mac_addr[0] < 0xd0) return;

        sensor_datas.push_back(sensor_data);
    }
};

std::map<string, string> read_file_to_map(File& in_file)
{
    std::map<string, string> kv_pairs;
    auto k = string(in_file.readStringUntil(':').c_str());
    auto v = string(in_file.readStringUntil('\n').c_str());
    kv_pairs[k] = v;
    return kv_pairs;
}

void setup()
{
    M5.begin();
    M5.RTC.begin();
    M5.EPD.SetRotation(0);
    M5.EPD.Clear(true);

    File config_file = SPIFFS.open("/config.txt", FILE_READ);
    if (config_file.available()) {
        auto config_data = read_file_to_map(config_file);
        FONT_FACE = string("/") + config_data["font_face"];
        FONT_SIZE = stoi(config_data["font_size"]);
        ROW_HEIGHT = stoi(config_data["row_height"]);
        ROW_PADDING = stoi(config_data["row_padding"]);
        REFRESH_INTERVAL = stoi(config_data["refresh_interval"]);
    }

    drawHeader(ROW_NUM(0), "Loading...", 0, 15);

    if (SPIFFS.begin(false, "/")) {
        drawHeader(ROW_NUM(0), "SD Card mount failed!", 0, 15);
        delay(10000);
    }

    File wifi_file = SPIFFS.open("/wifi.txt", FILE_READ);
    if (wifi_file.available()) {
        auto wifi_data = read_file_to_map(wifi_file);
        string wifi_ssid = wifi_data["wifi_ssid"];         //"";
        string wifi_password = wifi_data["wifi_password"]; //"";
        string hostname = wifi_data["hostname"];           //"bprst-monitor";
        if (!(wifi_ssid.empty() && wifi_password.empty())) {
            bool connected = connectWifi(wifi_ssid.c_str(), wifi_password.c_str(), 10000);
            if (connected) initMDNS(hostname.c_str());
        }
    }

    File tz_file = SPIFFS.open("/tz.txt", FILE_READ);
    if (tz_file.available()) {
        auto tz_data = read_file_to_map(tz_file);
        string tz = tz_data["tz"];                     //"MST7MDT,M3.2.0,M11.1.0";
        string ntp_server_1 = tz_data["ntp_server_1"]; //"pool.ntp.org";
        string ntp_server_2 = tz_data["ntp_server_2"]; //"time.nist.gov";
        string ntp_server_3 = tz_data["ntp_server_3"]; //"time.google.com";
        if (!(tz.empty())) {
            configTime(0, 0, ntp_server_1.c_str(), ntp_server_2.c_str(), ntp_server_3.c_str());
            configTzTime(tz.c_str(), ntp_server_1.c_str(), ntp_server_2.c_str(), ntp_server_3.c_str());
            delay(2000);
        }
    }
    setupRTCTime();
    showDateTime();
    showBattery();

    File sensor_file = SPIFFS.open("/sensors.txt", FILE_READ);
    auto sensor_data = read_file_to_map(sensor_file);

    // "f1c4b4a2-2ccc-11ed-a261-0242ac120002"
    NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE);
    NimBLEDevice::setScanDuplicateCacheSize(200);
    NimBLEDevice::init("");
    pBLEScan = NimBLEDevice::getScan(); // create new scan
    // Set the callback for when devices are discovered, no duplicates.
    pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(), false);
    pBLEScan->setActiveScan(true); // Set active scanning, this will get more data from the advertiser.
    pBLEScan->setInterval(97);     // How often the scan occurs / switches channels; in milliseconds,
    pBLEScan->setWindow(37);       // How long to scan during the interval; in milliseconds.
    pBLEScan->setMaxResults(0);    // do not store the scan results, use callback only.

    drawHeader(ROW_NUM(1), "Devices", 0, 10);
}

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
    canvas.loadFont(FONT_FACE.c_str(), SD);
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
    showDateTime();

    M5.RTC.getDate(&RTCDate);
    showBattery();

    showDeviceCounts();

    if (pBLEScan->isScanning() == false) {
        pBLEScan->start(REFRESH_INTERVAL, nullptr, false);
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
    delay(REFRESH_INTERVAL);
}
