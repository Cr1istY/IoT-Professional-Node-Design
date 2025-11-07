#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

// **全局变量声明**
typedef struct {
  uint8_t type;
  uint8_t nodeId;
  char payload[16];
} __attribute__((packed)) Packet;

uint8_t centerMac[6] = {0};            // 中心MAC（动态学习）
uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // 广播地址
uint8_t nodeId = 0;
bool registered = false;

// **接收回调**
void onDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(Packet)) return;

  Packet *pkt = (Packet*)data;

  // 收到注册确认
  if (pkt->type == 1 && pkt->nodeId != 0) {
    memcpy(centerMac, mac, 6);
    nodeId = pkt->nodeId;
    registered = true;

    // **动态添加中心为peer**
    esp_now_add_peer(centerMac, ESP_NOW_ROLE_CONTROLLER, 1, NULL, 0);
    Serial.printf("注册成功！节点ID=%d\n", nodeId);
    Serial.printf("中心MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  centerMac[0], centerMac[1], centerMac[2],
                  centerMac[3], centerMac[4], centerMac[5]);
  }

  // 收到数据
  if (pkt->type == 2) {
    Serial.printf("中心数据: %s\n", pkt->payload);
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  wifi_set_channel(1);

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW初始化失败");
    return;
  }

  // **初始只添加广播peer**
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_CONTROLLER, 1, NULL, 0);

  esp_now_register_recv_cb(onDataRecv);
  Serial.println("节点启动...");
}

void loop() {
  static uint32_t lastAction = 0;

  // **注册请求（未注册时）**
  if (!registered && millis() - lastAction > 3000) {
    lastAction = millis();
    Packet regPkt = {0, 0, "REGISTER"};  // type=0
    esp_now_send(broadcastMac, (uint8_t*)&regPkt, sizeof(regPkt));
    Serial.println("发送注册请求...");
  }

  // **心跳/数据（已注册后）**
  if (registered && millis() - lastAction > 10000) {
    lastAction = millis();

    // 发送心跳
    Packet pkt = {3, nodeId, "HEARTBEAT"};
    esp_now_send(centerMac, (uint8_t*)&pkt, sizeof(pkt));
    Serial.println("发送心跳");

    // 可选：发送传感器数据
    // Packet sensorPkt = {1, nodeId, "Temp:25C"};
    // esp_now_send(centerMac, (uint8_t*)&sensorPkt, sizeof(sensorPkt));
  }
}