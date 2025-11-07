### DAY01

#### 设计方案：

使用多个esp8266采用星形拓扑的方式连接esp32。

#### 实际实现：

##### esp8266

代码：

```c++
#include <ESP8266WiFi.h>
extern "C" {
#include <espnow.h>
}

// 1. 每个终端唯一编号，1~255，烧录前改
#define NODE_ID  1

// 2. 中心 MAC（ESP32 的 STA MAC，请改成你自己的）
uint8_t center_mac[] = {0x84, 0x1F, 0xE8, 0x8F, 0x49, 0x08};

// 3. 帧格式，与 ESP32 中心保持一致
typedef struct __attribute__((packed)) {
  uint8_t  header;     // 0xAA 数据，0xBB 命令
  uint8_t  node_id;
  uint16_t seq;
  uint8_t  payload[24];
} frame_t;

frame_t tx_buf;        // 本机发送缓存
frame_t rx_buf;        // 接收缓存

// 4. 收到中心命令后回传数据
void ICACHE_RAM_ATTR on_recv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (len != sizeof(frame_t)) return;
  memcpy(&rx_buf, data, sizeof(rx_buf));
  // 只处理发给自己的命令帧
  if (rx_buf.header == 0xBB && rx_buf.node_id == NODE_ID) {
    // 构造回传数据帧
    tx_buf.header = 0xAA;
    tx_buf.node_id = NODE_ID;
    tx_buf.seq++;
    // 伪传感器：A0 采样值 0~255
    tx_buf.payload[0] = analogRead(A0) >> 2;   // 0~255
    // 其余字节可填其他数据
    for (int i = 1; i < 24; i++) tx_buf.payload[i] = 0;
    // 发送
    esp_now_send(center_mac, (uint8_t *)&tx_buf, sizeof(tx_buf));
    // 指示灯闪一下
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);   // 8266 低电平亮灯

  // 关闭 Wi-Fi 模式，再设为 Station 模式
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // 初始化 ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    ESP.restart();
  }

  // 注册回调
  esp_now_register_recv_cb(on_recv);

  // 添加唯一 peer（中心）
  esp_now_add_peer(center_mac, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

  // 发送缓存初始值
  tx_buf.header = 0xAA;
  tx_buf.node_id = NODE_ID;
  tx_buf.seq = 0;
  memset(tx_buf.payload, 0, sizeof(tx_buf.payload));

  Serial.printf("ESP8266 Node%d ESP-NOW ready\n", NODE_ID);
}

void loop() {
  // 纯中断驱动，主循环空跑
  delay(1000);
}
```

##### esp32代码：

```c++
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
```

##### 值得注意：

1. maccenter地址要为你自己esp32芯片的地址。

2. 本代码采用的是模拟数据，并且只记录。当esp32向本esp8266申请信息时，esp8266才发送。

3. 实际上，两者没有wifi连接，而是采用esp**裸 2.4 GHz 直连**：**“两边把射频调到同一信道 → 互相登记 MAC → 数据打包成 802.11 Action Frame 直接发 → 硬件过滤非目标包 → 中断回调交付用户”**，**全程不关联热点、不跑 TCP/IP**，这就是 ESP-NOW 的“裸 2.4 GHz 单播”通信过程。

   1. **上电初始化**

      - 双方只把射频设为 **Station 模式**（让 PHY 时钟工作）
      - **不调用 `WiFi.begin()`** → 不做 802.11 认证/关联，因此不会拿到 IP，也不连热点
      - 接着 `esp_now_init()` 启动 ESP-NOW 协议栈

   2. **建立“静态邻居表”**

      - ESP8266 端执行

        cpp

        复制

        ```cpp
        esp_now_add_peer(ESP32_MAC, ...)
        ```

        相当于告诉无线驱动：
        “以后只帮我把报文发给这个 MAC，别人发来的也帮我直接扔掉”

      - ESP32 中心同样要把每个 8266 的 MAC 写进自己的 peer 列表（代码里动态 `add/del` 即可）

   3. **信道对齐**

      - 默认跟随本地 Wi-Fi 信道（开机后调用 `wifi_set_channel(6)` 可锁死）
      - 双方必须在**同一 2.4 GHz 信道**才能互相收听到

   4. **帧封装与发送**

      - 用户数据被拷贝到 **esp_now_send()**
      - ESP-NOW 在驱动层把数据装进 **802.11 Action Frame**（类型 0xD0），**不建立 BSS**
      - 帧头里填目标 MAC，**速率 1 Mbps/54 Mbps 可选**，功率按射频配置表发射

   5. **空中接收与过滤**

      - 射频芯片收到任何 2.4 GHz 包 → 硬件先比对 **MAC 地址/信道/CRC**
      - 只有 peer 列表里存在的 MAC 才放进接收 FIFO，否则**硬件级丢弃**
      - 正确帧触发 **RX 中断** → 回调函数 `on_recv()` 被立即调用（8266 在中断上下文，ESP32 在任务上下文）

   6. **双向回传**

      - 8266 在回调里把传感器数据再 `esp_now_send()` 回去 → 同样走 Action Frame
      - 中心收到后完成一次**单播轮询**

   7. **时序与可靠性**

      - 单帧最大 250 Byte，**默认 1 Mbps 速率**下空中时间约 2 ms
      - 驱动自带 **自动重传 0~7 次**（menuconfig 或 `esp_now_set_peer_rate()` 可调）
      - 若仍失败返回 `ESP_NOW_TX_FAIL`，应用层可再补发

