//
// Created by crist yang on 2025/11/7.
//

#ifndef ESP32_CONFIG_H
#define ESP32_CONFIG_H

#define WIFI_SSID       "cristPhone17"
#define WIFI_PASS       "wangyuting"
#define EMQX_SERVER     "172.20.10.5"  // **电脑IP，不是localhost**
#define EMQX_PORT       1883

// 2. MQTT认证（EMQX初始无认证可留空）
#define MQTT_USER       "admin"
#define MQTT_PASS       "cqupt520"

// 3. EMQX客户端ID（必须唯一）
#define MQTT_CLIENT_ID  "esp32_center"
#endif //ESP32_CONFIG_H