/*
 *  This sketch sends data via HTTP GET requests to data.sparkfun.com service.
 *
 *  You need to get streamId and privateKey at data.sparkfun.com and paste them
 *  below. Or just customize this script to talk to other HTTP servers.
 *
 */
#define ESP32
#define DEBUG_WEBSOCKETS
#include <SocketIOClient.h>

SocketIOClient client;
const char* ssid     = "xxxxxxxxxxxxxxxxx";
const char* password = "xxxxxxxxxxxxx";

char host[] = "192.168.178.26";
int port = 3484;

unsigned long previousMillis = 0;
long interval = 5000;
unsigned long lastreply = 0;
unsigned long lastsend = 0;

void setup() {
    Serial.begin(9600);
    Serial.println();
    delay(10);

    // We start by connecting to a WiFi network

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    client.on("rtime", [](const String &data, ackCallback_fn cb){
        Serial.print("Il est ");
        Serial.println(data);
        cb("confirmation for received!");
    });

    if (!client.connect(host, port)) {
        Serial.println("connection failed");
        return;
    }
    if (client.connected()) {
        client.send("Connected !!!!");
    }
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > interval) {
        previousMillis = currentMillis;
        client.emit("atime", "{\"message\":\"Time please?\"}");
    }
    client.monitor();
}

