# esp8266-mqtt-visca

ESP8266 Bridge to transfer WiFi/MQTT messages to RS232/VISCA.  
I use Cisco PrecisionHD (TTC8-02) cameras for development. Might work with other cameras.
![Cameras need eyes](https://raw.githubusercontent.com/JvPeek/esp8266-mqtt-visca/main/images/cams_smol.jpg)

## Usage

Here are some examples how to use the MQTT interface:

| Topic | example JSON | Outcome  |
|--------|----------------|---|
| visca/camera/command/moveto | <code>{<br>    x:400, <br>    y: 212, <br>    z: 0, <br>    focus: 420, <br>    cam: 0 <br>}<br></code> | Camera 0 moves to 400, 212, zooms all the way out, sets the focus to manual |
| visca/camera/command/settings | <code>{<br>    backlight: true, <br>    flip: true, <br>    mirror: true, <br>    mmdetect: true <br>}</code> | Camera 0 turns on backlight compensation, flips and mirrors the image and enables [EMFDP](# "external mechanical fuckery detection and prevention") |
| visca/camera/command/picture | <code>{<br>    wb:7,<br>    iris:-1,<br>    cam: 1<br>}<br></code | Camera 1 sets whitebalance to 7 and enables auto exposure |
| visca/camera/command/blinkenlights | <code>{<br>    led: 1, <br>    mode: 2, <br>    cam: 0 <br>}</code>``` | Camera 0 turns on LED 1 in blinking mode. |
| visca/system/command/getConfig | <code>{}</code> | Returns the current MQTT configuration |
| visca/system/command/updateConfig | <code>{<br>    "mqtt_server":"127.0.0.1",<br>    "mqtt_port":"1883",<br>    "mqtt_user":"test",<br>    "mqtt_password":"",<br>    "mqtt_basetopic":<br>    "VISCA"<br>}</code> | Update settings within the stored config.json on the microcontroller |
| visca/system/command/resetConfig | <code>{<br>    "reset":true<br>}</code> | Factory defaults.  |

## Hardware

- [D1 mini](https://www.wemos.cc/en/latest/d1/d1_mini.html) (any other ESP8266 will work. Haven't tested ESP32 boards yet)
- [max3232 breakout board](https://www.makershop.de/module/schnittstellen/max3232-mini/) (The DB9 version will also work)

__Somewhat important note: Do not use software serial for VISCA. Doesn't work. Makes you bang your head against the wall.__

## Resources for further development

[Cisco VISCA documentation](https://www.cisco.com/c/dam/en/us/td/docs/telepresence/endpoint/camera/precisionhd/user_guide/precisionhd_1080p-720p_camera_user_guide.pdf)

---
If you are looking for the rest of the documentation: this is the documentation so far. It ends here. Still a work in progress.
