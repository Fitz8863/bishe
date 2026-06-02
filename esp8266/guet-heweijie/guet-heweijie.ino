#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

const char *ssid = "GUET-WiFi";
const char *campus_account = "2200340118";
const char *campus_account_suffix = "@unicom";
const char *campus_password = "A159357zop";
const char *portal_probe_host = "www.baidu.com";
const char *portal_probe_path = "/";
const char *portal_login_base_url = "http://10.0.1.5:801/eportal/portal/login";

constexpr uint8_t max_login_attempts = 5;
constexpr unsigned long wifi_connect_timeout_ms = 30000;
constexpr unsigned long ip_wait_timeout_ms = 10000;
constexpr unsigned long login_request_timeout_ms = 10000;
constexpr unsigned long login_retry_delay_ms = 5000;
constexpr unsigned long http_probe_timeout_ms = 5000;

// MQTT Broker（保持 nn.txt 一致）
const char *mqtt_broker = "10.60.83.159";
const char *sub_topic  = "jetson/esp8266/cmd";
const char *pub_topic  = "jetson/esp8266/info";
const char *mqtt_username = "user";
const char *mqtt_password = "fuck123456";
const int mqtt_port = 1883;

// 设备 ID（保持 nn.txt 一致）
const char *device_id = "001";

// MQTT 心跳计时（保持 nn.txt 一致）
unsigned long lastHeartbeat = 0;
const long heartbeatInterval = 1000;
String usbSerialLineBuffer;

WiFiClient espClient;
PubSubClient client(espClient);

struct PortalContext {
  String redirectUrl;
  String wlanUserIp;
  String wlanAcName;
  String wlanAcIp;
  String wlanUserMac;
  String areaId;
};

bool waitForStationIP(unsigned long timeoutMs) {
  const unsigned long startedAt = millis();
  while (millis() - startedAt < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP()[0] != 0) {
      return true;
    }
    delay(250);
    yield();
  }
  return false;
}

String urlEncodeComponent(const String &value) {
  String encoded;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value.charAt(i);
    const bool isUnreserved =
      (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
      c == '-' || c == '_' || c == '.' || c == '~';
    if (isUnreserved) {
      encoded += c;
    } else {
      encoded += '%';
      encoded += hex[(static_cast<uint8_t>(c) >> 4) & 0x0F];
      encoded += hex[static_cast<uint8_t>(c) & 0x0F];
    }
  }
  return encoded;
}

String base64EncodeString(const String &value) {
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String encoded;
  for (size_t i = 0; i < value.length(); i += 3) {
    const uint32_t octetA = static_cast<uint8_t>(value.charAt(i));
    const uint32_t octetB = (i + 1 < value.length()) ? static_cast<uint8_t>(value.charAt(i + 1)) : 0;
    const uint32_t octetC = (i + 2 < value.length()) ? static_cast<uint8_t>(value.charAt(i + 2)) : 0;
    const uint32_t triple = (octetA << 16) | (octetB << 8) | octetC;

    encoded += alphabet[(triple >> 18) & 0x3F];
    encoded += alphabet[(triple >> 12) & 0x3F];
    encoded += (i + 1 < value.length()) ? alphabet[(triple >> 6) & 0x3F] : '=';
    encoded += (i + 2 < value.length()) ? alphabet[triple & 0x3F] : '=';
  }
  return encoded;
}

String formatPortalMacAddress() {
  String mac = WiFi.macAddress();
  mac.toLowerCase();
  mac.replace(":", "");
  return mac;
}

String extractQueryParam(const String &url, const String &key) {
  const String token = key + "=";
  const int queryStart = url.indexOf('?');
  if (queryStart < 0) return String();

  int valueStart = url.indexOf(token, queryStart + 1);
  if (valueStart < 0) return String();

  valueStart += token.length();
  int valueEnd = url.indexOf('&', valueStart);
  if (valueEnd < 0) valueEnd = url.length();

  String value = url.substring(valueStart, valueEnd);
  value.replace("%2F", "/");
  value.replace("%3A", ":");
  value.replace("%2D", "-");
  value.replace("%40", "@");
  return value;
}

