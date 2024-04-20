#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>

Adafruit_SHT31 sht31 = Adafruit_SHT31();

void setup() {
    Serial.begin(115200);
    Serial.println('oi');
    if (!sht31.begin(0x44)) {   // Set to the default I2C address for SHT31
        Serial.println("Couldn't find SHT31");
        while (1) delay(1);
    }
    Serial.println("SHT31 Found!");
}

void loop() {
    float temp = sht31.readTemperature();
    float humidity = sht31.readHumidity();

    if (!isnan(temp) && !isnan(humidity)) { // Check if readings are valid
        Serial.print("Temperature: ");
        Serial.print(temp);
        Serial.println(" °C");
        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");
    } else {
        Serial.println("Failed to read from sensor!");
    }

    delay(2000); // Wait for 2 seconds between readings
}