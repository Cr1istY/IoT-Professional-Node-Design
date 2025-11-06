#include <CenterHub.h>
#include <CenterMQTT.h>

/* ====== 用户只需改这里 ====== */
CenterMQTT::Config mqttCfg = {
  .ssid     = "cristPhone17",
  .pwd      = "wangyuting",
  .broker   = "172.20.10.5",
  .port     = 1883,
  .clientId = "esp32_center",
  .user     = "admin",
  .pass     = "public",
  .pubTopic = "center/events"
};
/* =========================== */

CenterMQTT mqtt(mqttCfg);
CenterHub  hub(mqtt, 8);   // 8 节点

void setup() {
  Serial.begin(115200);
  hub.begin();
}

void loop() {
  if (!mqtt.connected()) mqtt.connect();
  mqtt.loop();
  static uint32_t t0 = 0;
  if (millis() - t0 > 5000) {   // 每 5 s 一次
    t0 = millis();
    hub.spin();
  }
}