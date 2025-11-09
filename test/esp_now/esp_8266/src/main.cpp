#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "MyConfig.h"

bool registered = false;
uint32_t lastAction = 0;
uint8_t nodeId = 0;
uint8_t center_mac[6] = {0};
uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void onDataRecv(u8 * mac_addr, u8 * data, u8 len) {
    if (len != sizeof(Packet)) return;
    Packet * pkt = (Packet *) data;

    if (pkt->type == 1) {
        // 收到注册确认
        memcpy(center_mac, mac_addr, 6);
        nodeId = pkt->nodeId;
        registered = true;
        Serial.printf("Registered As Node[%d]\n", nodeId);
        esp_now_add_peer(center_mac, ESP_NOW_ROLE_CONTROLLER, 1, NULL, 0);
        Serial.printf("Center MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", center_mac[0], center_mac[1], center_mac[2],
                      center_mac[3], center_mac[4], center_mac[5]);
    } else if (pkt->type == 2) {
        if (memcmp(mac_addr, center_mac, 6) != 0) return;
        Serial.printf("中心数据：%s\n", pkt->message);
    }
}

void setup() {
    uint8_t retry_times = 0;
    do {
        Serial.begin(115200);
        // 开启 WiFi 并设置通道
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(50); // 延时，确保wifi关闭
        wifi_set_channel(1);
        Serial.printf("WiFi init [%d]\n", retry_times);
        retry_times++;
    }while (esp_now_init() != 0);

    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_CONTROLLER, 1, NULL, 0);
    memcpy(center_mac, broadcastMac, 6); // 首先，广播发送注册
    esp_now_register_recv_cb(onDataRecv);
    Serial.println("ESP8266 Already!");

}

void loop() {
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    // 失活检测，5s内无响应则认为结点失活
    if (!registered && millis() - lastAction > 1000) {
        // Serial.println("Send!");
        registered = false;
        lastAction = millis();
        Packet regPkt = {0, nodeId, mac, "register"};
        esp_now_send(broadcastMac, (uint8_t *) &regPkt, sizeof(regPkt));
        Serial.println("Node unActive, Registering...");
    }
    Packet pkt = {2, nodeId, mac, "hello"};
    esp_now_send(broadcastMac, (uint8_t *) &pkt, sizeof(pkt));
    delay(5000);

}
