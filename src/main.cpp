#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include "DHT20.h"
#include "Wire.h"
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

// ========== PIN DEFINITION ==========
#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22
#define TRIG_PIN 19
#define ECHO_PIN 23
#define SERVO_PIN 26
#define FAN 2
#define FAN_PWM_CHANNEL 4

#define PIR_PIN 27
#define IR_PIN 32
#define LED_PIN 33
#define NUM_LEDS 4

// ========== NETWORK CONFIG ==========
constexpr char WIFI_SSID[] = "hasonnn";
constexpr char WIFI_PASSWORD[] = "28082004";

// === TOKENs ===
constexpr char TOKEN_SENSOR[] = "knvzbj9qjf96dj9wwagm";
constexpr char TOKEN_FAN[] = "GLVo2PmoGVVVXlOJmtA7";
constexpr char TOKEN_PIR[] = "fyl76qhrztcreto2vej7";
constexpr char TOKEN_LED4[] = "zUvDq5FLsYePSqKWqurB";

constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883;

constexpr uint32_t MAX_MESSAGE_SIZE = 1024;
constexpr uint16_t telemetrySendInterval = 2000;

WiFiClient wifiClientSensor, wifiClientFan, wifiClientPIR, wifiClientLED4;
Arduino_MQTT_Client mqttClientSensor(wifiClientSensor);
Arduino_MQTT_Client mqttClientFan(wifiClientFan);
;
Arduino_MQTT_Client mqttClientPIR(wifiClientPIR);
Arduino_MQTT_Client mqttClientLED4(wifiClientLED4);
ThingsBoard tbLED4(mqttClientLED4, MAX_MESSAGE_SIZE);
ThingsBoard tbSensor(mqttClientSensor, MAX_MESSAGE_SIZE);
ThingsBoard tbFan(mqttClientFan, MAX_MESSAGE_SIZE);
ThingsBoard tbPIR(mqttClientPIR, MAX_MESSAGE_SIZE);

// ========== OBJECTS ==========
DHT20 dht20;
Servo myServo;
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ========== GLOBAL VARIABLES ==========
int fanSpeed = 0;
bool attributesSent = false;
unsigned long lastSendTime = 0;
unsigned long doorOpenTime = 0;
bool isDoorOpen = false;
float distance = 0;
unsigned long duration;
bool led4State = false;        // false = tắt, true = bật
bool led3State = false;        // false = tắt, true = bật
bool manualMode = false;       // false = tự động, true = thủ công
bool controlDoorState = false; // false = đóng, true = mở
bool sensorRPCSubscribed = false;
bool fanRPCSubscribed = false;
bool led4RPCSubscribed = false;

// ========== RPC: FAN ==========
void processFanSpeedChange(JsonVariantConst const &request, JsonDocument &response)
{
  if (!request["speed"])
  {
    Serial.println("⚠️ Request không có trường 'speed'");
    return;
  }

  int speed = request["speed"].as<int>();
  if (speed < 0 || speed > 100)
  {
    Serial.println("⚠️ Giá trị speed ngoài phạm vi 0-100");
    return;
  }

  fanSpeed = speed;
  int pwmValue = map(fanSpeed, 0, 100, 0, 255);
  ledcWrite(FAN_PWM_CHANNEL, pwmValue);
  Serial.printf("✅ Fan speed updated: %d%% (PWM: %d)\n", fanSpeed, pwmValue);

  response["status"] = "ok";
  response["fanSpeed"] = fanSpeed;
}

RPC_Callback fanSpeedCallback("fanSpeed", processFanSpeedChange);

// void processManualMode(JsonVariantConst request, JsonDocument &response)
// {
//   Serial.println("📥 Nhận RPC setState (Manual Mode)");

//   if (!request.containsKey("setState"))
//   {
//     Serial.println("⚠️ RPC thiếu trường 'setState'");
//     response["status"] = "error";
//     response["message"] = "Missing 'setState'";
//     return;
//   }

//   manualMode = request["setState"].as<bool>();
//   response["status"] = "ok";
//   response["manualMode"] = manualMode;

//   Serial.print("⚙️ Manual Mode: ");
//   Serial.println(manualMode ? "BẬT" : "TẮT");

