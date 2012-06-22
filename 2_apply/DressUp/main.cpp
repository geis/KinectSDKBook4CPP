#include "ClothSetting.h"

void main()
{
  try {
    ClothSetting cloth;
    cloth.setClothImage( "tshirts.png" );
  }
  catch ( std::exception& ex ) {
    std::cout << ex.what() << std::endl;
  }
}