#include <Arduino.h>

#define LED_PIN 19
#define BTN_PIN 35

unsigned long lastCheckTime = 0;
unsigned long ledOnTime = 0;
int holdSeconds = 0;
bool ledOn = false;
bool alreadyActivated = false;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println("ESP32 starter!");
}

void loop() {
  unsigned long currentTime = millis();

  if (!alreadyActivated) {
    if (currentTime - lastCheckTime >= 1000) {
      lastCheckTime = currentTime;

      int buttonState = digitalRead(BTN_PIN);

      if (buttonState == HIGH) {
        holdSeconds++;
        Serial.print("Button held for ");
        Serial.print(holdSeconds);
        Serial.println(" second(s)");

        if (holdSeconds >= 10 && !ledOn) {
          digitalWrite(LED_PIN, HIGH);
          ledOn = true;
          alreadyActivated = true;
          ledOnTime = currentTime;
          Serial.println("LED turned ON after 10 seconds hold");
        }
      } else {
        if (holdSeconds > 0) {
          Serial.println("Button released — timer reset");
        }
        holdSeconds = 0;
      }
    }
  }
  if (ledOn && (millis() - ledOnTime >= 5000)) {
    digitalWrite(LED_PIN, LOW);
    ledOn = false;
    Serial.println("LED turned OFF after 5 seconds");
  }
}
