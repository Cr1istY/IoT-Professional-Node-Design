//
// Created by crist yang on 2025/11/6.
//

#include "CenterESPNOW.h"
#include <esp_now.h>
#include <WiFi.h>

extern "C" {
#include <esp_now.h>
}

CenterESPNOW* CenterESPNOW::_inst = nullptr;

void CenterESPNOW::begin() {
    _inst = this;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(_rxCb);
        Serial.println("ESP-NOW OK");
    } else {
        Serial.println("ESP-NOW fail");
    }
}

void CenterESPNOW::pollNode(uint8_t id) {
    esp_now_peer_info_t peer = {};
    peer.channel = 0;
    peer.encrypt = false;
    uint8_t base[] = {0x3C, 0x61, 0x05, 0x0D, 0x8D, 0x00};
    base[5] = id;
    memcpy(peer.peer_addr, base, 6);

    if (esp_now_add_peer(&peer) == ESP_OK) {
        NodeFrame cmd{0xBB, id, {0}};
        esp_now_send(peer.peer_addr, (uint8_t*)&cmd, sizeof(cmd));
        delay(150);
        esp_now_del_peer(peer.peer_addr);
    }
}

void CenterESPNOW::_rxCb(const uint8_t* mac, const uint8_t* data, int len) {
    if (len == sizeof(NodeFrame) && _inst && _inst->_cb)
        _inst->_cb(*reinterpret_cast<const NodeFrame*>(data));
}