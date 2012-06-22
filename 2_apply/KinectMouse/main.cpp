#include <iostream>
#include <sstream>

// NuiApi.hの前にWindows.hをインクルードする
#include <Windows.h>
#include <NuiApi.h>

#include <opencv2/opencv.hpp>

#define ERROR_CHECK( ret )  \
  if ( ret != S_OK ) {      \
    std::stringstream ss;	  \
    ss << "failed " #ret " " << std::hex << ret << std::endl;			\
    throw std::runtime_error( ss.str().c_str() );			\
  }

const NUI_IMAGE_RESOLUTION CAMERA_RESOLUTION = NUI_IMAGE_RESOLUTION_640x480;

// マウス入力を送信する
class SendInput
{
public:

  SendInput()
  {
  }

  // マウスカーソルを動かす
  static void MouseMove( int x, int y, SIZE screen )
  {
    // X,Y座標を0-65535の範囲に変換する
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dx = (LONG)((x * 65535) / screen.cx);
    input.mi.dy = (LONG)((y * 65535) / screen.cy);
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

    ::SendInput( 1, &input, sizeof(input) );
  }

  // 左クリック
  static void LeftClick()
  {
    // マウスの左ボタンをDown->Upする
    INPUT input[] = {
      { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_LEFTDOWN },
      { INPUT_MOUSE, 0, 0, 0, MOUSEEVENTF_LEFTUP },
    };

    ::SendInput( sizeof(input) / sizeof(input[0]), input, sizeof(input[0]) );
  }
};

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
    ERROR_CHECK( kinect->NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, CAMERA_RESOLUTION,
      0, 2, 0, &imageStreamHandle ) );

    // スケルトンを初期化する
    ERROR_CHECK( kinect->NuiSkeletonTrackingEnable(
      0, NUI_SKELETON_TRACKING_FLAG_SUPPRESS_NO_FRAME_DATA ) );

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
      skeletonMouse();

      // 画像を表示する
      cv::imshow( "RGBCamera", image );

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
      ERROR_CHECK( kinect->NuiImageStreamReleaseFrame( imageStreamHandle, &imageFrame ) );
  }

  // スケルトンを使用してマウス操作を行う
  void skeletonMouse()
  {
    // スケルトンのフレームを取得する
    NUI_SKELETON_FRAME skeletonFrame = { 0 };
    kinect->NuiSkeletonGetNextFrame( 0, &skeletonFrame );

    // トラッキングしている最初のスケルトンを探す
    NUI_SKELETON_DATA* skeletonData = 0;
    for ( int i = 0; i < NUI_SKELETON_COUNT; ++i ) {
      NUI_SKELETON_DATA& data = skeletonFrame.SkeletonData[i];
      if ( data.eTrackingState == NUI_SKELETON_TRACKED ) {
        skeletonData = &data;
        break;
      }
    }
    // 追跡しているスケルトンがなければ終了
    if ( skeletonData == 0 ) {
      return;
    }

    // 右手が追跡されていなければ終了
    if ( skeletonData->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_RIGHT]
      == NUI_SKELETON_POSITION_NOT_TRACKED ) {
        return;
    }



    // 右手の座標を取得する
    Vector4 rh = skeletonData->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];

    // 右手の座標を、Depthの座標(2次元)に変換する
    FLOAT depthX = 0, depthY = 0;
    ::NuiTransformSkeletonToDepthImage( rh, &depthX, &depthY, CAMERA_RESOLUTION );

    // スクリーン座標を取得する
    SIZE screen = {
      ::GetSystemMetrics( SM_CXVIRTUALSCREEN ),
      ::GetSystemMetrics( SM_CYVIRTUALSCREEN )
    };

    // 右手の座標を、スクリーン座標に変換する
    int x = (depthX * screen.cx) / width;
    int y = (depthY * screen.cy) / height;

    // マウスを動かす
    SendInput::MouseMove( x, y, screen );

    // クリックを認識した場合に、右クリックを行う
    if ( isClicked( FramePoint( skeletonFrame.liTimeStamp.QuadPart, depthX, depthY ) ) ) {
      SendInput::LeftClick();
    }
  }

  // 基準となるフレーム
  struct FramePoint
  {
    LARGE_INTEGER TimeStamp;  // タイムスタンプ
    FLOAT X;                  // X座標
    FLOAT Y;                  // Y座標

    FramePoint( LONGLONG timeStamp = 0, FLOAT x = 0.0f, FLOAT y = 0.0f )
      : X( x )
      , Y( y )
    {
      TimeStamp.QuadPart = timeStamp;
    }
  };

  FramePoint  basePoint;

  static const int milliseconds = 2000;        // 認識するまでの停止時間の設定
  static const int threshold = 10;             // 座標の変化量の閾値

  // クリックされたか判定する
  bool isClicked( FramePoint& currentPoint )
  {
    // milliseconds時間経過したら steady
    if ( (currentPoint.TimeStamp.QuadPart - basePoint.TimeStamp.QuadPart) > milliseconds ) {
      basePoint = currentPoint;
      return true;
    }

    // 座標の変化量がthreshold以上ならば、basePointを更新して初めから計測
    if ( abs( currentPoint.X - basePoint.X ) > threshold ||
         abs( currentPoint.Y - basePoint.Y ) > threshold ) {
          // 座標が動いたので基点を動いた位置にずらして、最初から計測
          basePoint = currentPoint;
    }
    
    return false;
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
