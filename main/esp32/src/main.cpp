#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "MyConfig.h"
#include "PackSent.h"
#include <PubSubClient.h>

uint8_t nodeCount = 0; // 结点数量
NodeInfo nodeList[MAX_NODE_SIZE]; // 注册结点表
uint8_t currentChannel = 0; // 信道

// 用于Mqtt通信
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Mqtt回调函数 —— 接收到Mqtt客户端数据后执行
// topic：接收到消息的主题名称
// payload：消息负载数据（实际内容）
// len：负载数据的长度
void mqttCallback(char* topic, byte* payload, unsigned int len) {
    Serial.print("EMQX命令: ");
    Serial.println(topic);
}

// 连接MQTT服务器
void connectMQTT() {
    if (mqttClient.connected()) return;
    String clientId = MQTT_CLIENT_ID + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
        Serial.println("✓ MQTT连接成功");
        // 订阅 cmd/esp32/# 下的消息
        mqttClient.subscribe("cmd/esp32/#");
    }
}

// 发布到EMQX
void publishToEMQX(uint8_t nodeId, const char* type, const char* value) {
    char jsonMsg[128];
    uint16_t ts = millis();
    snprintf(jsonMsg, 128, "{\"nodeId\":%d, \"type\":\"%s\", \"value\":%s, \"ts\":%d}",
             nodeId, type, value, ts);
    // 在 data/esp32/nodes 主题发布数据
    mqttClient.publish("data/esp32/nodes", jsonMsg);
}


// 参数说明：
// mac参数
// 类型：const uint8_t *
// 长度：6字节
// 内容：发送方设备的MAC地址
// data参数
// 类型：const uint8_t *
// 内容：接收到的数据指针
// 格式：二进制数据流
// len参数
// 类型：int
// 内容：接收数据的长度（字节数）
// 限制：ESP-NOW最大250字节
// 接收到esp8266发送的连接请求，进行回复
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    // 如果接收到的数据长度与我们定义的数据包长度不同，则说明接收到的数据包有误
    if (len != sizeof(Packet)) return;

    Packet *packet = (Packet *)data; // 将数据包指针转换成数据包结构体指针

    // 数据包类型：0 注册请求，1 注册确认，2 数据包
    // 如果是注册请求
    if (packet->type == 0) {
        // 检查结点是否已经被注册
        for (int i = 0; i < nodeCount; i++) {
            if (memcmp(nodeList[i].mac, mac, 6) == 0) {
                nodeList[i].lastSeen = millis(); // 更新最后响应时间
                sendRegisterAck(mac, packet->nodeId);
                return;
            }
        }
        // 未注册结点
        // 进行新结点注册
        // 检查是否超过最大值
        if (nodeCount < MAX_NODE_SIZE) {
            memcpy(nodeList[nodeCount].mac, mac, 6);
            nodeList[nodeCount].nodeId = nodeCount;
            nodeList[nodeCount].isActive = true;
            nodeList[nodeCount].lastSeen = millis();
            // 添加到peer
            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, mac, 6);
            peerInfo.channel = currentChannel;;
            peerInfo.encrypt = false;
            esp_now_add_peer(&peerInfo);
            Serial.printf("结点[%d]注册成功，MAC地址：[%p]\n", nodeCount, mac);
            // 注册成功后，进行广播确认
            sendRegisterAck(mac, nodeList[nodeCount].nodeId);
            nodeCount++;
            // esp8266的逻辑 （不由mac地址检查，由nodeId检查）
            // 1. 检查自己是否处于活跃状态
            // 2. 若自己没有处于活跃状态，则向esp32发送注册请求
            //    1. 先尝试发送注册请求
            //    2. 如果发送失败，则进行重试，直到成功
            // 3. 接收esp32发送的注册确认
            // 4. 更新自己的活跃状态
        }
    } else if (packet->type == 1) {
    } else if (packet->type == 2) {
        // 找到发送结点
        for (int i = 0; i < nodeCount; i++) {
            if (memcmp(nodeList[i].mac, mac, 6) == 0) {
                nodeList[i].lastSeen = millis(); // 更新最后响应时间
                Serial.printf("结点[%d]发送了消息：[%s]\n", i, packet->message);
                publishToEMQX(packet->nodeId, "message", packet->message);
                break;
            }
        }
    }

}


void setup() {
    Serial.begin(115200);

    do {
        // 开启 WiFi 并设置通道
        WiFi.mode(WIFI_MODE_APSTA);
        // WiFi.disconnect();
        // 连接Wifi
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        Serial.println("连接WIFI");
        delay(50);
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(500);
        }
        Serial.println("\nWiFi: " + WiFi.localIP().toString());

        currentChannel = WiFi.channel();
        Serial.printf("Wi-Fi 信道: %d\n", currentChannel);
        delay(1000);
        esp_wifi_start();
        esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

    }while (esp_now_init() != ESP_OK);

    esp_now_peer_info_t peerInfo = {};

    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = currentChannel;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo); // 将广播加入esp_now网络
    esp_now_register_recv_cb(onDataRecv); // 通过esp_now网络接收到信息后，调用onDataRecv回调函数

    Serial.println("ESP32 Center ESPNOW Already");

    // 连接 mqtt 服务器 emqx

    mqttClient.setServer(EMQX_SERVER, EMQX_PORT);
    mqttClient.setCallback(mqttCallback);
}

void loop() {

    static uint32_t last = 0;
    // 间隔10s向节点轮询发送消息
    if (millis() - last > 10000 && nodeCount > 0) {
        for (uint8_t i = 0; i < nodeCount; i++) {
            char message[64];
            strncpy(message, "Hello, I'm ESP32 Center", sizeof(message) - 1);
            message[sizeof(message) - 1] = '\0'; // 确保字符串结束符
            last = millis();
            Packet packet;
            packet.type = 2;
            packet.nodeId = i;
            strncpy(packet.message, message, sizeof(packet.message) - 1);
            packet.message[sizeof(packet.message) - 1] = '\0'; // 确保字符串结尾安全
            esp_now_send(nodeList[i].mac, (uint8_t *)&packet, sizeof(packet));
            Serial.println("已发送");
            delay(50);
        }
    }

    for (uint8_t i = 0; i < nodeCount; i++) {
        nodeList[i].isActive = nodeList[i].lastSeen > millis() - 10000; // 更新活跃状态
        if (!nodeList[i].isActive) {
            // 从peer中删除失活结点
            esp_err_t result = esp_now_del_peer(nodeList[i].mac);
            delay(50);
            if (result == ESP_OK) {
                // 从结点列表中删除结点
                for (uint8_t j = i; j < nodeCount - 1; j++) {
                    nodeList[j] = nodeList[j + 1];
                }
                nodeCount--;
                memset(&nodeList[nodeCount], 0, sizeof(NodeInfo));
                Serial.printf("Node[%d] delete SUCCESS\n", i);
                return;
            } else {
                Serial.printf("Node[%d] delete FAIL\n", i);
            }
        }
    }


    if (!mqttClient.connected()) {
        connectMQTT();
    } else {
        mqttClient.loop();
    }



}





