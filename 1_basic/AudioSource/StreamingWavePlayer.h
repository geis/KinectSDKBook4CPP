#pragma once 

#include <Windows.h>

#include <mmsystem.h>
#pragma comment( lib, "winmm.lib" )

#include <vector>

// WAVEデータをストリーミング再生する
class StreamingWavePlayer
{
public:

	StreamingWavePlayer()
	{
	}

	virtual ~StreamingWavePlayer()
	{
		close();
	}

	// WAVE出力を開く
	void open( const WAVEFORMATEX* format, int audioBufferCount = 100 )
	{
		audioBufferCount_ = audioBufferCount;
		audioBufferNextIndex_ = 0;

		MMRESULT ret = ::waveOutOpen( &waveHandle_, WAVE_MAPPER, format, NULL, NULL, CALLBACK_NULL );
		if ( ret != MMSYSERR_NOERROR ) {
			throw std::runtime_error( "waveOutOpen failed\n" );
		}

		// 音声データ用のバッファの作成と初期化
		waveHeaders_.resize( audioBufferCount_ );
		memset( &waveHeaders_[0], 0, sizeof(WAVEHDR) * waveHeaders_.size() );

		for ( size_t i = 0; i < waveHeaders_.size(); ++i ) {
			waveHeaders_[i].lpData = new char[MAX_BUFFER_SIZE];
			waveHeaders_[i].dwUser = i;
			waveHeaders_[i].dwFlags = WHDR_DONE;
		}
	}

	// WAVE出力を開く
	void open( int sampleRate, int bitsPerSample, int channels, int audioBufferCount = 100 )
	{
		WAVEFORMATEX format;
		format.wFormatTag = WAVE_FORMAT_PCM;
		format.nChannels = channels;
		format.nSamplesPerSec = sampleRate;
		format.wBitsPerSample = bitsPerSample;
		format.nBlockAlign = format.wBitsPerSample * format.nChannels / 8;
		format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;

		open( &format, audioBufferCount );
	}

	// WAVE出力を閉じる
	void close()
	{
		for ( size_t i = 0; i < waveHeaders_.size(); ++i ) {
			delete[] waveHeaders_[i].lpData;
			waveHeaders_[i].lpData = 0;
		}

		if ( waveHandle_ != 0 ) {
			::waveOutPause( waveHandle_ );
			::waveOutClose( waveHandle_ );
			waveHandle_ = 0;
		}
	}

	// WAVEデータを出力する
	void output( const std::vector< BYTE >& buffer )
	{
		if ( buffer.size() != 0 ) {
			output( &buffer[0], buffer.size() );
		}
	}

	// WAVEデータを出力する
	void output( const void* buffer, DWORD length )
	{
		if ( length == 0 ) {
			return;
		}

		// バッファの取得
		WAVEHDR* header = &waveHeaders_[audioBufferNextIndex_];
		if ( (header->dwFlags & WHDR_DONE) == 0 ) {
			return;
		}

		// WAVEヘッダのクリーンアップ
		MMRESULT ret = ::waveOutUnprepareHeader( waveHandle_, header, sizeof(WAVEHDR) );
		if ( ret != MMSYSERR_NOERROR ) {
			return;
		}

		// WAVEデータの取得
		header->dwBufferLength = length;
		header->dwFlags = 0;
		memcpy( header->lpData, buffer, header->dwBufferLength );

		// WAVEヘッダの初期化
		ret = ::waveOutPrepareHeader( waveHandle_, header, sizeof(WAVEHDR) );
		if ( ret != MMSYSERR_NOERROR ) {
			return;
		}

		// WAVEデータを出力キューに入れる
		ret = ::waveOutWrite( waveHandle_, header, sizeof(WAVEHDR) );
		if ( ret != MMSYSERR_NOERROR ) {
			return;
		}

		// 次のバッファインデックス
		audioBufferNextIndex_ = (audioBufferNextIndex_ + 1) % audioBufferCount_;
	}

private:

	// コピーを禁止する
	StreamingWavePlayer( const StreamingWavePlayer& rhs );
	StreamingWavePlayer& operator = ( const StreamingWavePlayer& rhs );

private:

	static const int MAX_BUFFER_SIZE = 2 * 1024 * 1024;

	HWAVEOUT							waveHandle_;
	std::vector<WAVEHDR>  waveHeaders_;

	int audioBufferCount_;
	int audioBufferNextIndex_;
};
