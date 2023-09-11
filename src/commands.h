#pragma once
#include <camera.h>
#define VISCACOMMAND_MAX_LENGTH 128

struct VISCACommand {
    uint8_t len;
    uint8_t payload[VISCACOMMAND_MAX_LENGTH];
};

void convertValues(uint input, byte* output);
void parseCommand(uint8_t* command, int length);
void requestEverything();
void handleSerial();

void handleCommands(char* topic, byte* payload, unsigned int length);

VISCACommand makePackage(byte* payload, uint8_t length, uint8_t camNum);
VISCACommand blinkenlights(uint8_t led = 0, uint8_t mode = 0, uint8_t cam = 0);
VISCACommand flip(bool setting = 0, uint8_t cam = 0);
VISCACommand mirror(bool setting = 0, uint8_t cam = 0);
VISCACommand backlight(bool setting = 0, uint8_t cam = 0);
VISCACommand mmdetect(bool setting = 0, uint8_t cam = 0);
VISCACommand wb(int setting = 0, uint8_t cam = 0);
VISCACommand iris(int setting = 0, uint8_t cam = 0);
VISCACommand relativeMovement(int x, int y, uint8_t cam = 0);

VISCACommand movement(uint8_t cam = 0);