//   // Nếu bật chế độ manual: tắt đèn và đóng cửa ngay
//   if (manualMode)
//   {
//     // Tắt LED
//     strip.setPixelColor(0, strip.Color(0, 0, 0));
//     strip.setPixelColor(1, strip.Color(0, 0, 0));
//     strip.setPixelColor(2, strip.Color(0, 0, 0));
//     strip.setPixelColor(3, strip.Color(0, 0, 0));
//     strip.show();

//     // Đóng cửa nếu đang mở
//     myServo.write(0);
//     isDoorOpen = false;
//     Serial.println("🚪 Cửa đã đóng (do Manual Mode)");
//   }

//   // Gửi trạng thái về ThingsBoard
//   tbSensor.sendTelemetryData("manualMode", manualMode);
// }
// RPC_Callback manualModeCallback("setState", processManualMode);

// ========== RPC: LED4 ==========
void processLED4Control(JsonVariantConst request, JsonDocument &response)
{
  Serial.println("📥 Nhận RPC setLED4");

  if (!request.is<JsonObjectConst>())
  {
    Serial.println("⚠️ RPC LED4 cần là object với trường 'setLED4'");
    response["status"] = "error";
    response["message"] = "Expected object with 'setLED4'.";
    return;
  }

  if (!request.containsKey("setLED4"))
  {
    Serial.println("⚠️ RPC LED4 thiếu trường 'setLED4'");
    response["status"] = "error";
    response["message"] = "Missing field 'setLED4'.";
    return;
  }

  led4State = request["setLED4"].as<bool>();

  if (led4State)
  {
    strip.setPixelColor(3, strip.Color(255, 255, 255)); // Bật LED 4
    strip.show();

    Serial.println("💡 LED4: BẬT");
  }
  else
  {
    strip.setPixelColor(3, strip.Color(0, 0, 0)); // Tắt LED 4
    strip.show();

    Serial.println("💡 LED4: TẮT");
  }

  strip.show();

  response["status"] = "ok";
  response["setLED4"] = led4State;

  // Gửi trạng thái LED4 về ThingsBoard như telemetry hoặc attribute (tùy bạn)
  tbLED4.sendTelemetryData("led4State", led4State ? "ON" : "OFF");
}

RPC_Callback led4ControlCallback("setLED4", processLED4Control);

void processLED3Control(JsonVariantConst request, JsonDocument &response)
{
  Serial.println("📥 Nhận RPC setLED3");

  if (!request.is<JsonObjectConst>())
  {
    Serial.println("⚠️ RPC LED3 cần là object với trường 'setLED3'");
    response["status"] = "error";
    response["message"] = "Expected object with 'setLED3'.";
    return;
  }

  if (!request.containsKey("setLED3"))
  {
    Serial.println("⚠️ RPC LED3 thiếu trường 'setLED3'");
    response["status"] = "error";
    response["message"] = "Missing field 'setLED3'.";
    return;
  }

  led3State = request["setLED3"].as<bool>();


    if (!led3State)
    {      strip.setPixelColor(2, strip.Color(0, 0, 0)); // Tắt LED 3
      Serial.println("💡 LED3: TẮT");
       strip.setPixelColor(0, strip.Color(0, 0, 0));
    strip.setPixelColor(1, strip.Color(0, 0, 0));
    strip.setPixelColor(2, strip.Color(0, 0, 0));
          tbSensor.sendTelemetryData("ir_dark", "Tối");

    }
    else
    {
 strip.setPixelColor(2, strip.Color(255, 255, 255)); 
     strip.setPixelColor(0, strip.Color(0, 0, 0));
    strip.setPixelColor(1, strip.Color(0, 0, 0));
    strip.setPixelColor(2, strip.Color(0, 0, 0));
      Serial.println("💡 LED3: BẬT");

    }

    strip.show();

    response["status"] = "ok";
    response["setLED3"] = led3State;

    // ✅ Gửi trạng thái LED3 về ThingsBoard thông qua tbIR
    tbSensor.sendTelemetryData("led3State", led3State ? "ON" : "OFF");
  }


RPC_Callback led3ControlCallback("setLED3", processLED3Control);

