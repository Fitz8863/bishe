#include <rclcpp/rclcpp.hpp>
#include <mqtt/async_client.h>
#include <string>
#include <iostream>

class MqttCallback : public virtual mqtt::callback 
{
public:
  MqttCallback(rclcpp::Logger logger) : logger_(logger) {}

  void message_arrived(mqtt::const_message_ptr msg) override 
  {
    std::string topic = msg->get_topic();
    std::string payload = msg->get_payload_str();
    RCLCPP_INFO(logger_, "Received on %s: %s", topic.c_str(), payload.c_str());
  }

private:
  rclcpp::Logger logger_;
};

class MqttNode : public rclcpp::Node
{
public:
  MqttNode()
      : Node("mqtt_node"), cb_(this->get_logger())
  {
    // Declare parameters
    this->declare_parameter<std::string>("broker", "localhost");
    this->declare_parameter<int>("port", 1883);
    this->declare_parameter<std::string>("client_id", "bishe_camera_001");
    this->declare_parameter<std::string>("subscribe_topic", "factory/camera/001/command");
    this->declare_parameter<std::string>("publish_topic", "factory/camera/001/status");

    this->get_parameter("broker", broker_);
    this->get_parameter("port", port_);
    this->get_parameter("client_id", client_id_);
    this->get_parameter("subscribe_topic", subscribe_topic_);
    this->get_parameter("publish_topic", publish_topic_);

    // Initialize MQTT client
    std::string server_address = "tcp://" + broker_ + ":" + std::to_string(port_);
    client_ = std::make_unique<mqtt::async_client>(server_address, client_id_);

    // Set callback
    client_->set_callback(cb_);

    // Connect
    try {
      mqtt::connect_options conn_opts;
      conn_opts.set_keep_alive_interval(20);
      conn_opts.set_clean_session(true);
      client_->connect(conn_opts)->wait();
      RCLCPP_INFO(this->get_logger(), "Connected to MQTT broker: %s", server_address.c_str());

      // Subscribe to command topic
      client_->subscribe(subscribe_topic_, 1)->wait();
      RCLCPP_INFO(this->get_logger(), "Subscribed to: %s", subscribe_topic_.c_str());

    } catch (const mqtt::exception &e) {
      RCLCPP_ERROR(this->get_logger(), "MQTT connection failed: %s", e.what());
    }

    RCLCPP_INFO(this->get_logger(), "MQTT node started");
  }

  ~MqttNode()
  {
    try {
      client_->disconnect()->wait();
    } catch (const mqtt::exception &e) {
      RCLCPP_ERROR(this->get_logger(), "MQTT disconnect error: %s", e.what());
    }
  }

  // 预留的发布函数
  void publishStatus(const std::string& status)
  {
    if (client_->is_connected()) {
      try {
        auto msg = mqtt::make_message(publish_topic_, status, 1, false);
        client_->publish(msg)->wait();
        RCLCPP_INFO(this->get_logger(), "Published to %s: %s", publish_topic_.c_str(), status.c_str());
      } catch (const mqtt::exception &e) {
        RCLCPP_ERROR(this->get_logger(), "Publish failed: %s", e.what());
      }
    } else {
      RCLCPP_WARN(this->get_logger(), "Cannot publish, not connected");
    }
  }

  // 预留：发布抓拍结果
  void publishCaptureResult(const std::string& capture_id, const std::string& violation_type)
  {
    std::string payload = "{\"capture_id\":\"" + capture_id + "\",\"violation_type\":\"" + violation_type + "\"}";
    publishStatus(payload);
  }

  // 预留：发布系统状态
  void publishSystemStatus(const std::string& status, int violation_count)
  {
    std::string payload = "{\"status\":\"" + status + "\",\"violations\":" + std::to_string(violation_count) + "}";
    publishStatus(payload);
  }

private:
  std::unique_ptr<mqtt::async_client> client_;
  MqttCallback cb_;
  std::string broker_;
  int port_;
  std::string client_id_;
  std::string subscribe_topic_;
  std::string publish_topic_;
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MqttNode>());
  rclcpp::shutdown();
  return 0;
}
