// Copyright (c) 2021 HeRoMo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "time_util.h"

extern std::string FONT_FACE;
extern int FONT_SIZE;
extern int ROW_HEIGHT;
extern int ROW_PADDING;

time_t t;
struct tm* tm;

static const char* wd[7] = { "Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat" };

void drawDateTime(const char* timeStr)
{
    int width = 475;
    int height = ROW_HEIGHT;
    int bgcolor = 15;
    int fgcolor = 0;
    int ofsetY = (60 - height) / 2;
    int fontSize = ROW_HEIGHT;
    M5EPD_Canvas canvas(&M5.EPD);
    canvas.loadFont(FONT_FACE.c_str(), SD);
    canvas.createRender(fontSize, 256);

    canvas.createCanvas(width, height);
    canvas.fillCanvas(bgcolor);
    canvas.setTextSize(fontSize);
    canvas.setTextColor(fgcolor, bgcolor);
    canvas.drawString(timeStr, 0, 0);
    canvas.pushCanvas(20, ofsetY, UPDATE_MODE_A2);
    canvas.deleteCanvas();
}

void setupRTCTime()
{
    t = time(NULL);
    tm = localtime(&t);

    RTCtime.hour = tm->tm_hour;
    RTCtime.min = tm->tm_min;
    RTCtime.sec = tm->tm_sec;
    M5.RTC.setTime(&RTCtime);

    RTCDate.year = tm->tm_year + 1900;
    RTCDate.mon = tm->tm_mon + 1;
    RTCDate.day = tm->tm_mday;
    M5.RTC.setDate(&RTCDate);
}

char lastTime[23] = "0000/00/00 (000) 00:00";

void showDateTime()
{
    char currentTime[23];

    sprintf(currentTime, "%d/%02d/%02d (%s) %02d:%02d", RTCDate.year, RTCDate.mon, RTCDate.day, wd[RTCDate.week],
        RTCtime.hour, RTCtime.min);
    if (strcmp(lastTime, currentTime) != 0) {
        drawDateTime(currentTime);
        strcpy(lastTime, currentTime);
    }
}
