#include "KinectControl.h"


KinectControl::KinectControl()
{
}


KinectControl::~KinectControl()
{
  if ( kinect != 0 ) {
    kinect->NuiShutdown();
    kinect->Release();
  }
}

void KinectControl::initialize()
{
  createInstance();

  // Kinectの設定を初期化する
  ERROR_CHECK( kinect->NuiInitialize( NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON ) );

  // RGBカメラを初期化する
  ERROR_CHECK( kinect->NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, CAMERA_RESOLUTION, 0, 2, 0, &imageStreamHandle ) );

  // 距離カメラを初期化する
  ERROR_CHECK( kinect->NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, CAMERA_RESOLUTION, 0, 2, 0, &depthStreamHandle ) );

  // スケルトンを初期化する
  ERROR_CHECK( kinect->NuiSkeletonTrackingEnable( 0, NUI_SKELETON_TRACKING_FLAG_SUPPRESS_NO_FRAME_DATA ) );

  // フレーム更新イベントのハンドルを作成する
  streamEvent = ::CreateEvent( 0, TRUE, FALSE, 0 );
  ERROR_CHECK( kinect->NuiSetFrameEndEvent( streamEvent, 0 ) );

  // 指定した解像度の、画面サイズを取得する
  ::NuiImageResolutionToSize(CAMERA_RESOLUTION, width, height );
}

void KinectControl::run()
{
  // メインループ
  while ( 1 ) {
    try {
      // データの更新を待つ
      DWORD ret = ::WaitForSingleObject( streamEvent, INFINITE );
      ::ResetEvent( streamEvent );

      setRgbImage( rgbImage );
      setDepthImage( depthImage );
      setSkeleton( depthImage );
      fitCloth();

      // 終了のためのキー入力チェック兼、表示のためのウェイト
      int key = cv::waitKey( 10 );
      if ( key == 'q' ) {
        break;
      }
    }
    catch( std::exception &e ) {
      std::cerr << e.what() << std::endl;
    }
  }
}

void KinectControl::createInstance()
{
  // 接続されているKinectの数を取得する
  int count = 0;
  ERROR_CHECK( ::NuiGetSensorCount( &count ) );
  if ( count == 0 ) {
    throw std::runtime_error( "Kinect を接続してください" );
  }

  // 最初のKinectのインスタンスを作成する
  ERROR_CHECK( ::NuiCreateSensorByIndex( 0, &kinect ) );

  // Kinectの状態を取得する
  HRESULT status = kinect->NuiStatus();
  if ( status != S_OK ) {
    throw std::runtime_error( "Kinect が利用可能ではありません" );
  }
}

void KinectControl::setRgbImage( cv::Mat& image )
{
  try {
    // RGBカメラのフレームデータを取得する
    NUI_IMAGE_FRAME imageFrame = { 0 };
    ERROR_CHECK( kinect->NuiImageStreamGetNextFrame( imageStreamHandle, 0, &imageFrame ) );

    // 画像データを取得する
    NUI_LOCKED_RECT colorData;
    imageFrame.pFrameTexture->LockRect( 0, &colorData, 0, 0 );

    // 画像データをコピーする
    image = cv::Mat( height, width, CV_8UC4, colorData.pBits );

    // フレームデータを解放する
    ERROR_CHECK( kinect->NuiImageStreamReleaseFrame( imageStreamHandle, &imageFrame ) );
  }
  catch ( std::exception& ex ) {
    std::cout << "KinectControl::setRgbImage" << ex.what() << std::endl;
  }
}

void KinectControl::setDepthImage( cv::Mat& image )
{
  try {
    userMask = cv::Mat( height, width, CV_8UC1, cv::Scalar( 0 ) );
    image = cv::Mat( height, width, CV_8UC1, cv::Scalar ( 0 ) );

    // 距離カメラのフレームデータを取得する
    NUI_IMAGE_FRAME depthFrame = { 0 };
    ERROR_CHECK( kinect->NuiImageStreamGetNextFrame( depthStreamHandle, 0, &depthFrame ) );

    // 距離データを取得する
    NUI_LOCKED_RECT depthData = { 0 };
    depthFrame.pFrameTexture->LockRect( 0, &depthData, 0, 0 );

    USHORT* depth = (USHORT*)depthData.pBits;
    for ( int i = 0; i < (depthData.size / sizeof(USHORT)); ++i ) {
      USHORT distance = ::NuiDepthPixelToDepth( depth[i] );
      USHORT player = ::NuiDepthPixelToPlayerIndex( depth[i] );

      LONG depthX = i % width;
      LONG depthY = i / width;
      LONG colorX = depthX;
      LONG colorY = depthY;

      // 距離カメラの座標を、RGBカメラの座標に変換する
      kinect->NuiImageGetColorPixelCoordinatesFromDepthPixelAtResolution( CAMERA_RESOLUTION, CAMERA_RESOLUTION, 0, depthX , depthY, 0, &colorX, &colorY );

      // ユーザピクセルかどうかをマスク画像に記録
      if ( player != 0 ) {
        int index = (colorY * width) + colorX;
        userMask.data[index] = 255;
      }

      // 8bitにしてデプス画像に格納
      int index = (colorY * width) + colorX;
        image.data[index] = distance / 8192.0 * 255;
    }

    // フレームデータを解放する
    ERROR_CHECK( kinect->NuiImageStreamReleaseFrame( depthStreamHandle, &depthFrame ) );
  }
  catch ( std::exception& ex ) {
    std::cout << "KinectControl::setDepthImage" << ex.what() << std::endl;
  }
}

