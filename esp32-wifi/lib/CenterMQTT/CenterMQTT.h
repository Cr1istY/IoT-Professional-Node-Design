//
// Created by crist yang on 2025/11/6.
//

#pragma once
#include <WiFi.h>
#include <PubSubClient.h>

class CenterMQTT {
public:
    struct Config {
        const char* ssid;
        const char* pwd;
        const char* broker;
        uint16_t    port;
        const char* clientId;
        const char* user;
        const char* pass;
        const char* pubTopic;
    };

    explicit CenterMQTT(const Config& cfg);
    bool   connect();
    bool   connected();
    void   loop();
    bool   publish(const char* payload);   // 直接发字符串
private:
    Config _cfg;
    WiFiClient   _client;
    PubSubClient _mqtt;
};

