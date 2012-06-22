#include "KinectAudio.h"


#define CHECKHR( x ) { HRESULT hr = x; if (FAILED(hr)) { char buf[256]; sprintf( buf, "%d: %08X\n", __LINE__, hr ); throw std::runtime_error( buf );} }

// 初期化する
void KinectAudio::initialize( INuiSensor* sensor )
{
}

// エコーのキャンセルと、ノイズの抑制について設定する
void KinectAudio::setSystemMode( LONG mode )
{
}

// 音声入力を開始する
void KinectAudio::start()
{
}

// 音声データを取得する
std::vector< BYTE > KinectAudio::read()
{
}
