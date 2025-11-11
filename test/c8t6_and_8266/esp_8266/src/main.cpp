#include <Arduino.h>
#define LED_BUILTIN 2

char rxBuffer[100];
bool newPacket = false;

void sendStringToSTM32(const char* str) {
    Serial.print('@');
    Serial.print(str);
    Serial.print("\r\n");
}

void handlePacket(char* msg) {
    // Serial.print("STM32回复: ");
    // Serial.println(msg);  // 仅调试时打印

    if (strcmp(msg, "LED_ON_OK") == 0) {
        digitalWrite(LED_BUILTIN, LOW);
    } else if (strcmp(msg, "LED_OFF_OK") == 0) {
        digitalWrite(LED_BUILTIN, HIGH);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
}

void loop() {
    static uint8_t state = 0;
    static uint8_t idx = 0;
    static unsigned long lastSend = 0;

    // // 每2秒切换一次LED
    if (millis() - lastSend > 2000) {
        lastSend = millis();
        static bool ledOn = false;
        ledOn = !ledOn;
        sendStringToSTM32(ledOn ? "LED_ON" : "LED_OFF");
    }

    // ✅ 纯接收，无输出污染
    while (Serial.available()) {
        char c = Serial.read();
        if (state == 0 && c == '@') {
            idx = 0;
            state = 1;
        } else if (state == 1) {
            if (c == '\r') {
                state = 2;
            } else if (idx < 99) {
                rxBuffer[idx++] = c;
            }
        } else if (state == 2 && c == '\n') {
            rxBuffer[idx] = '\0';
            newPacket = true;
            state = 0;
        } else {
            state = 0;
        }
    }

    if (newPacket) {
        handlePacket(rxBuffer);
        newPacket = false;
    }
}