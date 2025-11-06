//
// Created by crist yang on 2025/11/6.
//

#include "NodeESPNow.h"

NodeESPNow* NodeESPNow::_inst = nullptr;

NodeESPNow::NodeESPNow(uint8_t id, const uint8_t centerMac[6], int ledPin)
    : _id(id), _led(ledPin) {
    memcpy(_centerMac, centerMac, 6);
    _inst = this;
}

void NodeESPNow::begin(DataFillCallback cb) {
    _userFill = cb;
    pinMode(_led, OUTPUT);
    digitalWrite(_led, HIGH);   // 8266 低电平亮

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    if (esp_now_init() != 0) {
        Serial.println("ESP-NOW init failed");
        ESP.restart();
    }
    esp_now_register_recv_cb(_rxCb);
    esp_now_add_peer(_centerMac, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

    _tx.header = 0xAA;
    _tx.node_id = _id;
    _tx.seq = 0;
    memset(_tx.payload, 0, sizeof(_tx.payload));

    Serial.printf("ESP8266 Node%d ESP-NOW ready\n", _id);
}

void ICACHE_RAM_ATTR NodeESPNow::_rxCb(uint8_t *mac, uint8_t *data, uint8_t len) {
    if (len != sizeof(Frame) || !_inst) return;
    memcpy(&_inst->_rx, data, sizeof(Frame));
    _inst->handleCmd(_inst->_rx);
}

void NodeESPNow::handleCmd(const Frame& f) {
    if (f.header == 0xBB && f.node_id == _id) {
        // 用户填充数据
        if (_userFill) _userFill(_tx);
        else {
            // 默认：A0 采样
            _tx.payload[0] = analogRead(A0) >> 2;
            for (int i = 1; i < 24; ++i) _tx.payload[i] = 0;
        }
        _tx.seq++;
        esp_now_send(_centerMac, (uint8_t*)&_tx, sizeof(_tx));

        // 指示灯
        digitalWrite(_led, LOW);
        delay(50);
        digitalWrite(_led, HIGH);
    }
}