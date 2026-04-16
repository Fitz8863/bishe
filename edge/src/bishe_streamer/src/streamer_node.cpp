#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/image_transport.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <rclcpp/executors/multi_threaded_executor.hpp>
#include "bishe_msgs/msg/detector_result.hpp"
#include <chrono>
#include <atomic>
#include <mutex>

class StreamerNode : public rclcpp::Node
{
public:
    StreamerNode()
        : Node("streamer_node")
    {
        this->declare_parameter<float>("scale", 1.0);
        this->declare_parameter<std::string>("rtsp_url", "rtsp://localhost:8554/stream");
        this->declare_parameter<std::string>("audio_device", "hw:0,0");
        this->declare_parameter<int>("framerate", 60);
        this->declare_parameter<int>("output_width", 0);
        this->declare_parameter<int>("output_height", 0);
        this->declare_parameter<double>("output_fps", 0.0);

        this->get_parameter("rtsp_url", rtsp_url_);
        this->get_parameter("scale", scale_);
        this->get_parameter("audio_device", audio_device_);
        this->get_parameter("framerate", framerate_);

        stream_callback_group_ = this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
        rclcpp::SubscriptionOptions sub_options;
        sub_options.callback_group = stream_callback_group_;

        result_sub_ = this->create_subscription<bishe_msgs::msg::DetectorResult>(
            "detector/result", 10, std::bind(&StreamerNode::resultCallback, this, std::placeholders::_1), sub_options);

        RCLCPP_INFO(this->get_logger(), "推流节点开启, 推流地址: %s", rtsp_url_.c_str());
    }

    ~StreamerNode()
    {
        RCLCPP_INFO(this->get_logger(), "正在关闭推流管线...");
        if (writer_.isOpened())
        {
            writer_.release();
        }
        RCLCPP_INFO(this->get_logger(), "推流节点已安全退出");
    }

private:
    uint64_t written_frames_{ 0 };
    std::atomic<uint64_t> received_frames_{ 0 };
    std::atomic<uint64_t> write_completed_frames_{ 0 };
    double write_time_ms_acc_{ 0.0 };
    std::mutex stats_mutex_;
    std::chrono::steady_clock::time_point fps_window_start_{ std::chrono::steady_clock::now() };
    double current_fps_{ 0.0 };

    void reportPipelineStats()
    {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - fps_window_start_).count();
        if (elapsed_ms < 1000)
        {
            return;
        }

        const double seconds = static_cast<double>(elapsed_ms) / 1000.0;
        const uint64_t received = received_frames_.exchange(0);
        const uint64_t written = write_completed_frames_.exchange(0);
        double write_time_ms = 0.0;
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            write_time_ms = write_time_ms_acc_;
            write_time_ms_acc_ = 0.0;
        }
        const double avg_write_ms = written > 0 ? write_time_ms / static_cast<double>(written) : 0.0;

        // RCLCPP_INFO(
        //     this->get_logger(),
        //     "streamer 接收FPS: %.2f, 写出FPS: %.2f, 平均writer耗时: %.2fms",
        //     static_cast<double>(received) / seconds,
        //     static_cast<double>(written) / seconds,
        //     avg_write_ms);

        fps_window_start_ = now;
    }

    void updateFps()
    {
        ++written_frames_;
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - fps_window_start_).count();
        if (elapsed_ms >= 1000)
        {
            current_fps_ = static_cast<double>(written_frames_) * 1000.0 / static_cast<double>(elapsed_ms);
            written_frames_ = 0;
            syncRuntimeParameters();
        }
    }

    void syncRuntimeParameters()
    {
        const std::vector<rclcpp::Parameter> runtime_params = {
            rclcpp::Parameter("output_width", output_width_),
            rclcpp::Parameter("output_height", output_height_),
            rclcpp::Parameter("output_fps", current_fps_)
        };

        const auto results = this->set_parameters(runtime_params);
        for (size_t i = 0; i < results.size(); ++i)
        {
            if (!results[i].successful)
            {
                RCLCPP_WARN_THROTTLE(
                    this->get_logger(), *this->get_clock(), 5000,
                    "更新推流运行时参数 %s 失败: %s",
                    runtime_params[i].get_name().c_str(), results[i].reason.c_str());
            }
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

    void resultCallback(const bishe_msgs::msg::DetectorResult::ConstSharedPtr& msg)
    {
        try
        {
            received_frames_.fetch_add(1);
            cv::Mat frame = cv_bridge::toCvCopy(msg->annotated_image, "bgr8")->image;
            if (frame.empty()) return;

            int width = frame.cols;
            int height = frame.rows;
            int target_width = static_cast<int>(width * scale_);
            int target_height = static_cast<int>(height * scale_);
            cv::Size target_size(target_width, target_height);

            if (target_width != output_width_ || target_height != output_height_)
            {
                output_width_ = target_width;
                output_height_ = target_height;
                syncRuntimeParameters();
            }

            if (!writer_.isOpened())
            {
                initWriter(target_width, target_height);
                if (!writer_.isOpened()) return;
            }

            cv::Mat out_frame;
            if (scale_ == 1.0f) {
                out_frame = frame;
            }
            else {
                static cv::Mat resized_frame;
                cv::resize(frame, resized_frame, target_size, 0.0, 0.0, cv::INTER_LINEAR);
                out_frame = resized_frame;
            }

            updateFps();

            if (!rclcpp::ok()) return;

            if (current_fps_ > 0.0) {
                std::string fps_text = cv::format("FPS: %.1f", current_fps_);
                int font_face = cv::FONT_HERSHEY_SIMPLEX;
                double font_scale = std::max(0.5, out_frame.rows / 720.0);
                int thickness = std::max(1, static_cast<int>(2 * font_scale));
                int baseline = 0;
                cv::Size text_size = cv::getTextSize(fps_text, font_face, font_scale, thickness, &baseline);

                cv::Point text_org(out_frame.cols - text_size.width - 20, text_size.height + 20);

                cv::putText(out_frame, fps_text, text_org, font_face, font_scale, cv::Scalar(0, 255, 0), thickness, cv::LINE_AA);
            }

            const auto write_start = std::chrono::steady_clock::now();
            writer_.write(out_frame);
            const auto write_end = std::chrono::steady_clock::now();
            const double write_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(write_end - write_start).count()) / 1000.0;
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                write_time_ms_acc_ += write_ms;
            }
            write_completed_frames_.fetch_add(1);
            reportPipelineStats();
        }
        catch (const cv_bridge::Exception& e)
        {
            RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
        }
        catch (const std::exception& e)
        {
            RCLCPP_ERROR(this->get_logger(), "Exception during video write: %s", e.what());
        }
    }

    rclcpp::Subscription<bishe_msgs::msg::DetectorResult>::SharedPtr result_sub_;
    rclcpp::CallbackGroup::SharedPtr stream_callback_group_;
    cv::VideoWriter writer_;
    std::string rtsp_url_;
    std::string audio_device_;
    float scale_;
    int framerate_;
    int output_width_{ 0 };
    int output_height_{ 0 };
};

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<StreamerNode>();
    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(node);
    executor.spin();
    rclcpp::shutdown();
    return 0;
}