bool capturePortalRedirect(PortalContext &context) {
  WiFiClient probeClient;
  probeClient.setTimeout(http_probe_timeout_ms);

  if (!probeClient.connect(portal_probe_host, 80)) {
    Serial.println("[Portal] 无法建立外部 HTTP 探测连接。");
    return false;
  }

  probeClient.printf("HEAD %s HTTP/1.1\r\n", portal_probe_path);
  probeClient.printf("Host: %s\r\n", portal_probe_host);
  probeClient.print("Connection: close\r\n\r\n");

  const unsigned long startedAt = millis();
  while (!probeClient.available() && millis() - startedAt < http_probe_timeout_ms) {
    delay(10);
    yield();
  }

  if (!probeClient.available()) {
    Serial.println("[Portal] 等待 portal 重定向响应超时。");
    probeClient.stop();
    return false;
  }

  while (probeClient.available()) {
    String headerLine = probeClient.readStringUntil('\n');
    headerLine.trim();
    if (headerLine.length() == 0) break;
    if (headerLine.startsWith("Location:")) {
      context.redirectUrl = headerLine.substring(9);
      context.redirectUrl.trim();
      break;
    }
  }

  probeClient.stop();

  if (context.redirectUrl.length() == 0) {
    Serial.println("[Portal] 当前没有检测到认证页重定向，认为校园网已可用。");
    return false;
  }

  context.wlanUserIp = extractQueryParam(context.redirectUrl, "wlanuserip");
  context.wlanAcName = extractQueryParam(context.redirectUrl, "wlanacname");
  context.wlanAcIp = extractQueryParam(context.redirectUrl, "wlanacip");
  context.wlanUserMac = extractQueryParam(context.redirectUrl, "wlanusermac");
  context.areaId = extractQueryParam(context.redirectUrl, "areaID");

  if (context.wlanUserIp.length() == 0) context.wlanUserIp = WiFi.localIP().toString();
  if (context.wlanAcIp.length() == 0) context.wlanAcIp = WiFi.gatewayIP().toString();
  if (context.wlanUserMac.length() == 0) context.wlanUserMac = formatPortalMacAddress();

  Serial.printf("[Portal] Location: %s\n", context.redirectUrl.c_str());
  return true;
}

bool portalAccessStillIntercepted() {
  PortalContext context;
  if (!capturePortalRedirect(context)) {
    return false;
  }
  return context.redirectUrl.indexOf("/a79.htm") >= 0;
}

bool portalLoginResponseAccepted(const String &responseBody) {
  String normalized = responseBody;
  normalized.trim();
  const bool success = normalized.indexOf("\"result\":1") >= 0 ||
                       normalized.indexOf("\"result\":\"1\"") >= 0 ||
                       normalized.indexOf("已经在线") >= 0;
  const bool failure = normalized.indexOf("密码错误") >= 0 ||
                       normalized.indexOf("账号不存在") >= 0 ||
                       normalized.indexOf("ldap auth error") >= 0 ||
                       normalized.indexOf("AC认证失败") >= 0;
  return success && !failure;
}

void warmupPortalPage(const String &redirectUrl) {
  if (redirectUrl.length() == 0) return;

  WiFiClient warmupClient;
  HTTPClient http;
  http.setTimeout(http_probe_timeout_ms);
  if (!http.begin(warmupClient, redirectUrl)) return;
  http.addHeader("User-Agent", "Mozilla/5.0");
  http.addHeader("Connection", "close");
  http.GET();
  http.end();
}

String buildPortalLoginUrl(const PortalContext &context) {
  const String userAccount = String(",0,") + campus_account + campus_account_suffix;

  String url = String(portal_login_base_url) + "?callback=dr1004";
  url += "&login_method=1";
  url += "&user_account=" + urlEncodeComponent(userAccount);
  url += "&user_password=" + urlEncodeComponent(base64EncodeString(String(campus_password)));
  url += "&wlan_user_ip=" + urlEncodeComponent(context.wlanUserIp);
  url += "&wlan_user_ipv6=";
  url += "&wlan_user_mac=" + urlEncodeComponent(context.wlanUserMac);
  url += "&wlan_ac_ip=" + urlEncodeComponent(context.wlanAcIp);
  url += "&wlan_ac_name=" + urlEncodeComponent(context.wlanAcName);
  url += "&jsVersion=4.2";
  url += "&terminal_type=1";
  url += "&lang=zh-cn";
  url += "&v=" + String(random(500, 9999));
  url += "&lang=zh";
  return url;
}

