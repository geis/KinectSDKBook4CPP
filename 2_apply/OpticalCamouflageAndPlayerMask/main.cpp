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

  cv::Mat background;
  bool isBackground;

public:

  KinectSample()
    :isBackground( false )
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
      NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX ) );

    // RGBカメラを初期化する
    ERROR_CHECK( kinect->NuiImageStreamOpen(
      NUI_IMAGE_TYPE_COLOR, CAMERA_RESOLUTION,
      0, 2, 0, &imageStreamHandle ) );

    // 距離カメラを初期化する
    ERROR_CHECK( kinect->NuiImageStreamOpen(
      NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, CAMERA_RESOLUTION,
      0, 2, 0, &depthStreamHandle ) );

	  // フレーム更新イベントのハンドルを作成する
    streamEvent = ::CreateEvent( 0, TRUE, FALSE, 0 );
    ERROR_CHECK( kinect->NuiSetFrameEndEvent( streamEvent, 0 ) );

    // 指定した解像度の、画面サイズを取得する
    ::NuiImageResolutionToSize(CAMERA_RESOLUTION, width, height );
  }

  void run()
  {
    // メインループ
    while ( 1 ) {
      // データの更新を待つ
      DWORD ret = ::WaitForSingleObject( streamEvent, INFINITE );
      ::ResetEvent( streamEvent );

      // RGBカメラのフレームデータを取得する
      NUI_IMAGE_FRAME imageFrame = { 0 };
      ERROR_CHECK( kinect->NuiImageStreamGetNextFrame(
        imageStreamHandle, INFINITE, &imageFrame ) );

      NUI_IMAGE_FRAME depthFrame = { 0 };
      ERROR_CHECK( kinect->NuiImageStreamGetNextFrame(
        depthStreamHandle, 0, &depthFrame ) );

      // それぞれの加工を行う
      cv::Mat image1 = opticalCamouflage( imageFrame, depthFrame );
      cv::Mat image2 = playerMask( imageFrame, depthFrame );

      // 画像を表示する
      cv::imshow( "OpticalCamouflage", image1 );
      cv::imshow( "PlayerMask", image2 );

      // フレームデータを解放する
      ERROR_CHECK( kinect->NuiImageStreamReleaseFrame(
        depthStreamHandle, &depthFrame ) );
      ERROR_CHECK( kinect->NuiImageStreamReleaseFrame(
        imageStreamHandle, &imageFrame ) );

      // 終了のためのキー入力チェック兼、表示のためのウェイト
      int key = cv::waitKey( 10 );
      if ( key == 'q' ) {
        break;
      }
    }
  }

private:

  // 光学迷彩
  cv::Mat opticalCamouflage( NUI_IMAGE_FRAME& imageFrame, NUI_IMAGE_FRAME& depthFrame )
  {
    // 画像データを取得する
    NUI_LOCKED_RECT colorData;
    imageFrame.pFrameTexture->LockRect( 0, &colorData, 0, 0 );

    // 画像データをコピーする
    cv::Mat image = cv::Mat( height, width, CV_8UC4, colorData.pBits ).clone();

    // 背景を取得していない場合、背景画像にする
    if ( !isBackground ) {
      isBackground = true;
      background = image.clone();
    }

    // 画像データを解放する
    imageFrame.pFrameTexture->UnlockRect( 0 );


    // 距離データを取得する
    NUI_LOCKED_RECT depthData = { 0 };
    depthFrame.pFrameTexture->LockRect( 0, &depthData, 0, 0 );

    USHORT* depth = (USHORT*)depthData.pBits;
    for ( int i = 0; i < (depthData.size / sizeof(USHORT)); ++i ) {
      USHORT player = ::NuiDepthPixelToPlayerIndex( depth[i] );

      LONG depthX = i % width;
      LONG depthY = i / width;
      LONG colorX = depthX;
      LONG colorY = depthY;

      // 距離カメラの座標を、RGBカメラの座標に変換する
      kinect->NuiImageGetColorPixelCoordinatesFromDepthPixelAtResolution(
        CAMERA_RESOLUTION, CAMERA_RESOLUTION,
        0, depthX , depthY, depth[i], &colorX, &colorY );

      // 変換された座標を利用して、表示画像のピクセルデータを取得する
      int index = ((colorY * width) + colorX) * 4;
      UCHAR* data = &image.data[index];
      UCHAR* back = &background.data[index];

      // プレイヤーがいる部分だけ描画する
      if ( player != 0 ) {
        data[0] = back[0];
        data[1] = back[1];
        data[2] = back[2];
      }
    }

    return image;
  }

  // プレイヤーのマスク
  cv::Mat playerMask( NUI_IMAGE_FRAME& imageFrame, NUI_IMAGE_FRAME& depthFrame )
  {
    // 表示用バッファを作成する
    cv::Mat image = cv::Mat( height, width, CV_8UC4, cv::Scalar( 255, 255, 255, 0 ) );

    // 画像データを取得する
    NUI_LOCKED_RECT colorData;
    imageFrame.pFrameTexture->LockRect( 0, &colorData, 0, 0 );

    // 画像データをコピーする
    cv::Mat frame( height, width, CV_8UC4, colorData.pBits );

    // 画像データを解放する
    imageFrame.pFrameTexture->UnlockRect( 0 );


    // 距離データを取得する
    NUI_LOCKED_RECT depthData = { 0 };
    depthFrame.pFrameTexture->LockRect( 0, &depthData, 0, 0 );

    USHORT* depth = (USHORT*)depthData.pBits;
    for ( int i = 0; i < (depthData.size / sizeof(USHORT)); ++i ) {
      USHORT player = ::NuiDepthPixelToPlayerIndex( depth[i] );

      LONG depthX = i % width;
      LONG depthY = i / width;
      LONG colorX = depthX;
      LONG colorY = depthY;

      // 距離カメラの座標を、RGBカメラの座標に変換する
      kinect->NuiImageGetColorPixelCoordinatesFromDepthPixelAtResolution(
        CAMERA_RESOLUTION, CAMERA_RESOLUTION,
        0, depthX , depthY, depth[i], &colorX, &colorY );

      // 変換された座標を利用して、表示画像のピクセルデータを取得する
      int index = ((colorY * width) + colorX) * 4;
      UCHAR* data = &image.data[index];
      UCHAR* rgb = &frame.data[index];

      // プレイヤーがいる部分のみ、描画する
      if ( player != 0 ) {
          data[0] = rgb[0];
          data[1] = rgb[1];
          data[2] = rgb[2];
      }
    }

    return image;
  }

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
