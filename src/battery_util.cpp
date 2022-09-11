// Copyright (c) 2021 HeRoMo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "battery_util.h"

extern std::string FONT_FACE;
extern int FONT_SIZE;
extern int ROW_HEIGHT;
extern int ROW_PADDING;

void drawBattery(std::string battery)
{
  int width = 40;
  int height = ROW_HEIGHT;
  int bgcolor = 15;
  int fgcolor = 0;
  int fontSize = ROW_HEIGHT;
  M5EPD_Canvas canvas(&M5.EPD);
  canvas.loadFont(FONT_FACE.c_str(), SD);
  canvas.createRender(fontSize, 256);

  canvas.createCanvas(width, height);
  canvas.fillCanvas(bgcolor);
  canvas.setTextSize(fontSize);
  canvas.setTextColor(fgcolor, bgcolor);
  canvas.drawString(battery.c_str(), 0, 0);
  canvas.pushCanvas(960 - width - ROW_PADDING, 0, UPDATE_MODE_A2);
  canvas.deleteCanvas();
}

std::string battery_icon(float pct)
{
  if (pct > 95) return "";
  if (pct > 90) return "";
  if (pct > 80) return "";
  if (pct > 70) return "";
  if (pct > 60) return "";
  if (pct > 50) return "";
  if (pct > 40) return "";
  if (pct > 30) return "";
  if (pct > 20) return "";
  if (pct > 10) return "";
  return "";
}

std::string lastBattery;

void showBattery()
{
  uint32_t vol = M5.getBatteryVoltage();
  if (vol < 3300) {
    vol = 3300;
  } else if (vol > 4350) {
    vol = 4350;
  }
  float battery_pct = (float)(vol - 3300) / (float)(4350 - 3300) * 100.0f;
  battery_pct = min(battery_pct, 100.0f);
  battery_pct = max(battery_pct, 0.0f);

  auto currentBattery = battery_icon(battery_pct);
  if (currentBattery != lastBattery) {
    drawBattery(currentBattery);
    lastBattery = currentBattery;
  }
}
