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
    const std::string &command = msg->data;
    if (command == "STOP")
    {
      RCLCPP_INFO(this->get_logger(), "收到结束通话指令，停止拉流");
      stopStream();
    }
    else
    {
      RCLCPP_INFO(this->get_logger(), "收到开始通话指令，RTSP 地址: %s", command.c_str());
      startStream(command);
    }
  }

  /**
   * @brief 开始语音拉流播放
   * 采用 ffplay 作为播放引擎，通过子进程执行，并指定输出到 USB 扬声器
   */
  void startStream(const std::string &url)
  {
    stopStream();

    RCLCPP_INFO(this->get_logger(), "使用 ffplay 拉流: %s", url.c_str());

    pid_t pid = fork();
    if (pid < 0)
    {
      RCLCPP_ERROR(this->get_logger(), "无法创建子进程 (fork failed)");
    }
    else if (pid == 0)
    {
      // 设置环境变量以指定 ALSA 设备为 hw:0 (USB 扬声器)
      // 这是为了确保 ffplay (SDL2) 能够直接找到并使用指定的硬件设备
      setenv("AUDIODEV", "hw:0", 1);
      
      // 使用 ffplay 播放音频参数说明:
      // -nodisp: 纯音频播放，不启动视频渲染窗口，节省系统资源
      // -autoexit: 当网络流结束或连接断开时，自动退出进程以便重连
      // -rtsp_transport tcp: 强制使用 TCP 传输。解决 RTSP over UDP 可能出现的 EOF 或花屏/卡顿问题
      execl("/usr/local/bin/ffplay", "ffplay",
            "-nodisp", 
            "-autoexit", 
            "-rtsp_transport", "tcp",
            "-i", url.c_str(),
            nullptr);
      _exit(1); // execl 失败时的保护
    }
    else
    {
      child_pid_ = pid;
      RCLCPP_INFO(this->get_logger(), "ffplay 拉流子进程已启动 (PID: %d)", pid);
    }
  }

  void stopStream()
  {
    if (child_pid_ > 0)
    {
      RCLCPP_INFO(this->get_logger(), "正在终止拉流子进程 (PID: %d)...", child_pid_);

      kill(child_pid_, SIGTERM);

      int status;
      pid_t ret = waitpid(child_pid_, &status, WNOHANG);
      if (ret == 0)
      {
        usleep(500000);
        ret = waitpid(child_pid_, &status, WNOHANG);
        if (ret == 0)
        {
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

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<IntercomNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}