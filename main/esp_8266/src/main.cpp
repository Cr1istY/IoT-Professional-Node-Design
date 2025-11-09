#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

typedef struct {
  uint8_t type;
  uint8_t nodeId;
  char payload[48];
} __attribute__((packed)) Packet;

// **状态机**
enum NodeState {
  STATE_REGISTERING,  // 注册中
  STATE_HANDSHAKING,  // 握手确认
  STATE_CONNECTED     // 已连接
};
NodeState state = STATE_REGISTERING;

uint8_t centerMac[6] = {0};
uint8_t nodeId = 0;
uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// **超时与重试**
uint32_t lastSendTime = 0;
uint8_t retryCount = 0;
#define REGISTER_TIMEOUT  5000  // 5秒超时
#define MAX_RETRY         3     // 最多重试3次

void onDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(Packet)) return;

  Packet *pkt = (Packet*)data;

  // **收到注册确认**
  if (state == STATE_REGISTERING && pkt->type == 1) {
    memcpy(centerMac, mac, 6);
    nodeId = pkt->nodeId;
    state = STATE_HANDSHAKING;  // 进入握手确认状态

    Serial.printf("\n=== 收到注册确认 ===\n");
    Serial.printf("分配ID: %d\n", nodeId);
    Serial.printf("中心MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // **立即发送注册完成确认**
    Packet ok = {2, nodeId, "OK"};  // type=2=注册完成
    esp_now_add_peer(centerMac, ESP_NOW_ROLE_CONTROLLER, 1, NULL, 0);
    esp_now_send(centerMac, (uint8_t*)&ok, sizeof(ok));
    Serial.println("→ 发送握手完成\n");
  }

  // **收到中心数据**
  if (state == STATE_HANDSHAKING && pkt->type == 1 && pkt->nodeId == nodeId) {
    state = STATE_CONNECTED;  // **握手成功**
    Serial.println("✓ 握手完成，进入连接状态\n");
  }

  // **收到心跳回复**
  if (state == STATE_CONNECTED && pkt->type == 5) {  // type=5=心跳回复
    Serial.print("♥");  // 心跳符号
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP8266 节点启动 ===");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  wifi_set_channel(1);

  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_CONTROLLER, 1, NULL, 0);
    esp_now_register_recv_cb(onDataRecv);
    Serial.println("✓ ESP-NOW就绪");
  } else {
    Serial.println("✗ ESP-NOW失败");
    while(1);
  }
}

void loop() {
  uint32_t now = millis();

  switch (state) {
    case STATE_REGISTERING:  // **注册请求阶段**
      if (now - lastSendTime > REGISTER_TIMEOUT) {
        if (retryCount >= MAX_RETRY) {
          Serial.println("\n✗ 注册超时，重启...");
          ESP.restart();
        }

        Packet req = {0, 0, "REGISTER"};
        int result = esp_now_send(broadcastMac, (uint8_t*)&req, sizeof(req));
        Serial.printf("发送注册请求 [%d/3] 结果=%d\n", retryCount+1, result);
        retryCount++;
        lastSendTime = now;
      }
      break;

    case STATE_HANDSHAKING:  // **等待握手确认**
      if (now - lastSendTime > 2000) {
        Serial.println("等待握手确认...");
        lastSendTime = now;
        // 可重发握手完成包
        Packet ok = {2, nodeId, "OK"};
        esp_now_send(centerMac, (uint8_t*)&ok, sizeof(ok));
      }
      break;

    case STATE_CONNECTED:    // **已连接，正常工作**
      // **每10秒发送传感器数据**
      static uint32_t lastData = 0;
      if (now - lastData > 10000) {
        lastData = now;
        char sensor[48];
        snprintf(sensor, 48, "{\"temp\":%.1f,\"humi\":%.1f}",
                 random(220, 280)/10.0, random(450, 650)/10.0);
        Packet pkt = {3, nodeId, ""};  // type=3=数据
        strncpy(pkt.payload, sensor, 48);
        esp_now_send(centerMac, (uint8_t*)&pkt, sizeof(pkt));
        Serial.printf("数据: %s\n", sensor);
      }

      // **每30秒发送心跳**
      static uint32_t lastBeat = 0;
      if (now - lastBeat > 30000) {
        lastBeat = now;
        Packet pkt = {4, nodeId, "HEARTBEAT"};  // type=4=心跳
        esp_now_send(centerMac, (uint8_t*)&pkt, sizeof(pkt));
      }
      break;
  }
}