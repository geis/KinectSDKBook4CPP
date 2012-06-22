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

  // フレーム更新イベントのハンドルを作成する
  streamEvent = ::CreateEvent( 0, TRUE, FALSE, 0 );
  ERROR_CHECK( kinect->NuiSetFrameEndEvent( streamEvent, 0 ) );

  // 指定した解像度の、画面サイズを取得する
  ::NuiImageResolutionToSize(CAMERA_RESOLUTION, width, height );

  // PointCloudビューワを初期化
  viewer = new pcl::visualization::CloudViewer("Kinect Point Cloud");
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

void KinectControl::run()
{
  // メインループ
  while ( 1 ) {
    // データの更新を待つ
    DWORD ret = ::WaitForSingleObject( streamEvent, INFINITE );
    ::ResetEvent( streamEvent );

    setRgbImage( rgbImage );
    setDepthImage( depthImage );

    // 画像を表示する
    cv::imshow( "RGBCamera", rgbImage );
    cv::imshow( "DepthCamera", depthImage );

    // ポイントクラウドを表示する
    viewer->showCloud(cloud);

    // 終了のためのキー入力チェック兼、表示のためのウェイト
    int key = cv::waitKey( 10 );
    if ( key == 'q' ) {
      break;
    }
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
    std::cout << ex.what() << std::endl;
  }
}

void KinectControl::setDepthImage( cv::Mat& image )
{
  try {
    // ポイントクラウド準備
    pcl::PointCloud<pcl::PointXYZRGBA>::Ptr points(new pcl::PointCloud<pcl::PointXYZRGBA>);
    points->width = width;
    points->height = height;

    // 距離画像準備
    image = cv::Mat( height, width, CV_8UC1, cv::Scalar( 0 ) );

    // 距離カメラのフレームデータを取得する
    NUI_IMAGE_FRAME depthFrame = { 0 };
    ERROR_CHECK( kinect->NuiImageStreamGetNextFrame( depthStreamHandle, 0, &depthFrame ) );

    // 距離データを取得する
    NUI_LOCKED_RECT depthData = { 0 };
    depthFrame.pFrameTexture->LockRect( 0, &depthData, 0, 0 );

    USHORT* depth = (USHORT*)depthData.pBits;
    for ( int i = 0; i < (depthData.size / sizeof(USHORT)); ++i ) {
      USHORT distance = ::NuiDepthPixelToDepth( depth[i] );

      LONG depthX = i % width;
      LONG depthY = i / width;
      LONG colorX = depthX;
      LONG colorY = depthY;

      // 距離カメラの座標を、RGBカメラの座標に変換する
      kinect->NuiImageGetColorPixelCoordinatesFromDepthPixelAtResolution( CAMERA_RESOLUTION, CAMERA_RESOLUTION, 0, depthX , depthY, 0, &colorX, &colorY );

      // 距離画像作成
      image.at<UCHAR>( colorY, colorX ) = distance / 8192.0 * 255.0;

      // ポイントクラウド
      Vector4 real = NuiTransformDepthImageToSkeleton(depthX, depthY, distance, CAMERA_RESOLUTION);
      pcl::PointXYZRGBA point;
      point.x = real.x;
      point.y = -real.y;
      point.z = real.z;

      // テクスチャ
      cv::Vec4b color = rgbImage.at<cv::Vec4b>(colorY, colorX);
      point.r = color[2];
      point.g = color[1];
      point.b = color[0];
      points->push_back(point);
    }

    // ポイントクラウドをコピー
    cloud = points;

    // フレームデータを解放する
    ERROR_CHECK( kinect->NuiImageStreamReleaseFrame( depthStreamHandle, &depthFrame ) );
  }
  catch ( std::exception& ex ) {
    std::cout << ex.what() << std::endl;
  }
}
