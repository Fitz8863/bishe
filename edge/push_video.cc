#include <iostream>
#include <string>
#include <chrono>
#include <opencv2/cudawarping.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda.hpp>

using namespace cv;
using namespace std;

int main()
{
    // 1. 视频捕获管道 (保持不变)
    
    std::string pipeline =
        "v4l2src device=/dev/video0 ! "
        "image/jpeg, width=1280, height=720, framerate=60/1 ! "
        "nvv4l2decoder mjpegdecode=1 ! nvvidconv ! video/x-raw, format=BGRx ! "
        "videoconvert ! video/x-raw, format=BGR ! "
        "appsink";

    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "错误: 无法打开视频" << std::endl;
        return -1;
    }

    float scale = 0.9;


    int target_width = cap.get(cv::CAP_PROP_FRAME_WIDTH) * scale;
    int target_height = cap.get(cv::CAP_PROP_FRAME_HEIGHT) * scale;
    cv::Size target_size(target_width, target_height);
    double FPS = 60.0;

    // 2. 复杂的音视频复合推流管道
    // 我们定义两个分支：一个 appsrc(视频)，一个 alsasrc(音频)
    // 注意：rtspclientsink 会自动处理多路输入
    // 假设你的 USB 拾音器是 hw:1,0，如果不是，请根据 arecord -l 的结果修改
// 1. 确定你的 USB 拾音器地址
    std::string audio_device = "hw:0,0";

    // 2. 构造复合管道
    // 关键：appsrc 必须放在最前面
    std::string rtsp_out =
        "appsrc ! videoconvert ! video/x-raw,format=I420 ! "
        "x264enc bitrate=8000 speed-preset=ultrafast tune=zerolatency ! h264parse ! queue ! sink. "
        "alsasrc device=" + audio_device + " ! audioconvert ! audioresample ! "
        "voaacenc bitrate=128000 ! aacparse ! queue ! sink. "
        "rtspclientsink location=rtsp://localhost:8554/stream name=sink";  // rtsp://jetson-orin-nano:8554/stream

    cv::VideoWriter writer;
    // 注意：使用 CAP_GSTREAMER 模式，writer 实际上只负责向管道中的 vsource (appsrc) 喂数据
    writer.open(rtsp_out, cv::CAP_GSTREAMER, 0, FPS, target_size, true);

    if (!writer.isOpened()) {
        std::cerr << "错误: 推流管道打开失败" << std::endl;
        return -1;
    }

    cv::Mat frame, resized_frame;
    cv::cuda::GpuMat d_frame, d_resized_frame;

    while (cap.read(frame))
    {
        // CUDA 处理
        d_frame.upload(frame);
        cv::cuda::resize(d_frame, d_resized_frame, target_size);
        d_resized_frame.download(resized_frame);

        // 写入视频帧 (GStreamer 内部会自动与正在采集的音频同步)
        writer.write(resized_frame);
    }

    cap.release();
    writer.release();
    return 0;

    
}

