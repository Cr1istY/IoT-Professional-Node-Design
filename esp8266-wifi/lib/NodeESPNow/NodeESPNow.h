//
// Created by crist yang on 2025/11/6.
//

#pragma once
#include <ESP8266WiFi.h>
#include <functional>

extern "C" {
#include <espnow.h>
}

struct __attribute__((packed)) Frame {
    uint8_t  header;     // 0xAA 数据，0xBB 命令
    uint8_t  node_id;
    uint16_t seq;
    uint8_t  payload[24];
};

using DataFillCallback = std::function<void(Frame& f)>;  // 用户填充数据

class NodeESPNow {
public:
    // 参数：本机编号、中心 MAC、LED 引脚（默认板载 LED）
    NodeESPNow(uint8_t id, const uint8_t centerMac[6], int ledPin = LED_BUILTIN);
    void begin(DataFillCallback cb = nullptr);   // 启动 ESP-NOW
    uint16_t getSeq() const { return _tx.seq; }

private:
    static void ICACHE_RAM_ATTR _rxCb(uint8_t *mac, uint8_t *data, uint8_t len);
    static NodeESPNow* _inst;
    void handleCmd(const Frame& f);

    uint8_t _id;
    int _led;
    uint8_t _centerMac[6];
    Frame _tx, _rx;
    DataFillCallback _userFill;
};