//
// Created by crist yang on 2025/11/6.
//

#pragma once
#include <ArduinoJson.h>
#include "CenterESPNOW.h"
#include "CenterMQTT.h"

class CenterHub {
public:
    CenterHub(CenterMQTT& mqtt, uint8_t maxNode = 8);
    void begin();
    void spin();                 // 轮询 + 上报

private:
    void handleFrame(const NodeFrame& f);
    CenterMQTT& _mqtt;
    CenterESPNOW _esp;           // << 1. 关键：实体对象
    const uint8_t _maxNode;
    JsonDocument _doc;
    uint8_t _pollId = 1;
};