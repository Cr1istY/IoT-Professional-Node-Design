//
// Created by crist yang on 2025/11/7.
//

#include "PackSent.h"
#include <esp_now.h>

// 发送确认帧
void sendRegisterAck(const uint8_t *mac, uint8_t assignedId) {
    // 构建一个确认帧
    Packet ack = {
        1,
        assignedId,
        ""
    };
    esp_now_send(broadcastMac, (uint8_t*) &ack, sizeof(ack));
}

// 发送其他消息帧
void sendMessage(const uint8_t *mac, uint8_t type, uint8_t nodeId) {
    Packet messagePacket = {
        type,
        nodeId,
        ""
    };

    esp_now_send(mac, (uint8_t*) &messagePacket, sizeof(messagePacket));
}




