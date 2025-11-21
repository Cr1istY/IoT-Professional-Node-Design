#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <cstdio>
#include "MyConfig.h"


bool registered = false;
uint32_t lastAction = 0;
uint8_t nodeId = 0;
uint8_t center_mac[6] = {0};
uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t wifiChannel = 0;

void onDataRecv(u8 * mac_addr, u8 * data, u8 len) {
    if (len != sizeof(Packet)) return;
    Packet * pkt = (Packet *) data;
    // Serial.printf("Received packet - Type: %d, NodeId: %d, Message: %s\n", pkt->type, pkt->nodeId, pkt->message);
    if (pkt->type == 1) {
        // 收到注册确认
        memcpy(center_mac, mac_addr, 6);
        nodeId = pkt->nodeId;
        registered = true;
        Serial.printf("Registered As Node[%d]\n", nodeId);
        esp_now_add_peer(center_mac, ESP_NOW_ROLE_CONTROLLER, wifiChannel, NULL, 0);
        Serial.printf("Center MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", center_mac[0], center_mac[1], center_mac[2],
                      center_mac[3], center_mac[4], center_mac[5]);
    } else if (pkt->type == 2) {
        if (nodeId != pkt->nodeId) return;
        registered = true;
    } else if (pkt->type == 3) {
        // 温度
        Serial.printf("@3\r\n");
    } else if (pkt->type == 4) {
        // 湿度
        Serial.printf("@4\r\n");
    } else if (pkt->type == 5) {
        // 红外
        Serial.printf("@5\r\n");
    } else if (pkt->type == 6) {
        // 气体浓度
        Serial.printf("@6\r\n");
    } else {
        // 不做处理
        delay(20);
    }
}

void setup() {
    uint8_t retry_times = 0;

    do {
        Serial.begin(115200);
        // 开启 WiFi 并设置通道
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        Serial.println("连接Wifi");
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        wifiChannel = WiFi.channel();
        Serial.printf("WiFiChannel [%d]", wifiChannel);
        delay(50); // 延时，确保wifi关闭
        Serial.printf("WiFi init by [%d]\n", retry_times);
        retry_times++;
    }while (esp_now_init() != 0);
    // 先连接wifi，获取信道
    // 再断开wifi连接
    WiFi.disconnect();
    delay(1000);
    // 首先，将广播地址加入peer list
    wifi_set_channel(wifiChannel);
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_CONTROLLER, wifiChannel, NULL, 0);
    memcpy(center_mac, broadcastMac, 6);
    esp_now_register_recv_cb(onDataRecv);
    Serial.println("ESP8266 Already!");
    // 第一次注册
    memcpy(center_mac, broadcastMac, 6);
    lastAction = millis();
    Packet regPkt = {0, nodeId, ""};
    esp_now_send(broadcastMac, (uint8_t *) &regPkt, sizeof(regPkt));
    delay(500);
}

void loop() {
    if (Serial.available()) {
        String data = Serial.readStringUntil('\n');
        // Serial.println(data);
        // 进行边缘计算，然后发送
        // stm32格式为 type,message
        uint8_t comma = data.indexOf(',');
        uint8_t messageType = data.substring(0, comma).toInt();
        String rawValue = data.substring(comma + 1);
        rawValue.trim();
        Packet pkt;
        pkt.type = messageType;
        pkt.nodeId = nodeId;
        rawValue.toCharArray(pkt.message, sizeof(pkt.message));
        esp_now_send(center_mac, (uint8_t *) &pkt, sizeof(pkt));
    }
    if (!registered) {
        memcpy(center_mac, broadcastMac, 6);
        lastAction = millis();
        Packet regPkt = {0, nodeId, ""};
        esp_now_send(broadcastMac, (uint8_t *) &regPkt, sizeof(regPkt));
        delay(500);
    }
    // 失活检测，60s内无响应则认为结点失活
    registered = lastAction + 60000 > millis();
    // delay(50);

}

// esp8266只做消息的转发：
// 充当esp32和stm32沟通的桥梁
// 1. 先发送注册包，等待注册确认
// 2. 注册完成后，进入循环，等待数据
// 3. 接收到数据，通过串口发送给stm32结点
// 4. 从stm32结点返回消息给esp32
