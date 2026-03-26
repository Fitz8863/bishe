#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>

class IntercomNode : public rclcpp::Node
{
public:
  IntercomNode() : Node("intercom_node"), child_pid_(-1)
  {
    sub_ = this->create_subscription<std_msgs::msg::String>(
      "intercom/control", 10,
      std::bind(&IntercomNode::controlCallback, this, std::placeholders::_1));

    RCLCPP_INFO(this->get_logger(), "远程通话节点 (Intercom Node) 已启动，等待拉流指令...");
  }

  ~IntercomNode() override
  {
    stopStream();
  }

private:
  void controlCallback(const std_msgs::msg::String::SharedPtr msg)
  {
    const std::string& command = msg->data;
    if (command == "STOP") {
      RCLCPP_INFO(this->get_logger(), "收到结束通话指令，停止拉流");
      stopStream();
    } else {
      RCLCPP_INFO(this->get_logger(), "收到开始通话指令，RTSP 地址: %s", command.c_str());
      startStream(command);
    }
  }

  void startStream(const std::string& url)
  {
    stopStream();

    RCLCPP_INFO(this->get_logger(), "使用 ffmpeg 拉流: %s", url.c_str());

    pid_t pid = fork();
    if (pid < 0) {
      RCLCPP_ERROR(this->get_logger(), "无法创建子进程 (fork failed)");
    } else if (pid == 0) {
      // 子进程：仅执行 execl，禁止调用任何 ROS 2 API
      // ffmpeg -i <url> -vn -f alsa default：丢弃视频，音频通过 ALSA 输出到默认设备，不依赖 DISPLAY
      execl("/usr/local/bin/ffmpeg", "ffmpeg",
            "-i", url.c_str(),
            "-vn",
            "-f", "alsa", "default",
            nullptr);
      _exit(1);
    } else {
      child_pid_ = pid;
      RCLCPP_INFO(this->get_logger(), "拉流子进程已启动 (PID: %d)", pid);
    }
  }

  void stopStream()
  {
    if (child_pid_ > 0) {
      RCLCPP_INFO(this->get_logger(), "正在终止拉流子进程 (PID: %d)...", child_pid_);

      kill(child_pid_, SIGTERM);

      int status;
      pid_t ret = waitpid(child_pid_, &status, WNOHANG);
      if (ret == 0) {
        usleep(500000);
        ret = waitpid(child_pid_, &status, WNOHANG);
        if (ret == 0) {
          RCLCPP_WARN(this->get_logger(), "子进程未响应 SIGTERM，发送 SIGKILL 强制终止");
          kill(child_pid_, SIGKILL);
          waitpid(child_pid_, &status, 0);
        }
      }

      RCLCPP_INFO(this->get_logger(), "拉流子进程已终止");
      child_pid_ = -1;
    }
  }

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_;
  pid_t child_pid_;
};

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<IntercomNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}