void KinectControl::setSkeleton( cv::Mat& image )
{ 
  try {
    // スケルトンのフレームを取得する
    NUI_SKELETON_FRAME skeletonFrame = { 0 };
    ERROR_CHECK( kinect->NuiSkeletonGetNextFrame( 0, &skeletonFrame ) );

    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i ) {
      NUI_SKELETON_DATA& skeletonData = skeletonFrame.SkeletonData[i];
      if ( skeletonData.eTrackingState == NUI_SKELETON_TRACKED ) {

        // 各ジョイントごとに
        for ( int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j ) {
          if ( skeletonData.eSkeletonPositionTrackingState[j] != NUI_SKELETON_POSITION_NOT_TRACKED ) {
            setJoint( image, j, skeletonData.SkeletonPositions[j] );
          }
        }
      }
      else if ( skeletonData.eTrackingState == NUI_SKELETON_POSITION_ONLY ) {
        setJoint( image, -1, skeletonData.Position );
      }
    }
  }
  catch ( std::exception& ex ) {
    std::cout << "KinectControl::setSkeleton" << ex.what() << std::endl;
  }
}

void KinectControl::setJoint( cv::Mat& image, int joint, Vector4 position )
{
  try {
    FLOAT depthX = 0, depthY = 0;
    ::NuiTransformSkeletonToDepthImage( position, &depthX, &depthY, CAMERA_RESOLUTION );

    LONG colorX = 0;
    LONG colorY = 0;

    kinect->NuiImageGetColorPixelCoordinatesFromDepthPixelAtResolution(
      CAMERA_RESOLUTION, CAMERA_RESOLUTION,
      0, (LONG)depthX , (LONG)depthY, 0, &colorX, &colorY );

    // 左肩・右肩・左腰・右腰
    if( joint == 4 || joint == 8 || joint == 12 || joint == 16 ) {
      cv::circle( image, cv::Point( colorX, colorY ), 5, cv::Scalar( 255 ), 2 );

      if( joints.size() < 4 ) {
        joints.push_back(cv::Point( colorX, colorY ));
      }
    }
  }
  catch ( std::exception& ex ) {
    std::cout << "KinectControl::setJoint" << ex.what() << std::endl;
  }
}

void KinectControl::setCloth( cv::Mat _clothImage, std::vector<cv::Point> _points )
{
  std::cout << "setCloth" << std::endl;
  clothImage = _clothImage.clone();
  points = _points;
}

void KinectControl::fitCloth()
{
  try {
    if( joints.size() == 4 ) {
      cv::Point2f src[4];
      cv::Point2f dst[4];
      for( int i = 0; i < 4; ++i ) {
        src[i] = points.at(i);
        dst[i] = joints.at(i);
      }

      trans = cv::getPerspectiveTransform( src, dst );

      cv::Mat fitImage;
      cv::warpPerspective( clothImage, fitImage, trans, rgbImage.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar( 255, 255, 255 ) );

      for( int i = 0; i < rgbImage.rows; ++i ) {
        for( int j = 0; j < rgbImage.cols; ++j ) {
          // 服の画像の画素値
          cv::Vec4b rgba = fitImage.at<cv::Vec4b>( i, j );

          // 背景でなく、かつユーザ領域なら
          if( rgba[3] != 0 && userMask.at<UCHAR>( i, j ) == 255 ) {
            for( int k = 0; k < 3; ++k ) {
              rgbImage.at<cv::Vec4b>( i, j )[k] = rgba[k];
            }
          }
        }
      }
      cv::imshow( "RGBCamera", rgbImage );

      joints.clear();
    }
  }
  catch ( std::exception& ex ) {
    std::cout << "KinectControl::fitCloth" << ex.what() << std::endl;
  }
}