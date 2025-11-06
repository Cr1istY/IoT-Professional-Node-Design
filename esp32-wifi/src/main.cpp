#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* 1. 连接现场热点 */
const char* WIFI_SSID = "cristPhone17";
const char* WIFI_PWD  = "wangyuting";


const char* MQTT_BROKER = "172.20.10.5";
const uint16_t MQTT_PORT = 1883;
const char* CLIENT_ID    = "esp32_center";
const char* MQTT_USER    = "admin";
const char* MQTT_PWD     = "public";
const char* UPLOAD_TOPIC = "center/events";          // 发布主题

/* 3. ESP-NOW数据结构 */
typedef struct {
  uint8_t  id;          // 终端编号
  uint16_t seq;         // 包序号
  uint8_t  data[24];    // 传感器数据
} __attribute__((packed)) msg_t;

StaticJsonDocument<1024> doc;   // 打包用
WiFiClient   espClient;
PubSubClient mqtt(espClient);

/* 4. 动态轮询相关 */
uint8_t  next_id = 1;           // 当前要轮询的终端
const uint8_t MAX_NODE = 8;     // 最多8个终端
msg_t    rx_buf;                // 接收缓存
bool     rx_new  = false;       // 收到新包标志

/* 5. ESP-NOW回调 */
void on_recv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len == sizeof(msg_t)) {
    memcpy(&rx_buf, data, sizeof(rx_buf));
    rx_new = true;
  }
}

/* 6. 连Wi-Fi + MQTT */
// 在 connect_cloud() 函数中添加更多调试信息
void connect_cloud() {
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PWD);
    Serial.print("WiFi …");
    for (int i=0;i<20&&WiFi.status()!=WL_CONNECTED;i++){delay(500);Serial.print('.');}
    if (WiFi.status() != WL_CONNECTED) WiFi.disconnect();
  }
  Serial.println("\nWiFi OK");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  while (!mqtt.connected()) {
    Serial.printf("Attempting MQTT connection to %s:%d...\n", MQTT_BROKER, MQTT_PORT);
    if (mqtt.connect(CLIENT_ID, MQTT_USER, MQTT_PWD)) {
      Serial.println("MQTT OK");
    } else {
      Serial.printf("MQTT failed, state: %d\n", mqtt.state());
      Serial.print("MQTT retry…"); delay(5000);
    }
  }
}


/* 7. 上传函数 */
void upload_cloud() {
  doc.clear();
  JsonArray nodes = doc.createNestedArray("nodes");
  for (uint8_t id = 1; id <= MAX_NODE; id++) {
    /* 动态注册→发命令→收数据 */
    esp_now_peer_info_t peer = {};
    peer.channel = 0; peer.encrypt = false;
    memcpy(peer.peer_addr, "\x3C\x61\x05\x0D\x8D\x00", 6);
    peer.peer_addr[5] = id;               // 末字节=node_id
    if (esp_now_add_peer(&peer) == ESP_OK) {
      msg_t cmd = {0xBB, id, {0}};        // 命令帧
      esp_now_send(peer.peer_addr, (uint8_t*)&cmd, sizeof(cmd));
      delay(150);                         // 等回应
      if (rx_new) {
        JsonObject obj = nodes.createNestedObject();
        obj["id"]   = rx_buf.id;
        obj["seq"]  = rx_buf.seq;
        obj["val"]  = rx_buf.data[0];     // 举例：温度
        rx_new = false;
      }
      esp_now_del_peer(peer.peer_addr);
    }
  }
  doc["ts"] = millis();
  char json[256];
  serializeJson(doc, json);
  if (mqtt.connected()) mqtt.publish(UPLOAD_TOPIC, json);
  Serial.printf("Upload %s\n", json);
}

/* 8. 初始化 */
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_now_init();
  esp_now_register_recv_cb(on_recv);
  connect_cloud();
}

/* 9. 主循环 */
void loop() {
  if (!mqtt.connected()) connect_cloud();
  mqtt.loop();
  static uint32_t t0 = 0;
  if (millis() - t0 > 5000) {   // 每5s汇总上传
    t0 = millis();
    upload_cloud();
  }
}
