#include "ClothSetting.h"


ClothSetting::ClothSetting(void)
{
}


ClothSetting::~ClothSetting(void)
{
}

void ClothSetting::setClothImage( std::string fileName )
{
  try {
    cloth = cv::imread( fileName, CV_LOAD_IMAGE_UNCHANGED );

    if( !cloth.empty() ) {
      cv::imshow( "ClothImage", cloth );
      cv::setMouseCallback( "ClothImage", _mouseCallback, this );

      marked = cloth.clone();

      cv::waitKey( 0 );
    }
  }
  catch( std::exception &ex ) {
    std::cerr << "ClothSetting::setClothImage" << ex.what() << std::endl;
  }
}

std::vector<cv::Point> ClothSetting::getPoints()
{
  return points;

}

cv::Mat ClothSetting::getClothImage()
{
  return cloth;
}

void ClothSetting::mouseCallback( int event, int x, int y, int flags )
{
  try {
    if( event == CV_EVENT_LBUTTONDOWN ) {
      if( points.size() < 4 ) {
        points.push_back( cv::Point( x, y ) );

        cv::circle( marked, cv::Point( x, y ), 20, cv::Scalar( 255, 0, 0 ), 10 );
        cv::imshow( "ClothImage", marked );
      }
      else if( points.size() == 4 ) {
        KinectControl kinect;
        kinect.setCloth( cloth, points );
        kinect.initialize();
        kinect.run();
      }
      else {
        points.clear();
        marked = cloth.clone();
      }
    }
  }
  catch ( std::exception& ex ) {
    std::cout << "ClothSetting::mouseCallback" << ex.what() << std::endl;
  }
}