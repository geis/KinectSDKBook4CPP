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

private:
  INuiSensor *kinect;
  HANDLE imageStreamHandle;
  HANDLE depthStreamHandle;
  HANDLE streamEvent;

  DWORD width;
  DWORD height;

  void createInstance();
  void setRgbImage(cv::Mat& image);
  void setSkeleton( cv::Mat& image );
  void setJoint( cv::Mat& image, int joint, Vector4 position );

  cv::Mat rgbImage;

  std::vector<cv::Point> joints;
};

