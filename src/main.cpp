#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SHT31.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <BH1750.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#include <nvs_flash.h>  // Include NVS library

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -7 * 3600, 60000);  // Adjust for UTC-7 (e.g., Pacific Daylight Time)

Adafruit_SHT31 sht31 = Adafruit_SHT31();
BH1750 lightMeter(0x23);

const char* serverName = "http://192.168.1.83:3000/dataBay";  // Use HTTP for now

WiFiManager wifiManager;

void httpPostTask(void *parameter);

void setup() {
    Serial.begin(115200);

    // Initialize NVS
    Serial.println("Initializing NVS...");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println("NVS needs to be erased, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        Serial.printf("NVS init failed: %s\n", esp_err_to_name(err));
        ESP.restart();
    }

    Serial.println("NVS initialized successfully");

    // WiFiManager to handle WiFi credentials and connection
    wifiManager.setConfigPortalTimeout(180);  // Set timeout for captive portal
    if (!wifiManager.autoConnect("ESP32_AP")) {
        Serial.println("Failed to connect to WiFi and hit timeout");
        ESP.restart();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
    }

    Serial.println("After wifi");

    // Initialize NTP Client
    timeClient.begin();
    timeClient.forceUpdate();

    if (!sht31.begin(0x44)) {
        Serial.println("Couldn't find SHT31 sensor! Restarting...");
        esp_restart();  // Reset the device
    } else {
        Serial.println("SHT31 sensor initialized.");
    }

    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23)) {
        Serial.println("Could not find a valid BH1750 sensor, check wiring! Restarting...");
        esp_restart();  // Reset the device
    } else {
        Serial.println("BH1750 sensor initialized.");
    }

    // Enable watchdog timer
    esp_task_wdt_init(5, true); // 5 second timeout, panic reset enabled
    esp_task_wdt_add(NULL); // Add current thread to WDT watch

    // Create task for HTTP POST
    xTaskCreate(httpPostTask, "HTTP Post Task", 8192, NULL, 1, NULL);
}

void loop() {
    esp_task_wdt_reset(); // Reset watchdog timer

    if (WiFi.status() != WL_CONNECTED) {
        wifiManager.autoConnect("ESP32_AP");
    }

    timeClient.update();

    delay(1000);  // Adjust the delay as needed
}

void httpPostTask(void *parameter) {
    while (true) {
        esp_task_wdt_reset(); // Reset watchdog timer

        float temperature = sht31.readTemperature();
        float humidity = sht31.readHumidity();
        float lightLevel = lightMeter.readLightLevel();
        int sensorID = 2;  // Replace with your actual sensor ID

        // Log sensor readings
        Serial.print("Temperature: ");
        Serial.println(temperature);
        Serial.print("Humidity: ");
        Serial.println(humidity);
        Serial.print("Light Level: ");
        Serial.println(lightLevel);

        time_t now = timeClient.getEpochTime();
        struct tm *timeinfo = localtime(&now);
        char buffer[30];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        String timestamp = String(buffer);

        // Log timestamp
        Serial.print("Timestamp: ");
        Serial.println(timestamp);

        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(serverName);
            http.addHeader("Content-Type", "application/json");

            DynamicJsonDocument doc(1024);
            doc["time"] = timestamp;
            doc["temp"] = temperature;
            doc["hum"] = humidity;
            doc["light"] = lightLevel;
            doc["sid"] = sensorID;

            String httpRequestData;
            serializeJson(doc, httpRequestData);

            Serial.print("HTTP Request Data: ");
            Serial.println(httpRequestData);

            int httpResponseCode = http.POST(httpRequestData);

            if (httpResponseCode > 0) {
                String response = http.getString();
                Serial.print("HTTP Response code: ");
                Serial.println(httpResponseCode);
                Serial.println("Response: " + response);
            } else {
                Serial.print("Error on sending POST: ");
                Serial.println(httpResponseCode);
                // Additional logging for debugging
                switch (httpResponseCode) {
                    case HTTPC_ERROR_CONNECTION_REFUSED:
                        Serial.println("Connection refused.");
                        break;
                    case HTTPC_ERROR_SEND_HEADER_FAILED:
                        Serial.println("Send header failed.");
                        break;
                    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
                        Serial.println("Send payload failed.");
                        break;
                    case HTTPC_ERROR_NOT_CONNECTED:
                        Serial.println("Not connected.");
                        break;
                    case HTTPC_ERROR_CONNECTION_LOST:
                        Serial.println("Connection lost.");
                        break;
                    case HTTPC_ERROR_NO_STREAM:
                        Serial.println("No stream.");
                        break;
                    case HTTPC_ERROR_NO_HTTP_SERVER:
                        Serial.println("No HTTP server.");
                        break;
                    case HTTPC_ERROR_TOO_LESS_RAM:
                        Serial.println("Too less RAM.");
                        break;
                    case HTTPC_ERROR_ENCODING:
                        Serial.println("Encoding error.");
                        break;
                    case HTTPC_ERROR_STREAM_WRITE:
                        Serial.println("Stream write error.");
                        break;
                    case HTTPC_ERROR_READ_TIMEOUT:
                        Serial.println("Read timeout.");
                        break;
                    default:
                        Serial.println("Unknown error.");
                        break;
                }
            }

            http.end();
        } else {
            Serial.println("WiFi Disconnected, buffering data");
            // Implement data buffering logic here
        }

        delay(10000);  // Send data every 10 seconds
    }
}
