//
// Created by crist yang on 2025/11/6.
//

#include "CenterHub.h"

CenterHub::CenterHub(CenterMQTT& mqtt, uint8_t maxNode)
    : _mqtt(mqtt), _maxNode(maxNode) {}

void CenterHub::begin() {
    _esp.onData([this](const NodeFrame& f) { handleFrame(f); });
    _esp.begin();                // << 2. 通过对象调用
}

void CenterHub::spin() {
    _doc.clear();
    JsonArray nodes = _doc["nodes"].to<JsonArray>();

    for (_pollId = 1; _pollId <= _maxNode; ++_pollId) {
        _esp.pollNode(_pollId);  // << 3. 通过对象调用
    }

    _doc["ts"] = millis();
    char json[256];
    serializeJson(_doc, json);
    if (_mqtt.connected()) _mqtt.publish(json);
    Serial.printf("Upload %s\n", json);
}

void CenterHub::handleFrame(const NodeFrame& f) {
    JsonArray nodes = _doc["nodes"];          // 重新取同一数组
    JsonObject o = nodes.add<JsonObject>();
    o["id"]  = f.id;
    o["seq"] = f.seq;
    o["val"] = f.data[0];
}