### esp32 代码一

```c++
#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* 1. 连接现场热点 */
const char* WIFI_SSID = "cristPhone17";
const char* WIFI_PWD  = "wangyuting";


const char* MQTT_BROKER = "10.20.119.55"; // 百度云示例
const uint16_t MQTT_PORT = 1883;
const char* CLIENT_ID    = "esp32_center";
const char* MQTT_USER    = "";
const char* MQTT_PWD     = "";
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
void connect_cloud() {
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PWD);
    Serial.print("WiFi …");
    for (int i=0;i<20&&WiFi.status()!=WL_CONNECTED;i++){delay(500);Serial.print('.');}
    if (WiFi.status() != WL_CONNECTED) WiFi.disconnect();
  }
  Serial.println("\nWiFi OK");

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  while (!mqtt.connected()) {
    if (mqtt.connect(CLIENT_ID, MQTT_USER, MQTT_PWD)) {
      Serial.println("MQTT OK");
    } else {
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

```

查看本机mac地址程序：

```c++
#include <WiFi.h>

void setup() {
  Serial.begin(115200);

  // 获取并打印STA模式下的MAC地址
  String macAddress = WiFi.macAddress();
  Serial.println("STA MAC Address: " + macAddress);

  // 或者获取AP模式下的MAC地址
  String softAPMacAddress = WiFi.softAPmacAddress();
  Serial.println("SoftAP MAC Address: " + softAPMacAddress);
}

void loop() {
  // 主循环内容
}

```

我的esp32的mac地址:  84:1F:E8:8F:49:08

### 常见问题

#### 常见问题一

使用wsl运行docker部署emqx需要进行一下配置：

1. 确保计算机和esp32处于同一个网络；

2. 使用管理员运行命令敞口输入 `wsl -l -v` 查看已启动的虚拟机；

3. 键入`wsl -d Ubuntu-22.04 ip addr show eth0 | Select-String 'inet '` 查看虚拟机网关地址；

4. 抄录输出的第一个ip地址；

5. 进行端口代理:

   1. ```cmd
      netsh interface portproxy add v4tov4 ^
        listenaddress=0.0.0.0 listenport=1883 ^
        connectaddress={'你的地址'} connectport=1883
      ```

6. 防火墙放行:

   1. ```
      New-NetFirewallRule -DisplayName "EMQX-MQTT" `
        -Direction Inbound -Protocol TCP -LocalPort 1883,8883 `
        -Action Allow
      ```

7. 进行连接尝试；