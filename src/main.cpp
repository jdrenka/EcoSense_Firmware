#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SHT31.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>

// WiFi credentials
const char* ssid = "TELUS0850";
const char* password = "i8mz2ge68c";

// API endpoint
const char* serverName = "http://192.168.1.83:3000/dataBay";  // Replace with your server's IP address

// NTP Server
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -8 * 3600, 60000); // PST offset

// Create an SHT31 object
Adafruit_SHT31 sht31 = Adafruit_SHT31();

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");

    // Initialize NTP Client
    timeClient.begin();
    timeClient.forceUpdate();  // Make sure we get the time at least once

    // Initialize the SHT31 sensor
    if (!sht31.begin(0x44)) { // Set the SHT31 address to 0x44 or 0x45 depending on your wiring
        Serial.println("Couldn't find SHT31");
        while (1) delay(10);
    }
}

void loop() {
    timeClient.update();

    // Reading temperature and humidity from the SHT31 sensor
    float temperature = sht31.readTemperature();
    float humidity = sht31.readHumidity();
    int sensorID = 1;  // Replace with your actual sensor ID

    // Get the formatted date/time
    time_t now = timeClient.getEpochTime();
    struct tm *timeinfo = localtime(&now);
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    String timestamp = String(buffer);

    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverName);  // Specify request destination
        http.addHeader("Content-Type", "application/json");  // Specify content-type header

        // Prepare JSON data using ArduinoJson
        DynamicJsonDocument doc(1024);
        doc["time"] = timestamp;
        doc["temp"] = temperature;
        doc["hum"] = humidity;
        doc["sid"] = sensorID;

        String httpRequestData;
        serializeJson(doc, httpRequestData);

        int httpResponseCode = http.POST(httpRequestData);  // Send the request

        if (httpResponseCode > 0) {
            String response = http.getString();  // Get the request response payload
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
            Serial.println("Response: " + response);
        } else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }

        http.end();  // Close connection
    } else {
        Serial.println("WiFi Disconnected");
    }

    delay(10000);  // Send data every 10 seconds
}
