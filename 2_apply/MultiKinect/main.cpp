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

  HANDLE imageStreamEvent[3];
  bool hasSkeletonEngine;

  DWORD width;
  DWORD height;

public:

  KinectSample()
    : kinect( 0 )
    , hasSkeletonEngine( false )
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

  void initialize( int id = 0 )
  {
    createInstance( id );

    // Kinectの設定を初期化する
    HRESULT ret = kinect->NuiInitialize(
      NUI_INITIALIZE_FLAG_USES_COLOR |
      NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
      NUI_INITIALIZE_FLAG_USES_SKELETON );
    if ( ret != S_OK ) {
      // 一つのプロセスで、複数のKinectからスケルトンを取得することができない
      // RBBおよび距離カメラのみ利用可能(プレイヤーもスケルトンエンジンを使用しているため不可)
      if ( ret == E_NUI_SKELETAL_ENGINE_BUSY ) {
        ERROR_CHECK( kinect->NuiInitialize(
          NUI_INITIALIZE_FLAG_USES_COLOR |
          NUI_INITIALIZE_FLAG_USES_DEPTH ) );
      }
      else {
        ERROR_CHECK( ret );
      }
    }

    // 待機イベントの作成
    imageStreamEvent[0] = ::CreateEvent( 0, TRUE, FALSE, 0 );
    imageStreamEvent[1] = ::CreateEvent( 0, TRUE, FALSE, 0 );
    imageStreamEvent[2] = ::CreateEvent( 0, TRUE, FALSE, 0 );

    // RGBカメラを初期化する
    ERROR_CHECK( kinect->NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, CAMERA_RESOLUTION,
      0, 2, imageStreamEvent[0], &imageStreamHandle ) );

    // スケルトンエンジンの所持状態を取得する
    hasSkeletonEngine = ::HasSkeletalEngine( kinect );

    // スケルトンエンジンが利用できる場合、距離カメラおよびプレイヤー、スケルトンを有効にする
    if ( hasSkeletonEngine ) {
      // 距離カメラを初期化する
      ERROR_CHECK( kinect->NuiImageStreamOpen(
        NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, CAMERA_RESOLUTION,
        0, 2, imageStreamEvent[1], &depthStreamHandle ) );

      // スケルトンを初期化する
      ERROR_CHECK( kinect->NuiSkeletonTrackingEnable(
        imageStreamEvent[2], NUI_SKELETON_TRACKING_FLAG_SUPPRESS_NO_FRAME_DATA ) );
    }
    // スケルトンエンジンが利用できない場合、距離カメラのみ有効にする
    else {
      // 距離カメラを初期化する
      imageStreamEvent[1] = ::CreateEvent( 0, TRUE, FALSE, 0 );
      ERROR_CHECK( kinect->NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH, CAMERA_RESOLUTION,
        0, 2,  imageStreamEvent[1], &depthStreamHandle ) );
    }

    // 指定した解像度の、画面サイズを取得する
    ::NuiImageResolutionToSize(CAMERA_RESOLUTION, width, height );
  }

  void run()
  {
    cv::Mat image;

    std::stringstream ss;
    ss << "KinectSample:" << kinect->NuiInstanceIndex();

    // メインループ
    while ( 1 ) {
      try {
        // データの更新を待つ
        //	スケルトンエンジンがあれば、待機するイベントはRGB、距離、スケルトンの3つ
        //	スケルトンエンジンがなければ、待機するイベントはRGB、距離2つ
        ::WaitForMultipleObjects( (hasSkeletonEngine ? 3 : 2),
          imageStreamEvent, TRUE, INFINITE );

        drawRgbImage( image );
        drawDepthImage( image );

        // スケルトンエンジンが利用可能であれば、スケルトンを表示する
        if ( hasSkeletonEngine ) {
          drawSkeleton( image );
        }

        // 画像を表示する
        cv::imshow( ss.str(), image );
      }
      catch ( std::exception& ex ) {
        std::cout << ex.what() << std::endl;
      }

      // 終了のためのキー入力チェック兼、表示のためのウェイト
      int key = cv::waitKey( 10 );
      if ( key == 'q' ) {
        break;
      }
    }
  }

