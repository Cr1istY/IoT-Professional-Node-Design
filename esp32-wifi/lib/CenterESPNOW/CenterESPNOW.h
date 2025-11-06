//
// Created by crist yang on 2025/11/6.
//

#pragma once
#include <WiFi.h>
#include <functional>

struct NodeFrame {
    uint8_t  id;
    uint16_t seq;
    uint8_t  data[24];
} __attribute__((packed));

using OnNodeData = std::function<void(const NodeFrame&)>;

class CenterESPNOW {
public:
    CenterESPNOW() = default;
    void begin();
    void pollNode(uint8_t id);
    void onData(OnNodeData cb) { _cb = cb; }

private:
    static void _rxCb(const uint8_t* mac, const uint8_t* data, int len);
    OnNodeData _cb;
    static CenterESPNOW* _inst;
};