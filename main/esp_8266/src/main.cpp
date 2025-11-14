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
// uint8_t my_mac[6] = {0};


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
        Serial.printf("中心数据：%s\n", pkt->message);
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
    wifi_set_channel(wifiChannel);
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_CONTROLLER, wifiChannel, NULL, 0);
    memcpy(center_mac, broadcastMac, 6); // 首先，广播发送注册
    esp_now_register_recv_cb(onDataRecv);
    Serial.println("ESP8266 Already!");
    delay(10000);
}

void loop() {
    // 失活检测，60s内无响应则认为结点失活
    registered = lastAction + 60000 > millis();
    if (!registered) {
        // Serial.println("Send!");
        memcpy(center_mac, broadcastMac, 6);
        lastAction = millis();
        Packet regPkt = {0, nodeId, ""};
        esp_now_send(broadcastMac, (uint8_t *) &regPkt, sizeof(regPkt));
        Serial.println("Node Unactive, Registering...");
        delay(500);
        return;
    }
    // Packet pkt = {2, nodeId, "hello"};
    // esp_now_send(broadcastMac, (uint8_t *) &pkt, sizeof(pkt));
    // delay(5000);
    // 进行串口通信
    // 接收
    if (Serial.available() && registered) {
        String data = Serial.readStringUntil('\n');
        // 进行边缘处理，然后发送
        uint8_t comma = data.indexOf(',');
        uint8_t messageType = data.substring(0, comma).toInt();
        String rawValue = data.substring(comma + 1);
        rawValue.trim();
        Packet pkt;
        pkt.type = messageType;
        pkt.nodeId = nodeId;
        rawValue.toCharArray(pkt.message, sizeof(pkt.message));
        esp_now_send(center_mac, (uint8_t *) &pkt, sizeof(pkt));
        delay(50);
        Serial.println("已发送");
    }
    delay(5000);
}
