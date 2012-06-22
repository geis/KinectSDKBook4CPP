#pragma once 

#include <Windows.h>
#include <dmo.h> // IMediaObject
#include <vector>

// 音データを取得するためのバッファ。Kinect SDKのサンプルから引用および追加
class CStaticMediaBuffer : public IMediaBuffer {
public:

	CStaticMediaBuffer()
	{
	}

	STDMETHODIMP_(ULONG) AddRef()
	{
		return 2;
	}

	STDMETHODIMP_(ULONG) Release()
	{
		return 1;
	}

	STDMETHODIMP QueryInterface( REFIID riid, void **ppv )
	{
		if (riid == IID_IUnknown) {
			AddRef();
			*ppv = (IUnknown*)this;
			return NOERROR;
		}
		else if (riid == IID_IMediaBuffer) {
			AddRef();
			*ppv = (IMediaBuffer*)this;
			return NOERROR;
		}
		else {
			return E_NOINTERFACE;
		}
	}

	// 現在のデータ長を設定する(IMediaBufferの実装)
	STDMETHODIMP SetLength( DWORD ulLength ) {
		m_ulData = ulLength;
		return NOERROR;
	}

	// バッファの最大長を取得する(IMediaBufferの実装)
	STDMETHODIMP GetMaxLength(DWORD *pcbMaxLength) {
		*pcbMaxLength = buffer_.size();
		return NOERROR;
	}

	// バッファおよび現在のデータ長を取得する(IMediaBufferの実装)
	STDMETHODIMP GetBufferAndLength(BYTE **ppBuffer, DWORD *pcbLength) {
		if ( ppBuffer ) {
			*ppBuffer = &buffer_[0];
		}

		if ( pcbLength ) {
			*pcbLength = m_ulData;
		}

		return NOERROR;
	}

	// データ長をクリアする
	void Clear()
	{
		m_ulData = 0;
	}

	// データ長を取得する
	ULONG GetDataLength() const
	{
		return m_ulData;
	}

	// バッファサイズを設定する
	void SetBufferLength( ULONG length )
	{
		buffer_.resize( length );
	}

	// バッファをコピーする
	std::vector< BYTE > Clone() const
	{
		return std::vector< BYTE >( buffer_.begin(), buffer_.begin() + GetDataLength() );
	}

protected:

	std::vector< BYTE > buffer_;	// バッファ
	ULONG								m_ulData;	// データ長
};

