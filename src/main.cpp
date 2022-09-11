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

#include "config_file.h"

#define ROW_NUM(x) (x * (ROW_HEIGHT + ROW_PADDING))

using namespace std;

string FONT_FACE = "/default.ttf";
int FONT_SIZE = 42;
int ROW_HEIGHT = 60;
int ROW_PADDING = 5;
unsigned REFRESH_INTERVAL = 1000;
string WIFI_SSID;
string WIFI_PASS;
string HOSTNAME;
int TEMPERATURE_CALIBRATION = 0;

bool WIFI_CONNECTED;
bool NTP_REFRESHED;

const int SCREEN_WIDTH = 960;
const int SCREEN_HEIGHT = 540;

rtc_time_t RTCtime;
rtc_date_t RTCDate;

void wifi_connect()
{
  if (!(WIFI_SSID.empty() && WIFI_PASS.empty()) && !WIFI_CONNECTED) {
    bool connected = connectWifi(WIFI_SSID.c_str(), WIFI_PASS.c_str(), 10000);
    if (connected) {
      initMDNS(HOSTNAME.c_str());
      WIFI_CONNECTED = true;
    } else {
      WIFI_CONNECTED = false;
    }
  }
}

void drawRow(const char* text, int y, int fontSize = 0, int fgcolor = 15, int bgcolor = 0)
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
void drawRow(const string& text, int y, int fontSize = 0, int fgcolor = 15, int bgcolor = 0)
{
  drawRow(text.c_str(), y, fontSize, fgcolor, bgcolor);
}
void drawHeader(const char* title, int y = 0, int fgcolor = 15, int bgcolor = 0)
{
  drawRow(title, y, ROW_HEIGHT, fgcolor, bgcolor);
}
void drawHeader(const string& title, int y = 0, int fgcolor = 15, int bgcolor = 0)
{
  drawHeader(title.c_str(), y, fgcolor, bgcolor);
}

void showWiFi()
{
  int width = 50;
  int height = ROW_HEIGHT;
  int bgcolor = 15;
  int fgcolor = 0;
  int fontSize = 45;

  wifi_connect();
  string wifi_conn = WIFI_CONNECTED ? "直" : "睊";

  M5EPD_Canvas canvas(&M5.EPD);
  canvas.loadFont(FONT_FACE.c_str(), SD);
  canvas.createRender(fontSize, 256);

  canvas.createCanvas(width, height);
  canvas.fillCanvas(bgcolor);
  canvas.setTextSize(fontSize);
  canvas.setTextColor(fgcolor, bgcolor);
  canvas.drawString(wifi_conn.c_str(), 0, ROW_PADDING);
  canvas.pushCanvas(SCREEN_WIDTH - 200 - width - ROW_PADDING, 0, UPDATE_MODE_A2);
  canvas.deleteCanvas();
}

void showTemperature()
{
  int width = 130;
  int height = ROW_HEIGHT;
  int bgcolor = 15;
  int fgcolor = 0;
  int fontSize = 45;

  M5.SHT30.UpdateData();
  float temp_c = M5.SHT30.GetTemperature();
  float temp_f = (temp_c * 1.8) + 32.0;
  temp_f += TEMPERATURE_CALIBRATION;
  char temperature[10];
  auto written = std::snprintf(temperature, 10, "%.0f°F", temp_f);

  M5EPD_Canvas canvas(&M5.EPD);
  canvas.loadFont(FONT_FACE.c_str(), SD);
  canvas.createRender(fontSize, 256);

  canvas.createCanvas(width, height);
  canvas.fillCanvas(bgcolor);
  canvas.setTextSize(fontSize);
  canvas.setTextColor(fgcolor, bgcolor);
  canvas.drawString(temperature, 0, ROW_PADDING);
  canvas.pushCanvas(SCREEN_WIDTH - 50 - width - ROW_PADDING, 0, UPDATE_MODE_A2);
  canvas.deleteCanvas();
}

NimBLEScan* pBLEScan;
unsigned seen_devices = 0;
vector<prst_sensor_data_t> sensor_datas;
std::map<std::string, std::string> sensor_names;

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

    if (sensor_data.mac_addr.bytes[0] < 0xd0) return;
    if (sensor_data.batt_mv == 0) return;

    sensor_data.alias = value_or(sensor_data.mac_addr.to_str(), sensor_names, "");

    sensor_datas.push_back(sensor_data);
  }
};

