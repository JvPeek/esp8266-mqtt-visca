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

#define MAXX 1023
#define MAXY 255
#define MAXZ 255
#define MAXF 255

void debugPrint(String prompt) {}
void debugPrintln(String prompt) { debugPrint(prompt + "\n"); }
void handleSerial();

#define VISCACOMMAND_MAX_LENGTH 64

struct VISCACommand {
    uint8_t len;
    uint8_t payload[VISCACOMMAND_MAX_LENGTH];
};

VISCACommand makePackage(byte* payload, uint8_t camNum);
VISCACommand blink(uint8_t led, uint8_t mode, uint8_t cam);

VISCACommand makePackage(byte* payload, uint8_t camNum) {
    VISCACommand cmd;
    uint8_t charCount = 0;
    cmd.len = 5;
    cmd.payload[charCount++] = 0x81 + camNum;
    for (uint8_t i = 0; i < sizeof(payload); i++) {
        cmd.payload[charCount++] = payload[i];
    }
    cmd.payload[charCount++] = 0xFF;
    cmd.len = charCount;
    return cmd;
}
VISCACommand blink(uint8_t led = 0, uint8_t mode = 0, uint8_t cam = 0) {
    byte cmd[] = {0x01, 0x33, led, mode};
    VISCACommand command = makePackage(cmd, cam);
    return command;
}

// flag for saving data
bool shouldSaveConfig = false;

void callback(char* topic, byte* payload, unsigned int length);

// define your default values here, if there are different values in
// config.json, they are overwritten. char mqtt_server[40];
#define mqtt_server "192.168.2.11"
#define mqtt_port "1883"
#define mqtt_topic "visca/command/#"
#define mqtt_topicresult "visca/status"

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
    //  wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99),
    //  IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    // add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);

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
    if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
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
        if (client.connect(clientId.c_str())) {
            debugPrint("connected");
            client.subscribe("visca/command/#");
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

/* todo:

PAN / TILT
ZOOM
FOCUS
FLIP

LED ON/OFF/BLINK

*/
class PTZCam {
   public:
    // Constructor
    PTZCam(int xValue = 1023 / 2, int yValue = 255 / 2, int zValue = 255 / 2,
           int focusValue = 255 / 2)
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
        newFocus = constrain(newFocus, 0, MAXF);
        focus = newFocus;
    }
    void moveCamera() {
        Serial.write(0x81);
        Serial.write(0x01);
        Serial.write(0x06);
        Serial.write(0x02);
        Serial.write(0x00);
        Serial.write(0x01);
        Serial.write(0x01);
        Serial.write(0x01);
        Serial.write(0x01);
        Serial.write(0x01);
        Serial.write(0x01);
        Serial.write(0x01);
    }

   private:
    int x;
    int y;
    int z;
    int focus;
};

PTZCam cams[7];
void handleSerial() {
    bool newData = false;
    char buff[17] = {0};
    while (Serial.available() > 0) {
        newData = true;
        Serial.readBytes(buff, Serial.available());
    }
    if (newData) {
        client.publish("visca/data", buff);
        client.publish(mqtt_topicresult, "Habe Daten gemacht");
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    DynamicJsonBuffer response(1024);
    JsonObject& responseObject = response.parseObject(payload);

    if (strcmp(topic, "visca/command/raw") == 0) {
        Serial.write(payload, length);

        client.publish(mqtt_topicresult,
                       ("Kotze Daten " + String(length)).c_str());
    }
    if (strcmp(topic, "visca/command/x") == 0) {
        cams[0].setX(2);
    }
    if (strcmp(topic, "visca/command/blink") == 0) {
        VISCACommand command = blink(responseObject["led"].as<uint8_t>(), responseObject["mode"].as<uint8_t>(), 0);
        Serial.write(command.payload, command.len);
    }
    Serial.flush();
}
