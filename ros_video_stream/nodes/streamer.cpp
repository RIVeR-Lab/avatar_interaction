#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <opencv2/highgui/highgui.hpp>
#include <cv_bridge/cv_bridge.h>
#include <chrono>

int main(int argc, char **argv)
{
  ros::init(argc, argv, "webcam_stream");
  ros::NodeHandle nh;

  // Create an ImageTransport object
  image_transport::ImageTransport it(nh);

  // Create a publisher for the video stream
  image_transport::Publisher pub = it.advertise("camera/image_raw", 1);

  // Initialize the webcam
  cv::VideoCapture cap(2, cv::CAP_V4L2);
  if (!cap.isOpened())
  {
    ROS_ERROR("Failed to open the webcam!");
    return -1;
  }

  // // Set the resolution of the webcam
  cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);
  cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);
  cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
  cap.set(cv::CAP_PROP_FPS, 60);
  int fourcc = static_cast<int>(cap.get(cv::CAP_PROP_FOURCC));
  std::cout << "Captured using format(fourcc): " << 
                static_cast<char>(fourcc & 0xFF) << 
                static_cast<char>((fourcc >> 8) & 0xFF) <<
                static_cast<char>((fourcc >> 16) & 0xFF) <<
                static_cast<char>((fourcc >> 24) & 0xFF) << std::endl;

  std::cout << "Set Framerate: " << cap.get(cv::CAP_PROP_FPS) << std::endl;
  // Create a loop to capture and publish the video stream
  ros::Rate loop_rate(100);
  int idx = 0;
  while (ros::ok())
  {
    cv::Mat frame;
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    cap >> frame;
    std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
    std::cout << "time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << std::endl;
    // std::cout << "Actuall framerate: " << 1.0 / std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() * 1000 << std::endl;
    // Check if the frame is empty
    if (frame.empty())
    {
      std::cerr << "Failed to capture frame" << std::endl;
      break;
    }
    // std::cout << "frame codec: " << frame.type() << std::endl;
    // std::string filename = "/home/rui/bag/" + std::to_string(idx) + ".png";
    // cv::imwrite(filename, frame);

    // Convert the OpenCV image to a ROS message
    std_msgs::Header header;
    header.stamp = ros::Time::now();
    sensor_msgs::ImagePtr msg = cv_bridge::CvImage(header, "bgr8", frame).toImageMsg();

    // Publish the message
    pub.publish(msg);

    idx++;
    // ros::spinOnce();
    loop_rate.sleep();
  }

  // Release the webcam and exit
  cap.release();
  return 0;
}