bool performPortalLogin(const PortalContext &context) {
  WiFiClient loginClient;
  HTTPClient http;
  http.setTimeout(login_request_timeout_ms);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  const String loginUrl = buildPortalLoginUrl(context);
  if (!http.begin(loginClient, loginUrl)) {
    Serial.println("[Portal] 无法初始化登录请求。");
    return false;
  }

  http.addHeader("User-Agent", "Mozilla/5.0");
  http.addHeader("Accept", "*/*");
  http.addHeader("Referer", context.redirectUrl.length() > 0 ? context.redirectUrl : "http://10.0.1.5/a79.htm");
  http.addHeader("Connection", "close");

  const int httpCode = http.GET();
  const String responseBody = httpCode > 0 ? http.getString() : String();
  http.end();

  Serial.printf("[Portal] HTTP=%d\n", httpCode);
  Serial.printf("[Portal] 响应=%s\n", responseBody.c_str());
  return httpCode == HTTP_CODE_OK && portalLoginResponseAccepted(responseBody);
}

bool connectCampusNetwork() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(ssid);

  const unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    yield();
    Serial.print(".");
    if (millis() - wifiStart >= wifi_connect_timeout_ms) {
      Serial.println("\nWiFi 连接超时");
      return false;
    }
  }

  if (!waitForStationIP(ip_wait_timeout_ms)) {
    Serial.println("\n已连接 AP 但获取 IP 超时");
    return false;
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("当前 IP: ");
  Serial.println(WiFi.localIP());

  for (uint8_t attempt = 1; attempt <= max_login_attempts; ++attempt) {
    PortalContext context;
    Serial.printf("[Portal] 第 %u/%u 次认证尝试\n", attempt, max_login_attempts);

    if (!capturePortalRedirect(context)) {
      return true;
    }

    warmupPortalPage(context.redirectUrl);
    if (performPortalLogin(context)) {
      delay(login_retry_delay_ms);
      if (!portalAccessStillIntercepted()) {
        Serial.println("[Portal] 校园网认证成功");
        return true;
      }
    }

    delay(login_retry_delay_ms);
    yield();
  }

  Serial.println("[Portal] 校园网认证失败");
  return false;
}

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

  // 与 nn.txt 行为一致：通过硬件 Serial 下发六字节二进制流
  Serial.write(packet, 6);

  // 调试打印 (字符串日志打印到主串口)
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

// MQTT 回调（保持 nn.txt 一致）
void callback(char *topic, byte *payload, unsigned int length) {
  String messageTemp;
  for (unsigned int i = 0; i < length; i++) {
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

  const char *rx_device_id = doc["id"];

  // 匹配设备 ID
  if (rx_device_id != nullptr && strcmp(rx_device_id, device_id) == 0) {
    Serial.println("接收数据是一致的，准备往串口发送数据包\n");

    int8_t col = doc["col"];
    int8_t row = doc["row"];

    // 执行串口打包发送
    sendSerialPacket(col, row);
  }
  else if (rx_device_id != nullptr && strcmp(rx_device_id, device_id) != 0) {
    Serial.println("远程下发的控制指令，id 对应不上当前设备 id，停止发送串口指令\n");
  }
}

/**
 * MQTT 重连函数
 * 如果断开连接，会一直循环尝试重连，每次间隔 3 秒
 */
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("尝试连接 MQTT...");

    String client_id = "esp8266-neko-";
    client_id += String(WiFi.macAddress());

    // 尝试连接
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("已连接");

      // 连接成功后重新订阅主题
      client.subscribe(sub_topic);
    } else {
      Serial.print("失败，状态码=");
      Serial.print(client.state());
      Serial.println("，3秒后重试");

      // 等待 3 秒后再次尝试
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(micros());

  // WiFi 改为校园网认证连接
  if (!connectCampusNetwork()) {
    Serial.println("[System] 校园网连接失败，setup 提前结束。");
    return;
  }

  // 配置 MQTT（保持 nn.txt 一致）
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

  // 串口透传逻辑（改为非阻塞，避免拖慢 MQTT 回调）
  while (Serial.available()) {
    const char incomingChar = static_cast<char>(Serial.read());

    if (incomingChar == '\r') {
      continue;
    }

    if (incomingChar == '\n') {
      if (usbSerialLineBuffer.length() > 0) {
        client.publish(pub_topic, usbSerialLineBuffer.c_str());
        usbSerialLineBuffer = "";
      }
      continue;
    }

    usbSerialLineBuffer += incomingChar;
    if (usbSerialLineBuffer.length() > 256) {
      usbSerialLineBuffer = "";
    }
  }
}
