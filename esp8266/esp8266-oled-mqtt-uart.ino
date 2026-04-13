#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include "ACROBOTIC_SSD1306.h"  // OLED 库
#include "Wire.h"

// 串口变量定义
SoftwareSerial mySerial(D9, D10);

// const char *ssid = "ZTE-ExDsxH";
// const char *password = "520heweijie";

const char *ssid = "0721";
const char *password = "0d000721";

// MQTT Broker
const char *mqtt_broker = "10.60.83.159";
const char *sub_topic  = "jetson/esp8266/cmd";
const char *pub_topic  = "jetson/esp8266/info";
const char *mqtt_username = "user";
const char *mqtt_password = "fuck123456";
const int mqtt_port = 1883;

// 设备 ID
const char *device_id = "001";

// MQTT 心跳计时
unsigned long lastHeartbeat = 0;
const long heartbeatInterval = 1000;

WiFiClient espClient;
PubSubClient client(espClient);

/**
* 封装并发送串口数据包
* 注意：这里参数改成了 int8_t 以支持 -128 到 127
*/
void sendSerialPacket(int8_t col, int8_t row) {
  uint8_t packet[6];

  packet[0] = 0xAA; // 帧头
  packet[1] = 0x01; // 功能码
  
  // 强制转换为 uint8_t 存储。例如 -1 会变成 0xFF 存储，这是标准的补码形式
  packet[2] = (uint8_t)col; 
  packet[3] = (uint8_t)row;

  // 校验和计算：依然是前四个字节的累加和
  packet[4] = (uint8_t)(packet[0] + packet[1] + packet[2] + packet[3]);
  packet[5] = 0x55; // 帧尾

  // 1. 发送真正的二进制流给串口
  Serial.write(packet, 6);

  // 2. 调试打印
  Serial.println();
  Serial.println(">>>>>> [串口下发有符号二进制包] >>>>>>");
  Serial.printf("原始数值: Col=%d, Row=%d\n", col, row);
  Serial.print("HEX 字节: ");
  for (int i = 0; i < 6; i++) {
    if (packet[i] < 0x10) Serial.print("0");
    Serial.print(packet[i], HEX);
    Serial.print(" ");
  }
  Serial.println("\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

// MQTT 回调
void callback(char *topic, byte *payload, unsigned int length) {
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }

  Serial.println("\n[MQTT 接收到消息]");
  Serial.print("Payload: ");
  Serial.println(messageTemp);

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, messageTemp);

  if (error) {
    Serial.print("JSON 解析失败: ");
    Serial.println(error.f_str());
    return;
  }

  const char *rx_device_id = doc["device_id"];

  // 匹配设备 ID
  if (rx_device_id != nullptr && strcmp(rx_device_id, device_id) == 0) {
     Serial.println("接收数据是一致的，准备往串口发送数据包\n");
    
    // --- 重点修改：提取为有符号 8 位整数 ---
    int8_t col = doc["col"]; 
    int8_t row = doc["row"];

    // OLED 显示
    oled.clearDisplay();
    oled.setTextXY(0, 0);
    oled.putString("ID: ");
    oled.putString(device_id);

    oled.setTextXY(2, 0);
    oled.putString("Col: ");
    // 使用 String 转换可以正确显示负数符号
    oled.putString(String((int)col).c_str()); 

    oled.setTextXY(3, 0);
    oled.putString("Row: ");
    oled.putString(String((int)row).c_str());

    // 执行串口打包发送
    sendSerialPacket(col, row);
  }
  else if(rx_device_id !=nullptr && strcmp(rx_device_id,device_id)!=0){
    Serial.println("远程下发的控制指令，id对应不上当前设备id，停止发送串口指令\n");
  }
}

/**
 * MQTT 重连函数
 * 如果断开连接，会一直循环尝试重连，每次间隔 3 秒
 */
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("尝试连接 MQTT...");
    oled.setTextXY(3, 0);
    oled.putString("MQTT Connect...");

    String client_id = "esp8266-neko-";
    client_id += String(WiFi.macAddress());

    // 尝试连接
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("已连接");
      oled.setTextXY(3, 0);
      oled.putString("MQTT: OK!      "); // 用空格覆盖掉之前的长字符
      
      // 连接成功后重新订阅主题
      client.subscribe(sub_topic);
    } else {
      Serial.print("失败，状态码=");
      Serial.print(client.state());
      Serial.println("，3秒后重试");
      
      oled.setTextXY(3, 0);
      oled.putString("MQTT: Retry... ");
      
      // 等待 3 秒后再次尝试
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // 初始化 OLED
  Wire.begin(D2, D1);
  oled.init();
  oled.clearDisplay();

  // WiFi 连接
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  oled.setTextXY(0, 0);
  oled.putString("WiFi Connect...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  oled.clearDisplay();
  oled.setTextXY(0, 0);
  oled.putString("WiFi: OK!");
  oled.setTextXY(1, 0);
  oled.putString(WiFi.localIP().toString().c_str());

  // 配置 MQTT
  client.setServer(mqtt_broker, mqtt_port);
  client.setBufferSize(1024);
  client.setCallback(callback);

  // 初始连接 MQTT
  reconnectMQTT();
}

void sendHeartbeat() {
  if (client.connected()) {
    StaticJsonDocument<200> doc;
    doc["id"] = device_id;
    doc["status"] = "online";
    doc["uptime"] = millis() / 1000;
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    client.publish(pub_topic, jsonOutput.c_str());
  }
}

void loop() {
  // 实时检查 MQTT 连接状态，断开则自动重连
  if (!client.connected()) {
    reconnectMQTT();
  }
  
  // 维持 MQTT 底层通信
  client.loop();

  // 心跳发送逻辑
  unsigned long currentMillis = millis();
  if (currentMillis - lastHeartbeat >= heartbeatInterval) {
    lastHeartbeat = currentMillis;
    sendHeartbeat();
  }

  // 串口透传逻辑
  while (Serial.available()) {
    String incoming = Serial.readStringUntil('\n');
    client.publish(pub_topic, incoming.c_str());
  }
}