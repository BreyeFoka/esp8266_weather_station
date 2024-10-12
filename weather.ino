/* ESP8266 Weather Station v1.0 */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

const char* ssid = "REPLACE_WITH_SSID";
const char* password = "REPLACE_WITH_PASS";

void setup() {
  Serial.begin(115200);
  Serial.println("Booting NodeMCU...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
}

void loop() {
  // Simulated DHT22 Reading
  float h = 45.0 + (random(0,10) / 10.0);
  float t = 22.0 + (random(0,20) / 10.0);

  Serial.print("Humidity: "); Serial.print(h); Serial.print("%  ");
  Serial.print("Temp: "); Serial.print(t); Serial.println("°C");

  delay(30000); // Sleep 30s
}