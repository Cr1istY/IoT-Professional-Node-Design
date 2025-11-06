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