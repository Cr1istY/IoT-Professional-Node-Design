#include <NodeESPNow.h>

// 1. 本机编号 + 中心 MAC（ESP32 的 STA MAC）
uint8_t center[] = {0x84, 0x1F, 0xE8, 0x8F, 0x49, 0x08};
NodeESPNow node(1, center);   // NODE_ID = 1

// 2. 可选：自定义数据填充
void fillData(Frame& f) {
  f.payload[0] = analogRead(A0) >> 2;
  f.payload[1] = 0;          // 可填其他传感器
}

void setup() {
  Serial.begin(115200);
  node.begin(fillData);      // 不传回调就用默认 A0
}

void loop() {
  // 纯中断驱动，主循环空跑
  delay(1000);
}