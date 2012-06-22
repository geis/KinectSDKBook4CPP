#include "KinectControl.h"

void main()
{
  try {
    KinectControl kinect;
    kinect.initialize();
    kinect.run();
  }
  catch ( std::exception& ex ) {
    std::cout << ex.what() << std::endl;
  }
}
