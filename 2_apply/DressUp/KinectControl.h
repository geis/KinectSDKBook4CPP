#pragma once

#include <iostream>
#include <sstream>

#include <Windows.h>
#include <NuiApi.h>

#include <opencv2/opencv.hpp>

#define ERROR_CHECK(ret)                                      \
  if(ret != S_OK) {                                           \
  std::stringstream ss;                                       \
  ss << "failed " #ret " " << std::hex << ret << std::endl;   \
  throw std::runtime_error(ss.str().c_str());                 \
  }

const NUI_IMAGE_RESOLUTION CAMERA_RESOLUTION = NUI_IMAGE_RESOLUTION_640x480;

class KinectControl
{
public:
  KinectControl(void);
  ~KinectControl(void);

  void initialize();
  void run();
  void setCloth( cv::Mat _clothImage, std::vector<cv::Point> _points);

private:
  INuiSensor *kinect;
  HANDLE imageStreamHandle;
  HANDLE depthStreamHandle;
  HANDLE streamEvent;

  DWORD width;
  DWORD height;

  void createInstance();
  void setRgbImage(cv::Mat& image);
  void setDepthImage(cv::Mat& image);
  void setSkeleton( cv::Mat& image );
  void setJoint( cv::Mat& image, int joint, Vector4 position );
  void fitCloth();

  cv::Mat rgbImage;
  cv::Mat depthImage;
  cv::Mat clothImage;
  cv::Mat userMask;
  cv::Mat trans;
  std::vector<cv::Point> joints;
  std::vector<cv::Point> points;
};

