#include <Arduino.h>
#pragma once
#define MAXX 800
#define MAXY 212
#define MAXZ 2305
#define MAXF 65535
#define NUM_CAMS 7


class PTZCam {
   public:
    // Constructor
    PTZCam(int xValue = MAXX / 2, int yValue = MAXY / 2, int zValue = MAXZ / 2,
           int focusValue = MAXF / 2)
        : x(xValue), y(yValue), z(zValue), focus(focusValue) {}

    // Getter methods
    int getX() const { return x; }
    int getY() const { return y; }
    int getZ() const { return z; }
    int getFocus() const { return focus; }

    // Setter methods
    void setX(int newX) {
        newX = constrain(newX, 0, MAXX);
        x = newX;
    }
    void setY(int newY) {
        newY = constrain(newY, 0, MAXY);
        y = newY;
    }
    void setZ(int newZ) {
        newZ = constrain(newZ, 0, MAXZ);
        z = newZ;
    }
    void setFocus(int newFocus) {
        newFocus = constrain(newFocus, -1, MAXF);
        focus = newFocus;
    }

   private:
    int x;
    int y;
    int z;
    int focus;
};

extern PTZCam cams[NUM_CAMS];