void setup()
{
  M5.begin();
  M5.RTC.begin();
  M5.EPD.SetRotation(0);
  M5.EPD.Clear(true);

  if (!SD.begin()) {
    drawHeader("Failed to start filesystem");
    delay(5000);
  } else {
    drawHeader("Loading...", 0, 0, 15);

    SDFile config_file = SD.open("/config.txt", FILE_READ);
    if (config_file.available()) {
      drawRow("Reading config.txt", ROW_NUM(1));
      auto config_data = read_file_to_map(config_file);
      FONT_FACE = string("/") + value_or("font_face", config_data, FONT_FACE);
      FONT_SIZE = has_key("font_size", config_data) ? stoi(config_data["font_size"]) : FONT_SIZE;
      ROW_HEIGHT = has_key("row_height", config_data) ? stoi(config_data["row_height"]) : ROW_HEIGHT;
      ROW_PADDING = has_key("row_padding", config_data) ? stoi(config_data["row_padding"]) : ROW_PADDING;
      TEMPERATURE_CALIBRATION = has_key("temperature_calibration", config_data)
          ? stoi(config_data["temperature_calibration"])
          : TEMPERATURE_CALIBRATION;
      REFRESH_INTERVAL =
          has_key("refresh_interval", config_data) ? stoi(config_data["refresh_interval"]) : REFRESH_INTERVAL;
    } else {
      drawHeader("Failed to open config.txt");
      delay(5000);
    }

    SDFile wifi_file = SD.open("/wifi.txt", FILE_READ);
    if (wifi_file.available()) {
      drawRow("Reading wifi.txt", ROW_NUM(1));
      auto wifi_data = read_file_to_map(wifi_file);
      WIFI_SSID = value_or("wifi_ssid", wifi_data, "");
      WIFI_PASS = value_or("wifi_password", wifi_data, "");

      HOSTNAME = value_or("hostname", wifi_data, "bprst-monitor");
      drawRow((string("Connecting to: '") + WIFI_SSID), ROW_NUM(2));
      wifi_connect();
    } else {
      drawHeader("Failed to open wifi.txt");
      delay(5000);
    }

    SDFile tz_file = SD.open("/tz.txt", FILE_READ);
    if (tz_file.available()) {
      drawRow("Reading tz.txt", ROW_NUM(1));
      auto tz_data = read_file_to_map(tz_file);
      string tz = value_or("tz", tz_data, "MST7MDT,M3.2.0,M11.1.0");
      string ntp_server_1 = value_or("ntp_server_1", tz_data, "pool.ntp.org");
      string ntp_server_2 = value_or("ntp_server_2", tz_data, "time.nist.gov");
      string ntp_server_3 = value_or("ntp_server_3", tz_data, "time.google.com");
      NTP_REFRESHED = false;
      if (!(tz.empty()) && WIFI_CONNECTED) {
        configTime(0, 0, ntp_server_1.c_str(), ntp_server_2.c_str(), ntp_server_3.c_str());
        configTzTime(tz.c_str(), ntp_server_1.c_str(), ntp_server_2.c_str(), ntp_server_3.c_str());
        NTP_REFRESHED = true;
        delay(2000);
      }
    } else {
      drawHeader("Failed to open tz.txt");
      delay(5000);
    }

    SDFile sensor_file = SD.open("/sensors.txt", FILE_READ);
    if (sensor_file.available()) {
      drawRow("Reading sensors.txt", ROW_NUM(1));
      auto sensor_data = read_file_to_map(sensor_file);
      int idx = 1;
      for (const auto& pair : sensor_data) {
        sensor_names[pair.first] = pair.second;
        drawRow((pair.first + string(" => ") + pair.second).c_str(), ROW_NUM(idx++));
      }
      delay(5000);
    } else {
      drawHeader("Failed to open sensors.txt");
      delay(5000);
    }
  }

  setupRTCTime();

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

  M5.EPD.Clear(true);

  drawHeader("", ROW_NUM(0), 0, 15);
  showDateTime();
  showBattery();
  showTemperature();
  drawHeader("Devices", ROW_NUM(1), 0, 10);
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

  showTemperature();

  showWiFi();

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

    drawRow(line, ROW_NUM(idx + 2), 30);
  }

  sensor_datas.clear();
  seen_devices = 0;
  delay(REFRESH_INTERVAL);
}
