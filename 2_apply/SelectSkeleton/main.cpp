#include <iostream>
#include <sstream>

// NuiApi.hの前にWindows.hをインクルードする
#include <Windows.h>
#include <NuiApi.h>

#include <opencv2/opencv.hpp>



#define ERROR_CHECK( ret )  \
  if ( ret != S_OK ) {    \
    std::stringstream ss;	\
    ss << "failed " #ret " " << std::hex << ret << std::endl;			\
    throw std::runtime_error( ss.str().c_str() );			\
  }

const NUI_IMAGE_RESOLUTION CAMERA_RESOLUTION = NUI_IMAGE_RESOLUTION_640x480;

class KinectSample
{
private:

  INuiSensor* kinect;
  HANDLE imageStreamHandle;
  HANDLE depthStreamHandle;
  HANDLE streamEvent;

  DWORD width;
  DWORD height;

public:

  KinectSample()
    : activeTrackId( 0 )
  {
  }

  ~KinectSample()
  {
    // 終了処理
    if ( kinect != 0 ) {
      kinect->NuiShutdown();
      kinect->Release();
    }
  }

  void initialize()
  {
    createInstance();

    // Kinectの設定を初期化する
    ERROR_CHECK( kinect->NuiInitialize(
      NUI_INITIALIZE_FLAG_USES_COLOR |
      NUI_INITIALIZE_FLAG_USES_SKELETON ) );

    // RGBカメラを初期化する
    ERROR_CHECK( kinect->NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR,
      CAMERA_RESOLUTION, 0, 2, 0, &imageStreamHandle ) );

    // スケルトンを初期化する
    ERROR_CHECK( kinect->NuiSkeletonTrackingEnable(
      0, NUI_SKELETON_TRACKING_FLAG_TITLE_SETS_TRACKED_SKELETONS ) );

    // フレーム更新イベントのハンドルを作成する
    streamEvent = ::CreateEvent( 0, TRUE, FALSE, 0 );
    ERROR_CHECK( kinect->NuiSetFrameEndEvent( streamEvent, 0 ) );

    // 指定した解像度の、画面サイズを取得する
    ::NuiImageResolutionToSize(CAMERA_RESOLUTION, width, height );
  }

  void run()
  {
    cv::Mat image;

    // メインループ
    while ( 1 ) {
      // データの更新を待つ
      DWORD ret = ::WaitForSingleObject( streamEvent, INFINITE );
      ::ResetEvent( streamEvent );

      drawRgbImage( image );
      drawSkeleton( image );

      // 画像を表示する
      cv::imshow( "KinectSample", image );

      // 終了のためのキー入力チェック兼、表示のためのウェイト
      int key = cv::waitKey( 10 );
      if ( key == 'q' ) {
        break;
      }
    }
  }

private:

  void createInstance()
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

  void drawRgbImage( cv::Mat& image )
  {
      // RGBカメラのフレームデータを取得する
      NUI_IMAGE_FRAME imageFrame = { 0 };
      ERROR_CHECK( kinect->NuiImageStreamGetNextFrame(
        imageStreamHandle, INFINITE, &imageFrame ) );

      // 画像データを取得する
      NUI_LOCKED_RECT colorData;
      imageFrame.pFrameTexture->LockRect( 0, &colorData, 0, 0 );

      // 画像データをコピーする
      image = cv::Mat( height, width, CV_8UC4, colorData.pBits );

      // フレームデータを解放する
      ERROR_CHECK( kinect->NuiImageStreamReleaseFrame(
        imageStreamHandle, &imageFrame ) );
  }

  void drawSkeleton( cv::Mat& image )
  {
    // スケルトンのフレームを取得する
    NUI_SKELETON_FRAME skeletonFrame = { 0 };
    kinect->NuiSkeletonGetNextFrame( 0, &skeletonFrame );

    selectActiveSkeleton( skeletonFrame );

    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i ) {
      NUI_SKELETON_DATA& skeletonData = skeletonFrame.SkeletonData[i];
      if ( skeletonData.eTrackingState == NUI_SKELETON_TRACKED ) {
        for ( int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j ) {
          if ( skeletonData.eSkeletonPositionTrackingState[j] 
            != NUI_SKELETON_POSITION_NOT_TRACKED ) {
              drawJoint( image, skeletonData.SkeletonPositions[j] );
          }
        }
      }
      else if ( skeletonData.eTrackingState == NUI_SKELETON_POSITION_ONLY ) {
        drawJoint( image, skeletonData.Position );
      }
    }
  }

  void drawJoint( cv::Mat& image, Vector4 position )
  {
    FLOAT depthX = 0, depthY = 0;
    ::NuiTransformSkeletonToDepthImage(
      position, &depthX, &depthY, CAMERA_RESOLUTION );

    LONG colorX = 0;
    LONG colorY = 0;

    kinect->NuiImageGetColorPixelCoordinatesFromDepthPixelAtResolution(
      CAMERA_RESOLUTION, CAMERA_RESOLUTION,
      0, (LONG)depthX , (LONG)depthY, 0, &colorX, &colorY );

    cv::circle( image, cv::Point( colorX, colorY ), 10, cv::Scalar( 0, 255, 0 ), 5 );
  }

  DWORD activeTrackId;
  void selectActiveSkeleton( NUI_SKELETON_FRAME& skeletonFrame )
  {
    const int center = width / 2;
    FLOAT minpos = 0;
    DWORD trackedId = 0;

    // 一番中心に近いプレイヤーを探す
    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i ) {
      NUI_SKELETON_DATA& skeletonData = skeletonFrame.SkeletonData[i];
      if ( skeletonData.eTrackingState != NUI_SKELETON_NOT_TRACKED ) {
        FLOAT depthX = 0, depthY = 0;
        ::NuiTransformSkeletonToDepthImage(
          skeletonData.Position, &depthX, &depthY, CAMERA_RESOLUTION );

        if ( abs(minpos - center) > abs(depthX - center) ) {
          minpos = depthX;
          trackedId = skeletonData.dwTrackingID;
        }
      }
    }

    // 現在追跡しているプレイヤーでなければ、中心に近いプレイヤーをアクティブにする
    if ( (trackedId != 0) || (trackedId != activeTrackId) ) {
      DWORD trackedIds[] = { trackedId, 0 };
      kinect->NuiSkeletonSetTrackedSkeletons( trackedIds );
      activeTrackId = trackedId;
    }
  }
};

void main()
{
  try {
    KinectSample kinect;
    kinect.initialize();
    kinect.run();
  }
  catch ( std::exception& ex ) {
    std::cout << ex.what() << std::endl;
  }
}
