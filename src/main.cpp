#include "FS.h"
#include "NimBLEDevice.h"
#include "SPIFFS.h"
#include "TodoView.h"
#include "battery_util.h"
#include "connect_wifi.h"
#include "headers.h"
#include "init_mdns.h"
#include "time_util.h"
#include <ArduinoJson.h>
#include <M5EPD.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <cstring>
#include <set>
#include <string>

// #define WIFI_SSID // define in platformio.ini
// #define WIFI_PASSWORD // define in platformio.ini

#define FONT_SIZE 24
#define ROW_HEIGHT 60
#define ROW_PADDING 5
#define ROW_NUM(x) (x * (ROW_HEIGHT + ROW_PADDING))

static const int MDT = 3600 * -8;
static const int MST = 3600 * -7;

RTC_SLOW_ATTR bool ntpDataFlag = false;
rtc_time_t RTCtime;
rtc_date_t RTCDate;
char lastTime[23] = "0000/00/00 (000) 00:00";
char lastBattery[5] = "  0%";

void drawRow(int y, const char* title, int fgcolor = 15, int bgcolor = 0)
{
    M5EPD_Canvas canvas(&M5.EPD);
    int margin = ROW_PADDING;
    int fontSize = ROW_HEIGHT - margin * 2;
    canvas.loadFont("/font.ttf", SD);
    canvas.createRender(fontSize, 256);
    canvas.createCanvas(960, ROW_HEIGHT);
    canvas.fillCanvas(bgcolor);
    canvas.setTextSize(fontSize);
    canvas.setTextColor(fgcolor, bgcolor);
    canvas.drawString(title, 20, margin);
    canvas.pushCanvas(0, y, UPDATE_MODE_GLR16);
    canvas.deleteCanvas();
}

NimBLEScan* pBLEScan;
static unsigned num_devices = 0;
std::vector<std::string> device_names;

class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice)
    {
        // Serial.printf("Advertised Device: %s \n", advertisedDevice->toString().c_str());
        device_names.push_back(advertisedDevice->getName());
    }
};

void setup()
{
    // Serial.begin(115200);

    M5.begin();
    M5.RTC.begin();
    M5.EPD.SetRotation(0);
    M5.EPD.Clear(true);

    drawRow(ROW_NUM(0), "", 0, 15);

    connectWifi(WIFI_SSID, WIFI_PASSWORD);
    initMDNS("bprst-monitor");

    configTime(MDT, 0, "pool.ntp.org", "time.nist.gov");
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
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
    pBLEScan->setActiveScan(true); // Set active scanning, this will get more data from the advertiser.
    pBLEScan->setInterval(97); // How often the scan occurs / switches channels; in milliseconds,
    pBLEScan->setWindow(37); // How long to scan during the interval; in milliseconds.
    pBLEScan->setMaxResults(0); // do not store the scan results, use callback only.

    drawRow(ROW_NUM(1), "Devices", 0, 10);
}

const unsigned refresh_interval = 1000;

void loop()
{
    M5.RTC.getTime(&RTCtime);
    showDateTime(lastTime);

    M5.RTC.getDate(&RTCDate);
    showBattery(lastBattery);

    if (pBLEScan->isScanning() == false) {
        // Start scan with: duration = 0 seconds(forever), no scan end callback, not a continuation of a previous scan.
        pBLEScan->start(refresh_interval, nullptr, false);
    }

    unsigned idx = 0;
    for (const auto& name : device_names) {
        drawRow(ROW_NUM(idx + 2), name.c_str());
    }

    delay(refresh_interval);
}
