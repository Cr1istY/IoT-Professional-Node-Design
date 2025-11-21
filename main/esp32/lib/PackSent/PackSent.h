//
// Created by crist yang on 2025/11/7.
//

#ifndef ESP32_PACKSENT_H
#define ESP32_PACKSENT_H

#include "../../src/MyConfig.h"

void sendRegisterAck(const uint8_t *mac, uint8_t assignedId);
void sendMessage(const uint8_t *mac, uint8_t type, uint8_t nodeId);

#endif //ESP32_PACKSENT_H