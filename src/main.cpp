#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include "DHT20.h"
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define WIFI_SSID "ACLAB"
#define WIFI_PASSWORD "ACLAB2023"

#define THINGSBOARD_SERVER "app.coreiot.io"
#define THINGSBOARD_PORT 1883U

#define DHT20_TOKEN "knvzbj9qjf96dj9wwagm"
#define PIR_TOKEN "fyl76qhrztcreto2vej7"
#define IR_TOKEN "WWYxxKoA4nwXNKukq7l3"

#define LED_PIN 33
#define NUM_LEDS 4

#define PIR_PIN 32
#define IR_PIN 27

#define INTERVAL 5000UL
unsigned long lastCheckTime = 0;

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient);
DHT20 dht20;

bool wasWiFiConnected = false;
bool wasMQTTConnected = false;

void connectWiFi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    if (wasWiFiConnected)
    {
      Serial.println("⚠️ Mất kết nối WiFi. Đang thử lại...");
      wasWiFiConnected = false;
    }
    Serial.println("📡 Đang kết nối WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  else if (!wasWiFiConnected)
  {
    Serial.println("✅ WiFi đã kết nối!");
    wasWiFiConnected = true;
  }
}

void connectMQTT()
{
  if (!tb.connected())
  {
    if (wasMQTTConnected)
    {
      Serial.println("⚠️ Mất kết nối ThingsBoard. Đang thử lại...");
      wasMQTTConnected = false;
    }

    Serial.println("🔌 Đang kết nối ThingsBoard...");
    if (tb.connect(THINGSBOARD_SERVER, DHT20_TOKEN, THINGSBOARD_PORT))
    {
      Serial.println("✅ MQTT đã kết nối ThingsBoard!");
      wasMQTTConnected = true;
    }
    else
    {
      Serial.println("❌ Kết nối ThingsBoard thất bại!");
    }
  }
}

void updateDHT20()
{
  dht20.read();
  float temp = dht20.getTemperature();
  float hum = dht20.getHumidity();

  if (!isnan(temp) && !isnan(hum))
  {
    Serial.printf("🌡 Nhiệt độ: %.2f°C, 💧 Độ ẩm: %.2f%%\n", temp, hum);
    tb.sendTelemetryData("temperature", temp);
    tb.sendTelemetryData("humidity", hum);

    // LED1 đỏ nếu nhiệt độ > 30
    strip.setPixelColor(0, temp > 30.0 ? strip.Color(255, 0, 0) : strip.Color(0, 0, 0));
    strip.show();
  }
  else
  {
    Serial.println("❌ Lỗi đọc cảm biến DHT20!");
  }
}

void updatePIR()
{
  bool motion = digitalRead(PIR_PIN) == HIGH;
  tb.sendTelemetryData("pir_motion", motion);
  Serial.println(String("👀 PIR phát hiện: ") + (motion ? "Yes" : "No"));

  // LED2 xanh nếu có chuyển động
  strip.setPixelColor(1, motion ? strip.Color(0, 255, 0) : strip.Color(0, 0, 0));
  strip.show();
}

void updateIR()
{
  bool isDark = digitalRead(IR_PIN) == LOW;
  tb.sendTelemetryData("ir_dark", isDark);
  Serial.println(String("🌙 Trạng thái ánh sáng (IR): ") + (isDark ? "Tối" : "Sáng"));

  // LED3 vàng nếu tối
  strip.setPixelColor(2, isDark ? strip.Color(255, 255, 0) : strip.Color(0, 0, 0));
  strip.show();
}

void runApp()
{
  if (tb.connected())
    tb.loop();

  if (millis() - lastCheckTime > INTERVAL)
  {
    lastCheckTime = millis();

    connectWiFi();

    if (WiFi.status() == WL_CONNECTED)
    {
      connectMQTT();
      if (tb.connected())
      {
        updateDHT20();
        updatePIR();
        updateIR();
      }
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("🚀 Bắt đầu hệ thống!");

  Wire.begin();
  dht20.begin();
  pinMode(PIR_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  strip.begin();
  strip.show(); // Tắt tất cả LED

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop()
{
  runApp();
}