void processControlDoor(JsonVariantConst request, JsonDocument &response)
{
  Serial.println("📥 Nhận RPC controlDoor");

  // Kiểm tra kiểu dữ liệu RPC
  if (!request.is<JsonObjectConst>())
  {
    Serial.println("⚠️ RPC controlDoor cần là object với trường 'controlDoor'");
    response["status"] = "error";
    response["message"] = "Expected object with field 'controlDoor'.";
    return;
  }

  // Kiểm tra có chứa trường 'controlDoor' không
  if (!request.containsKey("controlDoor"))
  {
    Serial.println("⚠️ RPC controlDoor thiếu trường 'controlDoor'");
    response["status"] = "error";
    response["message"] = "Missing field 'controlDoor'.";
    return;
  }

  // Lấy trạng thái điều khiển cửa từ RPC
  controlDoorState = request["controlDoor"].as<bool>();

  
    if (!controlDoorState)
    { 
      myServo.write(0); // Đóng cửa 
      isDoorOpen = false;
      Serial.println("🚪 Cửa: ĐÃ ĐÓNG (từ Dashboard - manual)");
                tbPIR.sendTelemetryData("pir_motion", "Đóng");
      
    }
    else
    {
     myServo.write(120); // Mở cửa
      isDoorOpen = true;
      doorOpenTime = millis();
      Serial.println("🚪 Cửa: ĐANG MỞ (từ Dashboard - manual)");
          tbPIR.sendTelemetryData("pir_motion", "Mở");


    }
    strip.show();

    response["status"] = "ok";
    response["controlDoor"] = controlDoorState;

    // ✅ Gửi trạng thái cửa về ThingsBoard
    tbSensor.sendTelemetryData("controlDoorState", controlDoorState ? "Mở" : "Đóng");
  }


RPC_Callback controlDoorCallback("controlDoor", processControlDoor);

// ========== INIT ==========
void InitWiFi()
{
  Serial.println("🔌 Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");
}

// ========== MQTT CONNECTION ==========
void ConnectLED4MQTT()
{
  if (!tbLED4.connected())
  {
    Serial.println("🔌 Kết nối MQTT LED4...");
    if (tbLED4.connect(THINGSBOARD_SERVER, TOKEN_LED4, THINGSBOARD_PORT))
    {
      Serial.println("✅ MQTT LED4 kết nối");

      if (!led4RPCSubscribed)
      {
        tbLED4.RPC_Subscribe(led4ControlCallback);
        led4RPCSubscribed = true;
      }
    }
    else
    {
      Serial.println("❌ Kết nối LED4 thất bại");
    }
  }
}

void ConnectSensorMQTT()
{
  if (!tbSensor.connected())
  {
    Serial.println("🔌 Kết nối MQTT SENSOR...");
    if (tbSensor.connect(THINGSBOARD_SERVER, TOKEN_SENSOR, THINGSBOARD_PORT))
    {
      Serial.println("✅ MQTT SENSOR kết nối");

      if (!sensorRPCSubscribed)
      {      

        tbSensor.RPC_Subscribe(controlDoorCallback);
      

        tbSensor.RPC_Subscribe(led3ControlCallback);

        sensorRPCSubscribed = true;
      }

      if (!attributesSent)
      {
        tbSensor.sendAttributeData("macAddress", WiFi.macAddress().c_str());
        tbSensor.sendAttributeData("ssid", WiFi.SSID().c_str());
        tbSensor.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
        attributesSent = true;
      }
    }
    else
    {
      Serial.println("❌ Kết nối SENSOR thất bại");
    }
  }
}

void ConnectFanMQTT()
{
  if (!tbFan.connected())
  {
    Serial.println("🔌 Kết nối MQTT FAN...");
    if (tbFan.connect(THINGSBOARD_SERVER, TOKEN_FAN, THINGSBOARD_PORT))
    {
      Serial.println("✅ MQTT FAN kết nối");

      if (!fanRPCSubscribed)
      {
        tbFan.RPC_Subscribe(fanSpeedCallback);
        fanRPCSubscribed = true;
      }
    }
    else
    {
      Serial.println("❌ Kết nối FAN thất bại");
    }
  }
}

void ConnectPIRMQTT()
{
  if (!tbPIR.connected())
  {
    Serial.println("🔌 Kết nối MQTT PIR...");
    if (tbPIR.connect(THINGSBOARD_SERVER, TOKEN_PIR, THINGSBOARD_PORT))
    {
      Serial.println("✅ MQTT PIR kết nối");
    }
    else
    {
      Serial.println("❌ Kết nối PIR thất bại");
    }
  }
}

