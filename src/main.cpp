#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>  //https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>
//#include <FS.h>  //this needs to be first, or it all crashes and burns...
#include "LittleFS.h"
// needed for library

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>

#include <camera.h>
#include <commands.h>

SoftwareSerial visca(D1,D2);


void debugPrint(String prompt) {}
void debugPrintln(String prompt) { debugPrint(prompt + "\n"); }
void handleSerial();


long lastRequestTime = 0;

// flag for saving data
bool shouldSaveConfig = false;

void callback(char* topic, byte* payload, unsigned int length);

// define your default values here, if there are different values in
// config.json, they are overwritten. char mqtt_server[40];
String mqtt_server;
uint16_t mqtt_port;
String mqtt_user;
String mqtt_password;
String mqtt_basetopic;

WiFiClient espClient;
PubSubClient client(espClient);

// callback notifying us of the need to save config
void saveConfigCallback() {
    debugPrintln("Should save config");
    shouldSaveConfig = true;
}
void setup() {
    Serial.begin(9600);
    visca.begin(9600);
    // put your setup code here, to run once:
    
    debugPrint("MAC: ");
    debugPrintln(WiFi.macAddress());
    WiFi.mode(WIFI_STA);
    // clean FS for testing

    // read configuration from FS json
    debugPrintln("mounting FS...");

    if (LittleFS.begin()) {
        debugPrintln("mounted file system");
        if (LittleFS.exists("/config.json")) {
            // file exists, reading and loading
            debugPrintln("reading config file");
            File configFile = LittleFS.open("/config.json", "r");
            if (configFile) {
                debugPrintln("opened config file");
                size_t size = configFile.size();
                // Allocate a buffer to store contents of the file.
                std::unique_ptr<char[]> buf(new char[size]);

                configFile.readBytes(buf.get(), size);
                DynamicJsonDocument jsonBuffer(1024);
                DeserializationError deserializeError = deserializeJson(jsonBuffer,buf.get());
                if (!deserializeError) {
                    debugPrintln("\nparsed json");
                    mqtt_server = String(jsonBuffer["mqtt_server"]);
                    mqtt_port = String(jsonBuffer["mqtt_port"]).toInt();
                    mqtt_user = String(jsonBuffer["mqtt_user"]);
                    mqtt_password = String(jsonBuffer["mqtt_password"]);
                    mqtt_basetopic = String(jsonBuffer["mqtt_basetopic"]);
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
    if(mqtt_basetopic == "") {
        const char* mqtt_basetopic = "VISCA";
        Serial.println(mqtt_basetopic);
    } 

    WiFiManagerParameter custom_mqtt_server("server", "mqtt server",mqtt_server.c_str(), 40);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", String(mqtt_port).c_str(), 6);
    WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user.c_str(), 40);
    WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password.c_str(), 40);
    WiFiManagerParameter custom_mqtt_basetopic("basetopic", "mqtt basetopic", mqtt_basetopic.c_str(), 128);

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
    wifiManager.addParameter(&custom_mqtt_basetopic);
    
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
    mqtt_server = custom_mqtt_server.getValue();
    mqtt_port = String(custom_mqtt_port.getValue()).toInt();

    // save the custom parameters to FS
    if (shouldSaveConfig) {
        debugPrintln("saving config");
        //DynamicJsonBuffer jsonBuffer;
        //JsonObject& json = jsonBuffer.createObject();
        StaticJsonDocument<256> jsonBuffer;
        //JsonObject& json = jsonBuffer.parseObject(buf.get());
        //DeserializationError jsonError = 
        //deserializeJson(jsonBuffer,buf.get());

        jsonBuffer["mqtt_server"] = custom_mqtt_server.getValue();
        jsonBuffer["mqtt_port"] = custom_mqtt_port.getValue();
        jsonBuffer["mqtt_user"] = custom_mqtt_user.getValue();
        jsonBuffer["mqtt_password"] = custom_mqtt_password.getValue();
        jsonBuffer["mqtt_basetopic"] = custom_mqtt_basetopic.getValue();

        File configFile = LittleFS.open("/config.json", "w");
        if (!configFile) {
            debugPrintln("failed to open config file for writing");
        }

        //json.printTo(Serial);
        //json.printTo(configFile);
        //serializeJsonPretty(jsonBuffer, Serial);
        serializeJson(jsonBuffer, configFile);
        configFile.close();
        // end save
    }
    debugPrintln("local ip");
    //uint16_t mqtt_port_x = 1883;
    client.setServer(mqtt_server.c_str(), mqtt_port);

    client.setCallback(callback);
}

String buildTopic(const char* subTopic) {
    if(mqtt_basetopic == ""){
        mqtt_basetopic = "VISCA";
    }
    String newTopic = String(mqtt_basetopic) + "/" + String(subTopic);
    return newTopic;
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

            client.subscribe(buildTopic("#").c_str());
            client.publish(buildTopic("system/status").c_str(), "ready");

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
    if (lastRequestTime + 1000 < millis()) {
        lastRequestTime = millis();
        requestEverything();
    }
}
void parseCommand(uint8_t* command, int length) {
    if (command[0] == 0x90 && command[1] == 0x50 &&
        command[2] == 0xFF) {
        return;
    }

    client.publish(buildTopic("return/camera/raw").c_str(), command, length);
    client.publish(buildTopic("return/camera/length").c_str(), String(length).c_str());

}
void handleSerial() {
    static enum { IDLE, RECEIVING } state = IDLE;
    static uint8_t buff[17] = {0};
    static int buffIndex = 0;
    while (visca.available() > 0) {
        uint8_t receivedByte = visca.read();
        Serial.println(receivedByte);
        client.publish("VISCA/return/camera/mqtt/", "Reading Data:");
        client.publish("VISCA/return/camera/mqtt/", String(receivedByte, HEX).c_str());
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
                    parseCommand(buff, buffIndex);
                    state = IDLE;
                }
                break;
        }

    }
}
void callback(char* topic, byte* payload, unsigned int length) {
    /*Preparation for sourcing out into commands.cpp*/
    //handleCommands(topic, payload, length);
    DynamicJsonDocument response(1024);
    deserializeJson(response,payload);
    JsonObject responseObject = response.as<JsonObject>();
    if (!responseObject.containsKey("cam")) {
        responseObject["cam"] = 0;
    }
    //uint8_t camNum = responseObject["cam"].as<uint8_t>();

    if (strcmp(topic, buildTopic("command/camera/raw").c_str()) == 0) {

        visca.write(payload, length);
        const uint focus = responseObject["focus"];
        byte focusValues[4];
        convertValues(focus, focusValues);
        for(int i =0; i < 4; i++){
            client.publish("VISCA/return/dev",String(focusValues[i]).c_str());            
        }

        client.publish(buildTopic("return/camera/status").c_str(),
                       ("Kotze Daten " + String(length)).c_str());
    }

    if (strcmp(topic, buildTopic("command/camera/blinkenlights").c_str()) == 0) {

        VISCACommand command =
            blinkenlights(responseObject["led"].as<uint8_t>(),
                          responseObject["mode"].as<uint8_t>(),
                          responseObject["cam"].as<uint8_t>());
        visca.write(command.payload, command.len);
    }

    if (strcmp(topic, buildTopic("command/camera/settings").c_str()) == 0) {

        if (responseObject.containsKey("backlight")) {
            VISCACommand command =
                backlight(responseObject["backlight"].as<bool>(),
                          responseObject["cam"].as<uint8_t>());
            visca.write(command.payload, command.len);
        }

        if (responseObject.containsKey("mirror")) {
            VISCACommand command = mirror(responseObject["mirror"].as<bool>(),
                                          responseObject["cam"].as<uint8_t>());
            visca.write(command.payload, command.len);
        }
        if (responseObject.containsKey("flip")) {
            VISCACommand command = flip(responseObject["flip"].as<bool>(),
                                        responseObject["cam"].as<uint8_t>());
            visca.write(command.payload, command.len);
        }

        if (responseObject.containsKey("mmdetect")) {
            VISCACommand command =
                mmdetect(responseObject["mmdetect"].as<bool>(),
                         responseObject["cam"].as<uint8_t>());
            visca.write(command.payload, command.len);
        }

        //  ir_output, ir_cameracontrol
    }


    if (strcmp(topic, buildTopic("command/camera/picture").c_str()) == 0) {

        if (responseObject.containsKey("wb")) {
            VISCACommand command = wb(responseObject["wb"].as<int>(),
                                      responseObject["cam"].as<uint8_t>());
            visca.write(command.payload, command.len);
        }
        if (responseObject.containsKey("iris")) {
            VISCACommand command = iris(responseObject["iris"].as<int>(),
                                        responseObject["cam"].as<uint8_t>());
            visca.write(command.payload, command.len);
        }
    }

    if (strcmp(topic, buildTopic("command/camera/moveto").c_str()) == 0) {
        Serial.println(responseObject.containsKey("x"));
        if (responseObject.containsKey("x")) {
            Serial.println(String(responseObject["cam"].as<uint8_t>()).c_str());
            Serial.println(String(responseObject["x"].as<int>()).c_str());
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

        visca.write(command.payload, command.len);
    }


    if (strcmp(topic, buildTopic("command/camera/moveby").c_str()) == 0) {

        if (!responseObject.containsKey("x")) {
            responseObject["x"] = 0;
        }
        if (!responseObject.containsKey("y")) {
            responseObject["y"] = 0;
        }

        VISCACommand command = relativeMovement(
            responseObject["x"].as<int>(), responseObject["y"].as<int>(),
            responseObject["cam"].as<uint8_t>());

        visca.write(command.payload, command.len);

        client.publish(buildTopic("command/camera/rawdata").c_str(), command.payload, command.len);

    }
    if (strcmp(topic, buildTopic("command/camera/clearBuffer").c_str()) == 0) {
        VISCACommand command = clearBuffer(responseObject["cam"].as<uint8_t>());

        visca.write(command.payload, command.len);
    }
    if (strcmp(topic, buildTopic("command/camera/setAddress").c_str()) == 0) {
        VISCACommand command = setAddress(responseObject["cam"].as<uint8_t>(), responseObject["address"].as<int>());

        visca.write(command.payload, command.len);
    }
    if (strcmp(topic, buildTopic("command/system/resetConfig").c_str()) == 0) {
        if (responseObject.containsKey("reset") && responseObject["reset"]) {
            client.publish(buildTopic("return/system").c_str(),"Device configuration deleted");
            ESP.eraseConfig();
            delay(2000);
            ESP.restart();
        }
        
    }
    if (strcmp(topic, buildTopic("command/system/updateConfig").c_str()) == 0) {

        File existingConfigFile = LittleFS.open("/config.json", "r");
        File newConfigFile = LittleFS.open("/config.json", "r+");
        //JSON-Object from storage
        size_t size = existingConfigFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        existingConfigFile.readBytes(buf.get(), size);
        StaticJsonDocument<256> existingBuffer;
        deserializeJson(existingBuffer,buf.get());
        JsonObject existingConfigObject = existingBuffer.as<JsonObject>();
        responseObject.remove("cam");
        existingConfigFile.close();
        if (!newConfigFile) {
            debugPrintln("failed to open config file for writing");
        } else {
            for(JsonPair currentObject : responseObject) {
                const char* key = currentObject.key().c_str();
                JsonVariant value = currentObject.value();
                if (existingConfigObject.containsKey(String(key))) {
                    existingConfigObject[key] = value;
                } else {
                    Serial.println("Nothing to change. Sad :(");
                }
                /*upcoming error handling
                else {
                    client.publish(buildTopic("return/system").c_str(),"No new settings written. Sad :(");
                }*/
            }
            String mqttResponse;
            //existingConfigObject.printTo(Serial);
            //existingBuffer.printTo(response);
            //existingBuffer.printTo(newConfigFile);
            serializeJsonPretty(existingBuffer, mqttResponse);
            serializeJson(existingBuffer,newConfigFile);
            client.publish(buildTopic("return/system").c_str(),("New MQTT-Settings: " + mqttResponse).c_str());
        }
        newConfigFile.close();
        delay(2000);
        ESP.restart();
    }
    if (strcmp(topic, buildTopic("command/system/getConfig").c_str()) == 0) {

        if (LittleFS.exists("/config.json")) {
            // file exists, reading and loading
            File configFile = LittleFS.open("/config.json", "r");
            if (configFile) {
                size_t size = configFile.size();
                std::unique_ptr<char[]> buf(new char[size]);
                configFile.readBytes(buf.get(), size);
                StaticJsonDocument<256> existingBuffer;
                deserializeJson(existingBuffer,buf.get());
                char mqttResponse[256];
                serializeJson(existingBuffer, mqttResponse);
                client.publish(buildTopic("return/system").c_str(),mqttResponse);
            }
        }
    }
    if (strcmp(topic, buildTopic("command/system/reboot").c_str()) == 0) {
        ESP.restart();
    }
    visca.flush();
}