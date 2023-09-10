#include <Arduino.h>
#include <camera.h>
#include <commands.h>

PTZCam cams[NUM_CAMS];


VISCACommand makePackage(byte* payload, uint8_t length, uint8_t camNum) {
    VISCACommand cmd;
    uint8_t charCount = 0;
    cmd.payload[charCount++] = 0x81 + camNum;
    for (uint8_t i = 0; i < length; i++) {
        cmd.payload[charCount++] = payload[i];
    }
    cmd.payload[charCount++] = 0xFF;
    cmd.len = charCount;
    return cmd;
}
VISCACommand blinkenlights(uint8_t led, uint8_t mode, uint8_t cam) {
    byte cmd[] = {0x01, 0x33, led, mode};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}

VISCACommand flip(bool setting, uint8_t cam) {
    byte cmd[] = {0x01, 0x04, 0x66, (setting ? 0x02 : 0x03)};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}
VISCACommand mirror(bool setting, uint8_t cam) {
    byte cmd[] = {0x01, 0x04, 0x61, (setting ? 0x02 : 0x03)};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}
VISCACommand backlight(bool setting, uint8_t cam) {
    byte cmd[] = {0x01, 0x04, 0x33, 0x02, (setting ? 0x02 : 0x03)};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}
VISCACommand mmdetect(bool setting, uint8_t cam) {
    byte cmd[] = {0x01, 0x50, 0x30, 0x01, setting};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}
void convertValues(uint input, byte* output) {
    output[0] = (input >> 12) & 0x0f;
    output[1] = (input >> 8) & 0x0f;
    output[2] = (input >> 4) & 0x0f;
    output[3] = input & 0x0f;
}
VISCACommand wb(int setting, uint8_t cam) {
    byte wbValues[4];
    convertValues(setting, wbValues);
    byte cmd[] = {
        0x01,       0x04,        0x35,        (setting <= -1 ? 0x00 : 0x06),
        0xff,       0x81 + cam,  0x01,        0x04,
        0x75,       wbValues[0], wbValues[1], wbValues[2],
        wbValues[3]};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}
VISCACommand iris(int setting, uint8_t cam) {
    byte irisValues[4];
    convertValues(setting, irisValues);
    byte cmd[] = {0x01,          0x04,
                  0x39,          (setting <= -1 ? 0x00 : 0x03),
                  0xff,          0x81 + cam,
                  0x01,          0x04,
                  0x4b,          irisValues[0],
                  irisValues[1], irisValues[2],
                  irisValues[3]};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}

VISCACommand relativeMovement(int x, int y, uint8_t cam) {
    // 01 06 01 0p 0t 03 01

    byte panDirection = 0x03;
    byte tiltDirection = 0x03;

    if (x > 0) {
        panDirection = 0x02;
    }
    if (x < 0) {
        panDirection = 0x01;
    }

    if (y > 0) {
        tiltDirection = 0x02;
    }
    if (y < 0) {
        tiltDirection = 0x01;
    }

    byte panSpeed = map(abs(x), 0, 100, 0x00, 0x1f);
    byte tiltSpeed = map(abs(y), 0, 100, 0x00, 0x1f);

    byte cmd[] = {0x01, 0x06, 0x01, 0x00, 0x00, 0x03, 0x03, 0xff, 0x81+cam, 0x01,      0x06,         0x01,         panSpeed,
                  tiltSpeed, panDirection, tiltDirection};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}
VISCACommand movement(uint8_t cam) {
    const uint x = cams[cam].getX();
    const uint y = cams[cam].getY();
    const uint z = cams[cam].getZ();
    const uint focus = cams[cam].getFocus();

    byte xValues[4];
    convertValues(x, xValues);

    byte yValues[4];
    convertValues(y, yValues);

    byte zValues[4];
    convertValues(z, zValues);

    byte focusValues[4];
    convertValues(focus, focusValues);

    byte cmd[] = {0x01,           0x04,
                  0x38,           (focus == -1 ? 0x02 : 0x03),
                  0xff,           (0x81 + cam),
                  0x01,           0x06,
                  0x01,           0x03,
                  0x03,           0x03,
                  0x03,           0xff,
                  (0x81 + cam),   0x01,
                  0x06,           0x20,
                  xValues[0],     xValues[1],
                  xValues[2],     xValues[3],

                  yValues[0],     yValues[1],
                  yValues[2],     yValues[3],
                  zValues[0],     zValues[1],
                  zValues[2],     zValues[3],
                  focusValues[0], focusValues[1],
                  focusValues[2], focusValues[3]};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);

    return command;
}

void requestEverything() {
    // 8x 09 06 12 ff request PT
    //
    byte cmd[] = {0x81, 0x09, 0x06, 0x12, 0xFF};
    for (uint8_t i = 0; i < NUM_CAMS; i++) {
        cmd[0] = 0x81 + i;
        Serial.write(cmd, sizeof(cmd));
    }
}

