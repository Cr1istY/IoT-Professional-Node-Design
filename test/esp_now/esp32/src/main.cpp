#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"

#define MAX_NODES 10

typedef struct {
  uint8_t mac[6];
  uint8_t nodeId;
  bool isActive;
  uint32_t lastSeen;
} NodeInfo;

NodeInfo nodeList[MAX_NODES];
uint8_t nodeCount = 0;

// **数据包类型：0=注册请求, 1=注册确认, 2=数据, 3=心跳**
typedef struct {
  uint8_t type;
  uint8_t nodeId;
  char payload[16];
} __attribute__((packed)) Packet;

void sendRegisterAck(const uint8_t *mac, uint8_t assignedId) {
  Packet ack = {1, assignedId, "REGISTERED"};  // type=1表示注册确认

  // **关键：使用广播地址发送确认，确保节点能收到**
  uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_send(broadcastMac, (uint8_t*)&ack, sizeof(ack));
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(Packet)) return;

  Packet *pkt = (Packet*)data;

  if (pkt->type == 0) {  // 注册请求
    // 检查是否已存在
    for (int i = 0; i < nodeCount; i++) {
      if (memcmp(nodeList[i].mac, mac, 6) == 0) {
        nodeList[i].lastSeen = millis();
        sendRegisterAck(mac, nodeList[i].nodeId);  // 重新发送确认
        return;
      }
    }

    // 注册新节点
    if (nodeCount < MAX_NODES) {
      memcpy(nodeList[nodeCount].mac, mac, 6);
      nodeList[nodeCount].nodeId = nodeCount + 1;
      nodeList[nodeCount].isActive = true;
      nodeList[nodeCount].lastSeen = millis();

      // 添加peer
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, mac, 6);
      peerInfo.channel = 1;
      peerInfo.encrypt = false;
      esp_now_add_peer(&peerInfo);

      Serial.printf("节点[%d]注册成功\n", nodeList[nodeCount].nodeId);
      sendRegisterAck(mac, nodeList[nodeCount].nodeId);  // 广播确认
      nodeCount++;
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_start();
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) return;

  // **必须添加广播peer才能发送广播包**
  uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastMac, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(onDataRecv);
  Serial.println("中心节点就绪");
}

void loop() {
  // 每10秒向节点1发送数据
  static uint32_t last = 0;
  if (millis() - last > 10000 && nodeCount > 0) {
    last = millis();
    Packet pkt = {2, 1, "Hello"};
    esp_now_send(nodeList[0].mac, (uint8_t*)&pkt, sizeof(pkt));
  }
}