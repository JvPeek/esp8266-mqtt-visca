#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>  //https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>
#include <FS.h>  //this needs to be first, or it all crashes and burns...

// needed for library

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>

#define MAXX 800
#define MAXY 212
#define MAXZ 2305
#define MAXF 65535


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

PTZCam cams[7];

void debugPrint(String prompt) {}
void debugPrintln(String prompt) { debugPrint(prompt + "\n"); }
void handleSerial();

#define VISCACOMMAND_MAX_LENGTH 128

struct VISCACommand {
    uint8_t len;
    uint8_t payload[VISCACOMMAND_MAX_LENGTH];
};

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
VISCACommand blinkenlights(uint8_t led = 0, uint8_t mode = 0, uint8_t cam = 0) {
    byte cmd[] = {0x01, 0x33, led, mode};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}

VISCACommand flip(bool setting = 0, uint8_t cam = 0) {
    byte cmd[] = {0x01, 0x04, 0x66, (setting ? 0x02 : 0x03)};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}
VISCACommand mirror(bool setting = 0, uint8_t cam = 0) {
    byte cmd[] = {0x01, 0x04, 0x61, (setting ? 0x02 : 0x03)};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}
VISCACommand backlight(bool setting = 0, uint8_t cam = 0) {
    byte cmd[] = {0x01, 0x04, 0x33, 0x02, (setting ? 0x02 : 0x03)};
    VISCACommand command = makePackage(cmd, sizeof(cmd), cam);
    return command;
}
VISCACommand mmdetect(bool setting = 0, uint8_t cam = 0) {
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
VISCACommand wb(int setting = 0, uint8_t cam = 0) {
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
VISCACommand iris(int setting = 0, uint8_t cam = 0) {
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

VISCACommand relativeMovement(int x, int y, uint8_t cam = 0) {
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
VISCACommand movement(uint8_t cam = 0) {
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

// flag for saving data
bool shouldSaveConfig = false;

void callback(char* topic, byte* payload, unsigned int length);

// define your default values here, if there are different values in
// config.json, they are overwritten. char mqtt_server[40];
#define mqtt_server "192.168.10.15"
#define mqtt_user "user"
#define mqtt_password "pass"
#define mqtt_port "1883"
#define mqtt_topic "viscadev/command/#"
#define mqtt_topicresult "viscadev/status"

WiFiClient espClient;
PubSubClient client(espClient);

// callback notifying us of the need to save config
void saveConfigCallback() {
    debugPrintln("Should save config");
    shouldSaveConfig = true;
}
void setup() {
    Serial.begin(9600);

    // put your setup code here, to run once:

    debugPrint("MAC: ");
    debugPrintln(WiFi.macAddress());
    WiFi.mode(WIFI_STA);
    // clean FS for testing

    // read configuration from FS json
    debugPrintln("mounting FS...");

    if (SPIFFS.begin()) {
        debugPrintln("mounted file system");
        if (SPIFFS.exists("/config.json")) {
            // file exists, reading and loading
            debugPrintln("reading config file");
            File configFile = SPIFFS.open("/config.json", "r");
            if (configFile) {
                debugPrintln("opened config file");
                size_t size = configFile.size();
                // Allocate a buffer to store contents of the file.
                std::unique_ptr<char[]> buf(new char[size]);

                configFile.readBytes(buf.get(), size);
                DynamicJsonBuffer jsonBuffer;
                JsonObject& json = jsonBuffer.parseObject(buf.get());
                json.printTo(Serial);
                if (json.success()) {
                    debugPrintln("\nparsed json");
                    strcpy(mqtt_server, json["mqtt_server"]);
                    strcpy(mqtt_port, json["mqtt_port"]);
                    strcpy(mqtt_user, json["mqtt_user"]);
                    strcpy(mqtt_password, json["mqtt_password"]);
                } else {
                    debugPrintln("failed to load json config");
                }
            }
        }
    } else {
        debugPrintln("failed to mount FS");
    }
    // end read

    // The extra parameters to be configured (can be either global or just in
    // the setup) After connecting, parameter.getValue() will get you the
    // configured value id/name placeholder/prompt default length
    WiFiManagerParameter custom_mqtt_server("server", "mqtt server",
                                            mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
    WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 40);
    WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 40);
    // WiFiManager
    // Local intialization. Once its business is done, there is no need to keep
    // it around
    WiFiManager wifiManager;

    // Reset Wifi settings for testing
    //  wifiManager.resetSettings();
    // set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    // SPIFFS.format();
    // wifiManager.resetSettings();

    // set static ip
    //  wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99),platform = esp

    //  IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    // add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_password);

    // reset settings - for testing
    // wifiManager.resetSettings();

    // set minimum quality of signal so it ignores AP's under that quality
    // defaults to 8%
    // wifiManager.setMinimumSignalQuality();

    // sets timeout until configuration portal gets turned off
    // useful to make it all retry or go to sleep
    // in seconds
    // wifiManager.setTimeout(120);

    // fetches ssid and pass and tries to connect
    // if it does not connect it starts an access point with the specified name
    // here  "AutoConnectAP"
    // and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect("KatzenWuerden", "viscakaufen")) {
        debugPrintln("failed to connect and hit timeout");
        delay(3000);
        // reset and try again, or maybe put it to deep sleep
        ESP.reset();
        delay(5000);
    }

    // if you get here you have connected to the WiFi
    debugPrintln("connected...yeey :)");

    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname("ViscaBridge");

    // No authentication by default
    // ArduinoOTA.setPassword((const char *)"123");

    ArduinoOTA.onStart([]() { debugPrintln("Start"); });
    ArduinoOTA.onEnd([]() { debugPrintln("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
    ArduinoOTA.onError([](ota_error_t error) {
        if (error == OTA_AUTH_ERROR)
            debugPrintln("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            debugPrintln("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            debugPrintln("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            debugPrintln("Receive Failed");
        else if (error == OTA_END_ERROR)
            debugPrintln("End Failed");
    });
    ArduinoOTA.begin();

    // read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());

    // save the custom parameters to FS
    if (shouldSaveConfig) {
        debugPrintln("saving config");
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.createObject();
        json["mqtt_server"] = mqtt_server;
        json["mqtt_port"] = mqtt_port;
        json["mqtt_user"] = mqtt_user;
        json["mqtt_password"] = mqtt_password;

        File configFile = SPIFFS.open("/config.json", "w");
        if (!configFile) {
            debugPrintln("failed to open config file for writing");
        }

        json.printTo(Serial);
        json.printTo(configFile);
        configFile.close();
        // end save
    }
    debugPrintln("local ip");
    const uint16_t mqtt_port_x = 1883;
    client.setServer(mqtt_server, mqtt_port_x);

    client.setCallback(callback);
}

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
        debugPrint("Attempting MQTT connection...");

        // Attempt to connect
        // If you do not want to use a username and password, change next line
        // to if (client.connect("ESP8266Client")) {

        String clientId = "VISCABridge-";
        clientId += String(random(0xffff), HEX);
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
            debugPrint("connected");
            client.subscribe("viscadev/command/#");
            client.publish(mqtt_topicresult, "ready");

        } else {
            debugPrint("failed, rc=");
            debugPrintln(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}
void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
    ArduinoOTA.handle();
    handleSerial();
}
void parseCommand(char* command) {

    client.publish("viscadev/return/raw", command);
    
}
void handleSerial() {
    static enum { IDLE, RECEIVING } state = IDLE;
    static char buff[17] = {0};
    static int buffIndex = 0;
    
    while (Serial.available() > 0) {
        char receivedByte = Serial.read();
        
        switch (state) {
            case IDLE:
                if (receivedByte == 0x90) {
                    state = RECEIVING;
                    buffIndex = 0;
                    buff[buffIndex++] = receivedByte;
                }
                break;
                
            case RECEIVING:
                buff[buffIndex++] = receivedByte;
                if (receivedByte == 0xff) {
                    // Full command received, call parseCommand
                    parseCommand(buff);
                    state = IDLE;
                }
                break;
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    DynamicJsonBuffer response(1024);
    JsonObject& responseObject = response.parseObject(payload);
    if (!responseObject.containsKey("cam")) {
        responseObject.set("cam", 0);
    }
    uint8_t camNum = responseObject["cam"].as<uint8_t>();

    if (strcmp(topic, "viscadev/command/raw") == 0) {
        Serial.write(payload, length);
        client.publish(mqtt_topicresult,
                       ("Kotze Daten " + String(length)).c_str());
    }
    if (strcmp(topic, "viscadev/command/blinkenlights") == 0) {
        VISCACommand command =
            blinkenlights(responseObject["led"].as<uint8_t>(),
                          responseObject["mode"].as<uint8_t>(),
                          responseObject["cam"].as<uint8_t>());
        Serial.write(command.payload, command.len);
    }
    if (strcmp(topic, "viscadev/command/settings") == 0) {
        if (responseObject.containsKey("backlight")) {
            VISCACommand command =
                backlight(responseObject["backlight"].as<bool>(),
                          responseObject["cam"].as<uint8_t>());
            Serial.write(command.payload, command.len);
        }

        if (responseObject.containsKey("mirror")) {
            VISCACommand command = mirror(responseObject["mirror"].as<bool>(),
                                          responseObject["cam"].as<uint8_t>());
            Serial.write(command.payload, command.len);
        }
        if (responseObject.containsKey("flip")) {
            VISCACommand command = flip(responseObject["flip"].as<bool>(),
                                        responseObject["cam"].as<uint8_t>());
            Serial.write(command.payload, command.len);
        }

        if (responseObject.containsKey("mmdetect")) {
            VISCACommand command =
                mmdetect(responseObject["mmdetect"].as<bool>(),
                         responseObject["cam"].as<uint8_t>());
            Serial.write(command.payload, command.len);
        }

        //  ir_output, ir_cameracontrol
    }

    if (strcmp(topic, "viscadev/command/picture") == 0) {
        if (responseObject.containsKey("wb")) {
            VISCACommand command = wb(responseObject["wb"].as<int>(),
                                      responseObject["cam"].as<uint8_t>());
            Serial.write(command.payload, command.len);
        }
        if (responseObject.containsKey("iris")) {
            VISCACommand command = iris(responseObject["iris"].as<int>(),
                                        responseObject["cam"].as<uint8_t>());
            Serial.write(command.payload, command.len);
        }
    }
    if (strcmp(topic, "viscadev/command/moveto") == 0) {
        if (responseObject.containsKey("x")) {
            cams[responseObject["cam"].as<uint8_t>()].setX(
                responseObject["x"].as<int>());
        }
        if (responseObject.containsKey("y")) {
            cams[responseObject["cam"].as<uint8_t>()].setY(
                responseObject["y"].as<int>());
        }
        if (responseObject.containsKey("z")) {
            cams[responseObject["cam"].as<uint8_t>()].setZ(
                responseObject["z"].as<int>());
        }
        if (responseObject.containsKey("focus")) {
            cams[responseObject["cam"].as<uint8_t>()].setFocus(
                responseObject["focus"].as<int>());
        }
        VISCACommand command = movement(responseObject["cam"].as<uint8_t>());

        Serial.write(command.payload, command.len);
    }

    if (strcmp(topic, "viscadev/command/moveby") == 0) {
        if (!responseObject.containsKey("x")) {
            responseObject.set("x", 0);
        }
        if (!responseObject.containsKey("y")) {
            responseObject.set("y", 0);
        }

        VISCACommand command = relativeMovement(
            responseObject["x"].as<int>(), responseObject["y"].as<int>(),
            responseObject["cam"].as<uint8_t>());

        Serial.write(command.payload, command.len);
        client.publish("viscadev/rawdata", command.payload, command.len);

        
    }
    Serial.flush();
}
