#include <vtr_sensors/xb3_replay.hpp>

#include <opencv2/opencv.hpp>

#include <chrono>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

Xb3Replay::Xb3Replay(const std::string &data_dir,
                     const std::string &stream_name, const std::string &topic,
                     const int qos)
    : Node("xb3_recorder"), reader_(data_dir, stream_name) {
  publisher_ = create_publisher<RigImages>(topic, qos);
  // \todo yuchen Main vtr node requires calibration data, so create a publisher
  // for it. However, in the old code, this should be a service (in robochunk)
  // Need to figure out where to put this.
  calibration_publisher_ =
      create_publisher<RigCalibration>("xb3_calibration", qos);
}

/// @brief Replay XB3 stereo images from a rosbag2
int main(int argc, char *argv[]) {

  // Default path
  fs::path data_dir{fs::current_path() / "xb3_data"};
  std::string stream_name = "front_xb3";
  bool manual_scrub = false;

  // User specified path
  if (argc == 4) {
    data_dir = argv[1];
    stream_name = argv[2];
    std::istringstream(argv[3]) >> std::boolalpha >> manual_scrub;
    if (manual_scrub){
      std::cout << "Manual replay selected. Press/hold any key to advance image stream." << std::endl;
    }
  } else if (argc != 1) {
    throw std::invalid_argument("Wrong number of arguments provided!");
  }

  rclcpp::init(argc, argv);
  auto replay = Xb3Replay(data_dir.string(), stream_name, "xb3_images");

  // \todo yuchen Main vtr node requires calibration data, so create a publisher
  // for it. However, in the old code, this should be a service (in robochunk)
  // Need to figure out where to put this.
  auto calibration_msg =
      replay.reader_.fetchCalibration()->get<RigCalibration>();
  replay.calibration_publisher_->publish(calibration_msg);
  // some delay required
  std::cout << "Sending calibration data" << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto message = replay.reader_.readNextFromSeek();
  
  uint64_t prev_stamp = 0;
  while (message.get()) {
    if (!rclcpp::ok()) break;

    auto image = message->template get<RigImages>();

    // \todo yuchen Add necessary info for vtr to run, but they should not be
    // here
    image.name = "front_xb3";
    image.vtr_header.sensor_time_stamp = image.channels[0].cameras[0].stamp;
    std::cout << "Publishing image with time stamp: "
              << image.vtr_header.sensor_time_stamp.nanoseconds_since_epoch
              << std::endl;

    // Publish message for use with offline tools
    replay.publisher_->publish(image);
    // Add a delay so that the image publishes at roughly the true rate.
    // std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Visualization
    auto left = image.channels[0].cameras[0];
    auto right = image.channels[0].cameras[1];

    // Replays images based on their timestamps. Converts nanoseconds to
    // milliseconds
    if (prev_stamp != 0) {
      if (manual_scrub) {
        cv::waitKey(0);
      } else {
        double delay = (left.stamp.nanoseconds_since_epoch - prev_stamp) * std::pow(10, -6);
        cv::waitKey(delay);
      }
    }
    prev_stamp = left.stamp.nanoseconds_since_epoch;

    // Get image parameters from left camera and assume right is the same
    int outputmode = -1;
    int datasize = 0;
    if (left.encoding == "bgr8") {
      datasize = left.height * left.width * 3;
      outputmode = CV_8UC3;
    } else if (left.encoding == "mono8") {
      datasize = left.height * left.width;
      outputmode = CV_8UC1;
    }

    // Create OpenCV images to be shown
    left.data.resize(datasize);
    cv::Mat cv_left =
        cv::Mat(left.height, left.width, outputmode, (void *)left.data.data());
    cv::imshow(image.channels[0].name + "/left", cv_left);
    right.data.resize(datasize);
    cv::Mat cv_right = cv::Mat(right.height, right.width, outputmode,
                               (void *)right.data.data());
    cv::imshow(image.channels[0].name + "/right", cv_right);

    message = replay.reader_.readNextFromSeek();
  }
  rclcpp::shutdown();
  return 0;
}
