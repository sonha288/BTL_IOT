#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi & MQTT config
const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqttServer = "app.coreiot.io";
const int mqttPort = 1883;
const char *mqttUser = "nUwypBbRbqsn2crp6u6P";
const char *mqttPassword = "";

#define LED1_PIN 13
bool ledState = false;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void callback(char *topic, byte *payload, unsigned int length)
{
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error)
  {
    Serial.println("[MQTT] JSON parse failed");
    return;
  }

  String method = doc["method"];
  String topicStr = String(topic);
  String requestId = topicStr.substring(strlen("v1/devices/me/rpc/request/"));
  String responseTopic = "v1/devices/me/rpc/response/" + requestId;

  if (method == "setLEDValue")
  {
    bool newState = doc["params"].is<bool>() ? doc["params"].as<bool>() : doc["params"]["value"].as<bool>();

    ledState = newState;
    digitalWrite(LED1_PIN, ledState ? HIGH : LOW);
    Serial.println("[RPC] LED: " + String(ledState ? "ON" : "OFF"));

    String payload = "{\"value\":" + String(ledState ? 1 : 0) + "}";
    mqttClient.publish(responseTopic.c_str(), payload.c_str());
  }
}

void connectToWiFi()
{
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WiFi] Connected. IP: " + WiFi.localIP().toString());
}

void reconnectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.print("[MQTT] Connecting...");
    if (mqttClient.connect("ESP32Client", mqttUser, mqttPassword))
    {
      Serial.println("Connected!");
      mqttClient.subscribe("v1/devices/me/rpc/request/+");
    }
    else
    {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5s...");
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
  }
}

void mqttTask(void *param)
{
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);

  for (;;)
  {
    if (!mqttClient.connected())
    {
      reconnectMQTT();
    }
    mqttClient.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED1_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);

  connectToWiFi();

  xTaskCreatePinnedToCore(
      mqttTask,
      "MQTTTask",
      4096,
      NULL,
      1,
      NULL,
      1);
}

void loop()
{
}