private:

  // Kinectの状態が変わった時に呼ばれるコールバック関数(クラス関数)
  static void CALLBACK StatusChanged( HRESULT hrStatus,
    const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName, void* pUserData )
  {
  }

  void createInstance( int id = 0 )
  {
    // V1.0 ではNuiInitializeに既知の問題があるため、同一プロセスで複数Kinectを使用する場合は
    // NuiInitializeの前にNuiSetDeviceStatusCallback()を呼び出す必要がある。
    // http://blogs.msdn.com/b/windows_multimedia_jp/archive/2012/02/28/microsoft-kinect-for-windows-sdk-v1-0.aspx
    ::NuiSetDeviceStatusCallback( StatusChanged, 0 );

    // 接続されているKinectの数を取得する
    int count = 0;
    ERROR_CHECK( ::NuiGetSensorCount( &count ) );
    if ( count == 0 ) {
      throw std::runtime_error( "Kinect を接続してください" );
    }

    // 最初のKinectのインスタンスを作成する
    ERROR_CHECK( ::NuiCreateSensorByIndex( id, &kinect ) );

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

  void drawDepthImage( cv::Mat& image )
  {
    // 距離カメラのフレームデータを取得する
    NUI_IMAGE_FRAME depthFrame = { 0 };
    ERROR_CHECK( kinect->NuiImageStreamGetNextFrame(
      depthStreamHandle, INFINITE, &depthFrame ) );

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
      //kinect->NuiImageGetColorPixelCoordinatesFromDepthPixelAtResolution(
      //  CAMERA_RESOLUTION, CAMERA_RESOLUTION,
      //  0, depthX , depthY, 0, &colorX, &colorY );

      // プレイヤー
      if ( player != 0 ) {
        int index = ((colorY * width) + colorX) * 4;
        UCHAR* data = &image.data[index];
        data[0] = 0;
        data[1] = 0;
        data[2] = 255;
      }
      else {
        // 一定以上の距離を描画しない
        if ( distance >= 1000 ) {
          int index = ((colorY * width) + colorX) * 4;
          UCHAR* data = &image.data[index];
          data[0] = 255;
          data[1] = 255;
          data[2] = 255;
        }
      }
    }

    // フレームデータを解放する
    ERROR_CHECK( kinect->NuiImageStreamReleaseFrame( depthStreamHandle, &depthFrame ) );
  }

  void drawSkeleton( cv::Mat& image )
  {
    // スケルトンのフレームを取得する
    NUI_SKELETON_FRAME skeletonFrame = { 0 };
    HRESULT ret = kinect->NuiSkeletonGetNextFrame( 0, &skeletonFrame );
    if ( ret != S_OK ) {
      return;
    }

    // スケルトンを表示する
    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i ) {
      NUI_SKELETON_DATA& skeletonData = skeletonFrame.SkeletonData[i];
      // スケルトンが追跡されている状態
      if ( skeletonData.eTrackingState == NUI_SKELETON_TRACKED ) {
        for ( int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j ) {
          if ( skeletonData.eSkeletonPositionTrackingState[j] 
            != NUI_SKELETON_POSITION_NOT_TRACKED ) {
              drawJoint( image, skeletonData.SkeletonPositions[j] );
          }
        }
      }
      // スケルトンの位置のみ追跡している状態
      // Nearモードの全プレイヤーおよび、Defaultモードのスケルトン追跡されているプレイヤー以外
      else if ( skeletonData.eTrackingState == NUI_SKELETON_POSITION_ONLY ) {
        drawJoint( image, skeletonData.Position );
      }
    }
  }

  void drawJoint( cv::Mat& image, Vector4 position )
  {
    // スケルトンの座標を、距離カメラの座標に変換する
    FLOAT depthX = 0, depthY = 0;
    ::NuiTransformSkeletonToDepthImage( position, &depthX, &depthY, CAMERA_RESOLUTION );

    LONG colorX = 0;
    LONG colorY = 0;

    // 距離カメラの座標を、RGBカメラの座標に変換する
    kinect->NuiImageGetColorPixelCoordinatesFromDepthPixelAtResolution(
      CAMERA_RESOLUTION, CAMERA_RESOLUTION,
      0, (LONG)depthX , (LONG)depthY, 0, &colorX, &colorY );

    cv::circle( image, cv::Point( colorX, colorY ), 10, cv::Scalar( 0, 255, 0 ), 5 );
  }
};

DWORD WINAPI ThreadEntry( LPVOID lpThreadParameter )
{
  KinectSample* kinect = (KinectSample*)lpThreadParameter;
  kinect->run();

  return 0;
}

void main()
{
  try {
    int count = 0;
    ERROR_CHECK( ::NuiGetSensorCount( &count ) );
    if ( count == 0 ) {
      throw std::runtime_error( "Kinect を接続してください" );
    }

    std::vector< KinectSample > kinects( count );
    std::vector< HANDLE > hThread( count );

    for ( int i = 0; i < kinects.size(); ++i )  {
      DWORD id = 0;
      kinects[i].initialize( i );
      hThread[i] = ::CreateThread( 0, 0, ThreadEntry, &kinects[i], 0, &id );
    }

    ::WaitForMultipleObjects( hThread.size(), &hThread[0], TRUE, INFINITE );
  }
  catch ( std::exception& ex ) {
    std::cout << ex.what() << std::endl;
  }
}
