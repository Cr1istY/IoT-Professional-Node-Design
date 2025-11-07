#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include "esp_wifi.h"

// ==================== 全局变量定义 ====================
uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // **必须定义**

// 配置
#define WIFI_SSID       "cristPhone17"
#define WIFI_PASS       "wangyuting"
#define EMQX_SERVER     "172.20.10.5"  // 你的EMQX服务器IP
#define MQTT_CLIENT_ID  "esp32-center-001"
#define MAX_NODES       10

// 数据结构
typedef struct {
  uint8_t type;        // 0=注册请求 1=注册确认 2=注册完成 3=数据
  uint8_t nodeId;
  char payload[48];
} __attribute__((packed)) Packet;

typedef struct {
  uint8_t mac[6];
  uint8_t nodeId;
  bool isActive;
  uint32_t lastSeen;
} NodeInfo;

// 全局对象
WiFiClient espClient;
PubSubClient mqttClient(espClient);
NodeInfo nodeList[MAX_NODES];
uint8_t nodeCount = 0;

// MQTT回调
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  Serial.print("EMQX命令: ");
  Serial.println(topic);
}

// MQTT连接
void connectMQTT() {
  if (mqttClient.connected()) return;
  String clientId = MQTT_CLIENT_ID + String(random(0xffff), HEX);
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("✓ MQTT连接成功");
    mqttClient.subscribe("cmd/esp32/#");
  }
}

// 发布到EMQX
void publishToEMQX(uint8_t nodeId, const char* type, const char* value) {
  char jsonMsg[128];
  snprintf(jsonMsg, 128, "{\"nodeId\":%d, \"type\":\"%s\", \"value\":%s}",
           nodeId, type, value);
  mqttClient.publish("data/esp32/nodes", jsonMsg);
}

// ESP-NOW接收回调
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(Packet)) return;

  Packet pkt;
  memcpy(&pkt, data, sizeof(Packet));

  NodeInfo* node = nullptr;
  for (int i = 0; i < nodeCount; i++) {
    if (memcmp(nodeList[i].mac, mac, 6) == 0) {
      node = &nodeList[i];
      break;
    }
  }

  // 新节点注册
  if (!node && pkt.type == 0) {
    if (nodeCount >= MAX_NODES) return;

    node = &nodeList[nodeCount];
    memcpy(node->mac, mac, 6);
    node->nodeId = nodeCount + 1;
    node->isActive = true;
    node->lastSeen = millis();

    // 添加peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    Serial.printf("\n[注册] 节点[%d] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 node->nodeId, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // 发送注册确认
    Packet ack = {1, node->nodeId, "ACK"};
    esp_now_send(broadcastMac, (uint8_t*)&ack, sizeof(ack));
    Serial.printf("→ 发送注册确认给节点[%d]\n", node->nodeId);

    nodeCount++;
  }

  if (!node) return;

  // 握手完成确认
  if (pkt.type == 2 && pkt.nodeId == node->nodeId) {
    Serial.printf("✓ 节点[%d] 握手完成\n", node->nodeId);
  }

  // 更新活性
  node->isActive = true;
  node->lastSeen = millis();

  // 处理数据
  if (pkt.type == 3) {
    publishToEMQX(node->nodeId, "sensor", pkt.payload);
    Serial.printf("节点[%d] 数据: %s\n", node->nodeId, pkt.payload);
  }
}

// 初始化
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 中心节点启动 ===");

  // ESP-NOW初始化
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_start();
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onDataRecv);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    Serial.println("✓ ESP-NOW + 广播peer 初始化成功");
  } else {
    Serial.println("✗ ESP-NOW初始化失败");
    while(1);
  }

  // WiFi连接
  Serial.print("连接WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi: " + WiFi.localIP().toString());

  // MQTT初始化
  mqttClient.setServer(EMQX_SERVER, 1883);
  mqttClient.setCallback(mqttCallback);
}

// 主循环
void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  } else {
    mqttClient.loop();
  }

  // 清理离线节点
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    for (int i = 0; i < nodeCount; i++) {
      if (millis() - nodeList[i].lastSeen > 30000) {
        nodeList[i].isActive = false;
        char offline[16];
        snprintf(offline, 16, "节点[%d]离线", nodeList[i].nodeId);
        publishToEMQX(nodeList[i].nodeId, "status", "\"offline\"");
        Serial.println(offline);
      }
    }
  }

  delay(10);
}