// ========== DOOR CONTROL ==========
void handleDoor()
{
  // Nếu cửa đang đóng và có điều kiện mở (vd: distance gần)
  if (!isDoorOpen && distance > 0 && distance < 15)
  {
    myServo.write(120);
    isDoorOpen = true;
    doorOpenTime = millis();
    Serial.println("🚪 Mở cửa");
    tbPIR.sendTelemetryData("pir_motion", "Mở");
  }

  // Nếu cửa đang mở và đã đủ thời gian để đóng
  if (isDoorOpen && (millis() - doorOpenTime >= 5000))
  {
    myServo.write(0);
    isDoorOpen = false;
    Serial.println("🚪 Đóng cửa");
    tbPIR.sendTelemetryData("pir_motion", "Đóng");
  }
}

// ========== SETUP ==========
void setup()
{
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  pinMode(PIR_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  strip.begin();
  strip.show();

  Wire.begin(SDA_PIN, SCL_PIN);
  dht20.begin();

  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 544, 2400);
  myServo.write(0);

  ledcSetup(FAN_PWM_CHANNEL, 5000, 8);
  ledcAttachPin(FAN, FAN_PWM_CHANNEL);
  tbLED4.sendTelemetryData("led4State", led4State ? "ON" : "OFF");
  tbSensor.sendTelemetryData("manualMode", manualMode);

  InitWiFi();
}

// ========== LOOP ==========
void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    InitWiFi();
    delay(5000);
    return;
  }

  ConnectSensorMQTT();
  ConnectFanMQTT();
  ConnectPIRMQTT();
  ConnectLED4MQTT();

  if (millis() - lastSendTime >= telemetrySendInterval)
  {
    if (tbSensor.connected())
    {
      dht20.read();
      float temperature = dht20.getTemperature();
      float humidity = dht20.getHumidity();

      if (!isnan(temperature) && !isnan(humidity))
      {
        // Gửi nhiệt độ và độ ẩm về ThingsBoard qua TOKEN_SENSOR
        tbSensor.sendTelemetryData("temperature", temperature);
        tbSensor.sendTelemetryData("humidity", humidity);

        Serial.printf("🌡️ Temp: %.2f°C | 💧 Hum: %.2f%%\n", temperature, humidity);

        // LED báo nhiệt độ
        strip.setPixelColor(0, temperature > 30.0 ? strip.Color(255, 0, 0) : strip.Color(0, 0, 0));
      }
    }
    else
    {
      Serial.println("⚠️ MQTT SENSOR chưa kết nối, bỏ qua gửi nhiệt độ/độ ẩm");
    }

    // Siêu âm
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    duration = pulseIn(ECHO_PIN, HIGH, 30000);
    distance = duration * 0.034 / 2;
    tbSensor.sendTelemetryData("distance", distance);
    Serial.printf("📏 Distance: %.2f cm\n", distance);

    
    
      // ==== TỰ ĐỘNG MỞ CỬA ====
      if (distance > 0 && distance < 15)
      {
        Serial.println("🚶 Có người - mở cửa");
        handleDoor();
      }
if (led3State){

      // ==== PIR ====
      bool motion = digitalRead(PIR_PIN) == HIGH;
      Serial.println(String("👀 PIR phát hiện: ") + (motion ? "Yes" : "No"));
      strip.setPixelColor(1, motion ? strip.Color(0, 255, 0) : strip.Color(0, 0, 0));
      tbPIR.sendTelemetryData("active", motion);
      // ==== IR ====
      bool isDark = digitalRead(IR_PIN) == LOW;
      strip.setPixelColor(2, isDark ? strip.Color(255, 255, 0) : strip.Color(0, 0, 0));
      tbSensor.sendTelemetryData("ir_dark", isDark ? "Sáng" : "Tối");
    }

    tbSensor.sendTelemetryData("rssi", WiFi.RSSI());
    tbSensor.sendTelemetryData("fanSpeed", fanSpeed);

    strip.show();
    lastSendTime = millis();
  }

  handleDoor();

  tbSensor.loop();
  tbFan.loop();
  tbPIR.loop();
  tbLED4.loop();
}