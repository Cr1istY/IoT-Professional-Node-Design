//
// Created by crist yang on 2025/11/6.
//

#include "CenterMQTT.h"

CenterMQTT::CenterMQTT(const Config& cfg)
    : _cfg(cfg), _mqtt(_client) {}

bool CenterMQTT::connect() {
    // WiFi
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        WiFi.begin(_cfg.ssid, _cfg.pwd);
        Serial.print("WiFi …");
        for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) {
            delay(500); Serial.print('.');
        }
        if (WiFi.status() != WL_CONNECTED) return false;
        Serial.println("\nWiFi OK");
        Serial.print("IP  "); Serial.println(WiFi.localIP());
    }

    // MQTT
    _mqtt.setServer(_cfg.broker, _cfg.port);
    if (_mqtt.connect(_cfg.clientId, _cfg.user, _cfg.pass)) {
        Serial.println("MQTT OK");
        return true;
    }
    Serial.printf("MQTT fail %d\n", _mqtt.state());
    return false;
}

bool CenterMQTT::connected() { return _mqtt.connected(); }
void CenterMQTT::loop() { _mqtt.loop(); }

bool CenterMQTT::publish(const char* payload) {
    return _mqtt.publish(_cfg.pubTopic, payload);
}