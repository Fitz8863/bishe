#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/image_transport.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include "bishe_msgs/msg/detector_result.hpp"
#include <chrono>

class StreamerNode : public rclcpp::Node
{
public:
    StreamerNode()
        : Node("streamer_node")
    {
        this->declare_parameter<float>("scale", 1.0);
        this->declare_parameter<std::string>("rtsp_url", "rtsp://localhost:8554/stream");
        this->declare_parameter<std::string>("audio_device", "hw:1,0");
        this->declare_parameter<int>("framerate", 60);

        this->get_parameter("rtsp_url", rtsp_url_);
        this->get_parameter("scale", scale_);
        this->get_parameter("audio_device", audio_device_);
        this->get_parameter("framerate", framerate_);

        result_sub_ = this->create_subscription<bishe_msgs::msg::DetectorResult>(
            "detector/result", 10, std::bind(&StreamerNode::resultCallback, this, std::placeholders::_1));

        RCLCPP_INFO(this->get_logger(), "推流节点开启, 推流地址: %s", rtsp_url_.c_str());
    }

    ~StreamerNode()
    {
        if (writer_.isOpened())
        {
            writer_.release();
        }
    }

private:
    uint64_t written_frames_{0};
    std::chrono::steady_clock::time_point fps_window_start_{std::chrono::steady_clock::now()};

    void reportFps()
    {
        ++written_frames_;
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - fps_window_start_).count();
        if (elapsed_ms >= 1000)
        {
            const double fps = static_cast<double>(written_frames_) * 1000.0 / static_cast<double>(elapsed_ms);
            RCLCPP_INFO(this->get_logger(), "streamer输出FPS: %.2f", fps);
            written_frames_ = 0;
            fps_window_start_ = now;
        }
    }

    void initWriter(int target_width, int target_height)
    {
        double fps = static_cast<double>(framerate_);
        cv::Size target_size(target_width, target_height);

        std::string rtsp_out_full =
            "appsrc is-live=true do-timestamp=true ! videoconvert ! video/x-raw,format=I420 ! "
            "x264enc bitrate=8000 speed-preset=ultrafast tune=zerolatency key-int-max=30 ! h264parse ! queue ! sink. "
            "alsasrc device=" + audio_device_ + " ! audioconvert ! audioresample ! "
            "voaacenc bitrate=128000 ! aacparse ! queue ! sink. "
            "rtspclientsink location=" + rtsp_url_ + " name=sink";

        RCLCPP_INFO(this->get_logger(), "尝试开启音视频复合推流管线...");
        writer_.open(rtsp_out_full, cv::CAP_GSTREAMER, 0, fps, target_size, true);

        if (!writer_.isOpened())
        {
            RCLCPP_WARN(this->get_logger(), "音视频复合管线开启失败（可能是音频设备 %s 被占用或不存在），尝试纯视频模式...", audio_device_.c_str());
            
            std::string rtsp_out_video_only =
                "appsrc is-live=true do-timestamp=true ! videoconvert ! video/x-raw,format=I420 ! "
                "x264enc bitrate=8000 speed-preset=ultrafast tune=zerolatency key-int-max=30 ! h264parse ! "
                "rtspclientsink location=" + rtsp_url_;

            writer_.open(rtsp_out_video_only, cv::CAP_GSTREAMER, 0, fps, target_size, true);
        }

        if (!writer_.isOpened())
        {
            RCLCPP_ERROR(this->get_logger(), "错误: 推流管道开启完全失败，请检查 RTSP 地址或 GStreamer 环境");
        }
        else
        {
            RCLCPP_INFO(this->get_logger(), "推流管线开启成功, 目标分辨率: %dx%d", target_width, target_height);
        }
    }

    void resultCallback(const bishe_msgs::msg::DetectorResult::ConstSharedPtr &msg)
    {
        try
        {
            cv::Mat frame = cv_bridge::toCvCopy(msg->annotated_image, "bgr8")->image;
            if (frame.empty()) return;

            int width = frame.cols;
            int height = frame.rows;
            int target_width = static_cast<int>(width * scale_);
            int target_height = static_cast<int>(height * scale_);
            cv::Size target_size(target_width, target_height);

            if (!writer_.isOpened())
            {
                initWriter(target_width, target_height);
                if (!writer_.isOpened()) return;
            }

            if (scale_ == 1.0f) {
                writer_.write(frame);
            } 
            else {
                static cv::Mat resized_frame;
                cv::resize(frame, resized_frame, target_size, 0.0, 0.0, cv::INTER_LINEAR);
                writer_.write(resized_frame);
            }
            
            // reportFps();
        }
        catch (const cv_bridge::Exception &e)
        {
            RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
        }
        catch (const std::exception &e)
        {
            RCLCPP_ERROR(this->get_logger(), "Exception during video write: %s", e.what());
        }
    }

    rclcpp::Subscription<bishe_msgs::msg::DetectorResult>::SharedPtr result_sub_;
    cv::VideoWriter writer_;
    std::string rtsp_url_;
    std::string audio_device_;
    float scale_;
    int framerate_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<StreamerNode>());
    rclcpp::shutdown();
    return 0;
}
