#include "KinectAudio.h"


#define CHECKHR( x ) { HRESULT hr = x; if (FAILED(hr)) { char buf[256]; sprintf( buf, "%d: %08X\n", __LINE__, hr ); throw std::runtime_error( buf );} }

// 初期化する
void KinectAudio::initialize( INuiSensor* sensor )
{
	CHECKHR( sensor->NuiGetAudioSource( &soundSource_ ) );
	CHECKHR( soundSource_.QueryInterface( &mediaObject_ ) );
	CHECKHR( soundSource_.QueryInterface( &propertyStore_ ) );
}

// 音声入力を開始する
void KinectAudio::start()
{
	memset( &outputBufferStruct_, 0, sizeof(outputBufferStruct_) );
	outputBufferStruct_.pBuffer = &mediaBuffer_;

	// Set DMO output format
	DMO_MEDIA_TYPE mt = {0};
	CHECKHR( ::MoInitMediaType( &mt, sizeof(WAVEFORMATEX) ) );

	mt.majortype = MEDIATYPE_Audio;
	mt.subtype = MEDIASUBTYPE_PCM;
	mt.lSampleSize = 0;
	mt.bFixedSizeSamples = TRUE;
	mt.bTemporalCompression = FALSE;
	mt.formattype = FORMAT_WaveFormatEx;
	memcpy( mt.pbFormat, &getWaveFormat(), sizeof(WAVEFORMATEX) );

	CHECKHR( mediaObject_->SetOutputType( 0, &mt, 0 ) );
	::MoFreeMediaType( &mt );

	// Allocate streaming resources. This step is optional. If it is not called here, it
	// will be called when first time ProcessInput() is called. However, if you want to 
	// get the actual frame size being used, it should be called explicitly here.
	CHECKHR( mediaObject_->AllocateStreamingResources() );

	// allocate output buffer
	mediaBuffer_.SetBufferLength( getWaveFormat().nSamplesPerSec * getWaveFormat().nBlockAlign );
}

// 音声データを取得する
std::vector< BYTE > KinectAudio::read()
{
	mediaBuffer_.Clear();

	do{
		// 音声データを取得する
		DWORD dwStatus;
		CHECKHR( mediaObject_->ProcessOutput(0, 1, &outputBufferStruct_, &dwStatus) );
	} while ( outputBufferStruct_.dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE );

	return mediaBuffer_.Clone();
}