##### 可改进

````markdown
两者之间的通信是怎么进行的:
办法一句话：**让 ESP8266 只登记“目标 ESP32 的 MAC 地址”作为唯一 peer**，空中任何其它板子发的包都会被硬件过滤掉，根本进不到代码层。

下面给出 3 个常用手段，**保证 8266 只跟“正确的那一台” ESP32 通信**：

--------------------------------------------------
1. 硬编码 MAC（最简单）
- 在 ESP32 中心里打印自己的 STA MAC：
```cpp
Serial.print("Center MAC: ");
Serial.println(WiFi.macAddress());
```
- 把打印出的 6 字节填到 ESP8266 侧：
```cpp
uint8_t center_mac[] = {0x24, 0x6F, 0x28, 0x12, 0x34, 0x56}; // 例
esp_now_add_peer(center_mac, ...);
```
8266 只注册这一个 peer，**硬件白名单**完成；其它板子的包被 PHY 层直接丢弃。

--------------------------------------------------
2. 软件过滤双保险（推荐）
即使 MAC 复制错误，再在**应用层**加一道口令：
```cpp
typedef struct {
  uint8_t  header;
  uint8_t  node_id;
  uint16_t seq;
  uint8_t  key;        // 1 字节口令，0~255
  uint8_t  payload[23];
} frame_t;
```
中心与所有终端约定同一个 `key = 0xA5`。  
8266 收到帧后：
```cpp
if (rx_buf.key != 0xA5) return; // 口令不对直接丢弃
```
**MAC + key** 双匹配才认为是“正确的主机”。

--------------------------------------------------
3. 绑定固定信道（防串网）
ESP-NOW 默认跟随**本地 Wi-Fi 信道**；如果附近有人把路由器调到 6 而你的板子还在 1，就会收不到。  
显式锁信道，两端放同一段代码：
```cpp
wifi_set_channel(6);   // SDK 函数，信道 6
```
确保**中心+终端**永远在同一 2.4 GHz 信道，**物理层隔离**完成。

--------------------------------------------------
4. 调试技巧
- 中心侧打开 **混杂模式** 抓包：
```cpp
wifi_promiscuous_enable(1);
```
可看到周围所有 ESP-NOW 帧，确认**目标 MAC 与口令**都正确再关闭。  
- 8266 打印 **rssi**：
```cpp
int8_t rssi = rx_pkt->rx_ctrl.rssi;
Serial.printf("From center rssi=%d\n", rssi);
```
只有 **> -50 dBm** 且 MAC 正确才算“连到身边这台”。

--------------------------------------------------
一句话总结  
**“把目标 ESP32 的 MAC 写死到 8266 的 peer 列表 → 同一信道 → 可选口令/key 双校验”**，硬件+软件两层锁，8266 就只会跟“正确的那一台” ESP32 通信，**其它任何板子发的包都被自动丢弃**。
````

#### 后续任务：

##### 现在已经完成：

esp32充当主机，与esp8266进行通信，同时，又通过mqtt协议上传到主机emqx服务器；

##### 待完成

1. esp8266与stm32进行串口通信，收集真正的数据然后发送；
2. esp32将信息上传到mqtt后进行处理；
3. mqtt将数据发送到移动应用上；
4. 支持移动应用操控系统各个设备；

### DAY02 

总结经验，对方案进行进一步设计。

#### 总流程

stm32 - esp8266 - esp32 - emqx

#### esp8266 与 esp32 通信流程

1. 使用 ESPNOW 进行直连。
2. 动态更新mac地址。
3. 三次握手机制。
4. mqtt上传。