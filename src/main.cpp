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

void debugPrint(String prompt) {}
void debugPrintln(String prompt) { debugPrint(prompt + "\n"); }
void handleSerial();

// flag for saving data
bool shouldSaveConfig = false;

void callback(char* topic, byte* payload, unsigned int length);

// define your default values here, if there are different values in
// config.json, they are overwritten. char mqtt_server[40];
#define mqtt_server "192.168.2.11"
#define mqtt_port "1883"
#define mqtt_topic "visca/raw"
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
            client.subscribe(mqtt_topic);
            client.publish(mqtt_topicresult, "ready");

        } else {
            debugPrint("failed, rc=");
            debugPrintln(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}
bool doSpam = false;
void spamLoop() {
    if (!doSpam) {
        return;
    }
    uint8_t command[32];

    command[0] = 0x88;
    command[1] = 0x01;
    command[2] = 0x04;
    command[3] = 0x00;
    command[4] = 0x03;
    command[5] = 0xff;

    for (uint8_t addr = 0x81; addr <= 0x88; addr++) {
        command[0] = addr;
        Serial.write(command, 6);
        Serial.flush();
    }
}
void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
    ArduinoOTA.handle();
    handleSerial();
    spamLoop();
}
void handleSerial() {
    bool newData = false;
    char buff[17] = {0};
    uint8_t packetSize = 0;
    while (Serial.available() > 0) {
        newData = true;
        Serial.readBytes(buff, Serial.available());
    }
    if (newData) {
        client.publish("visca/data", buff);
        client.publish("visca/debug", "Habe Daten gemacht");
    }
}

void callback(char* topic, byte* payload, unsigned int length) {

    Serial.write(payload, length);

    Serial.flush();
    client.publish("visca/debug", ("Kotze Daten " + String(length)).c_str());
}
