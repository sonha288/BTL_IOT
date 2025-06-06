#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#define CD_PIN 27    
#define LED_PIN        32  // chân dữ liệu
#define NUMPIXELS  4   // số lượng LED

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  pixels.begin();
  Serial.begin(115200);  // Start Serial Monitor
  pinMode(CD_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  int motion = digitalRead(CD_PIN);

  if (motion == HIGH) { // If motion is detected
    Serial.println("🚶 Motion detected! Turning on LED.");
    pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // đỏ
    pixels.show();
  } else {
    pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Turn off LED
  }
  delay(100); // Small delay to avoid excessive looping
}
