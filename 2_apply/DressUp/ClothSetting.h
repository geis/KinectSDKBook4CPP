#pragma once

#include <opencv2/opencv.hpp>
#include "KinectControl.h"

class ClothSetting
{
public:
  ClothSetting(void);
  ~ClothSetting(void);

  void setClothImage( std::string fileName );
  std::vector<cv::Point> getPoints();
  cv::Mat getClothImage();

private:
  cv::Mat cloth;
  cv::Mat marked;
  std::vector<cv::Point> points;

  static void _mouseCallback( int event, int x, int y, int flags, void* param )
  {
    ((ClothSetting*)param)->mouseCallback( event, x, y, flags );
  }
  void mouseCallback( int event, int x, int y, int flags );
};

