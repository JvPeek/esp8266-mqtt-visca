# esp8266-mqtt-visca

ESP8266 Bridge to transfer WiFi/MQTT messages to RS232/VISCA.  
I use Cisco PrecisionHD (TTC8-02) cameras for development. Might work with other cameras.

![Cameras need eyes](https://raw.githubusercontent.com/JvPeek/esp8266-mqtt-visca/main/images/cams_smol.jpg)

## Usage

Here are some examples how to use the MQTT interface:

| Topic | example JSON | Outcome  |
|--------|----------------|---|
| visca/command/camera/moveto | ```{x: 400, y: 212, z: 0, focus: 420, cam: 0}``` | Camera 0 moves to 400, 212, zooms all the way out, sets the focus to manual |
| visca/command/camera/settings | ```{backlight: true, flip: true, mirror: true, mmdetect: true}``` | Camera 0 turns on backlight compensation, flips and mirrors the image and enables [EMFDP](# "external mechanical fuckery detection and prevention") |
| visca/command/camera/picture | ```{wb: 7, iris: -1, cam: 1}``` | Camera 1 sets whitebalance to 7 and enables auto exposure |
| visca/command/camera/blinkenlights | ```{led: 1, mode: 2, cam: 0}``` | Camera 0 turns on LED 1 in blinking mode |
| visca/command/system/getConfig | ```{}``` | Returns the current MQTT configuration |
| visca/command/system/updateConfig | ```{"mqtt_server": "127.0.0.1", "mqtt_port": "1883", "mqtt_user": "test", "mqtt_password": "", "mqtt_basetopic": "VISCA"}``` | Update settings within the stored config.json on the microcontroller |
| visca/command/system/resetConfig | ```{"reset": true}``` | Factory defaults |

## Hardware

- [D1 mini](https://www.wemos.cc/en/latest/d1/d1_mini.html) (any other ESP8266 will work. Haven't tested ESP32 boards yet)
- [max3232 breakout board](https://www.makershop.de/module/schnittstellen/max3232-mini/) (The DB9 version will also work)

__Somewhat important note: Do not use software serial for VISCA. Doesn't work. Makes you bang your head against the wall.__

## Resources for further development

[Cisco VISCA documentation](https://www.cisco.com/c/dam/en/us/td/docs/telepresence/endpoint/camera/precisionhd/user_guide/precisionhd_1080p-720p_camera_user_guide.pdf)

---
If you are looking for the rest of the documentation: this is the documentation so far. It ends here. Still a work in progress.
