#pragma once

#include <iostream>
#include <sstream>

#include <Windows.h>
#include <NuiApi.h>

#include <opencv2/opencv.hpp>

#define _USE_MATH_DEFINES
#include <math.h>

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
  void setDepthImage(cv::Mat& image);
  void setSkeleton( cv::Mat& image );
  void setJoint( cv::Mat& image, int joint, Vector4 position );
  void setHandImage( Vector4 handPos, Vector4 wristPos, cv::Mat &image, std::string handName = "hand" );

  cv::Mat rgbImage;
  cv::Mat depthImage;
  cv::Mat lhImage;
  cv::Mat rhImage;

  Vector4 lhPos;  // 左手の位置
  Vector4 lwPos;  // 左手首の位置
  Vector4 rhPos;  // 右手の位置
  Vector4 rwPos;  // 右手首の位置
};

