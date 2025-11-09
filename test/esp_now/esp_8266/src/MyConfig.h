//
// Created by crist yang on 2025/11/7.
//

#ifndef ESP32_CONFIG_H
#define ESP32_CONFIG_H

#include <Arduino.h>


#define WIFI_SSID       "cristPhone17"
#define WIFI_PASS       "wangyuting"
#define EMQX_SERVER     "172.20.10.5"  // **电脑IP，不是localhost**
#define EMQX_PORT       1883

// 2. MQTT认证（EMQX初始无认证可留空）
#define MQTT_USER       "admin"
#define MQTT_PASS       "cqupt520"

// 3. EMQX客户端ID（必须唯一）
#define MQTT_CLIENT_ID  "esp32_center"

#define MAX_NODE_SIZE 10



// 结点结构体

// typedef struct Node {
//     uint8_t mac[6]; // mac地址
//     uint8_t nodeId; // 结点编号
//     bool isActive; // 是否活跃
//     uint32_t lastSeen; // 最后响应时间
// } NodeInfo;

// NodeInfo nodeList[MAX_NODE_SIZE]; // 注册结点表

// uint8_t nodeCount = 0; // 结点数量

// 消息结构 esp8266 和 esp32 统一使用
typedef struct {
    // 数据包类型：0 注册请求，1 注册确认，2 数据包
    uint8_t type;
    uint8_t nodeId;
    uint8_t *mac; // mac地址确认，防止编号相同
    char message[64];
} __attribute__((packed)) Packet;






#endif //ESP32_CONFIG_H