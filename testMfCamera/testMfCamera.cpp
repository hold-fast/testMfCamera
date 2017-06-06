// testMfCamera.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <strsafe.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wmcodecdsp.h>

#define CHECK_HR(hr, msg) if (hr != S_OK) { printf(msg); printf("Error: %.2X.\n", hr); goto done; }

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

template <class T> inline void SafeRelease(T*& pT)
{
	if (pT != NULL)
	{
		pT->Release();
		pT = NULL;
	}
}



LPCWSTR GetGUIDNameConst(const GUID& guid);
HRESULT GetGUIDName(const GUID& guid, WCHAR **ppwsz);
HRESULT LogAttributeValueByIndex(IMFAttributes *pAttr, DWORD index);
HRESULT SpecialCaseAttributeValue(GUID guid, const PROPVARIANT& var);
void DBGMSG(PCWSTR format, ...);
HRESULT LogMediaType(IMFMediaType *pType);
HRESULT EnumerateCaptureFormats(IMFMediaSource *pSource);


class MFCamera {
public:
	MFCamera() {}
	virtual ~MFCamera() {}


public:
	bool init()
	{
		IMFAttributes *pConfig = NULL;
		IMFActivate **ppDevices = NULL;
		IUnknown *spTransformUnk = NULL;
		UINT32 count = 0;
		DWORD mftStatus = 0;

		// Create an attribute store to hold the search criteria.
		CHECK_HR(MFCreateAttributes(&pConfig, 1), "MFCreateAttributes error\n");

		// Request video capture devices.
		CHECK_HR(pConfig->SetGUID(
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
		), "pConfig->SetGUID error");

		// Enumerate the devices
		CHECK_HR(MFEnumDeviceSources(pConfig, &ppDevices, &count), "MFEnumDeviceSources error\n");

		// Create a media source for the first device in the list.
		CHECK_HR(ppDevices[WEBCAM_DEVICE_INDEX]->ActivateObject(IID_PPV_ARGS(&m_pSource)), "ppDevices[0]->ActivateObject error\n");

		// Create a source reader.
		CHECK_HR(MFCreateSourceReaderFromMediaSource(
			m_pSource,
			pConfig,
			&m_pSourceReader), "MFCreateSourceReaderFromMediaSource error \n");

		//EnumerateCaptureFormats(pSource);

		MFCreateMediaType(&m_pSrcOutMediaType);
		m_pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		m_pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
		MFSetAttributeSize(m_pSrcOutMediaType, MF_MT_FRAME_SIZE, CAMERA_RESOLUTION_WIDTH, CAMERA_RESOLUTION_HEIGHT);
		CHECK_HR(MFSetAttributeRatio(m_pSrcOutMediaType, MF_MT_FRAME_RATE, TARGET_FRAME_RATE, 1), "Failed to set frame rate on video device out type.\n");

		CHECK_HR(m_pSourceReader->SetCurrentMediaType(0, NULL, m_pSrcOutMediaType), "Failed to set media type on source reader.\n");


		CHECK_HR(CoCreateInstance(CLSID_CMSH264EncoderMFT, NULL, CLSCTX_INPROC_SERVER,
			IID_IUnknown, (void**)&spTransformUnk), "Failed to create H264 encoder MFT.\n");

		CHECK_HR(spTransformUnk->QueryInterface(IID_PPV_ARGS(&m_pTransform)), "Failed to get IMFTransform interface from H264 encoder MFT object.\n");


		MFCreateMediaType(&m_pMFTOutputMediaType);
		m_pMFTOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		m_pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
		CHECK_HR(m_pMFTOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, TARGET_AVERAGE_BIT_RATE), "Failed to set average bit rate on H264 output media type.\n");
		CHECK_HR(MFSetAttributeSize(m_pMFTOutputMediaType, MF_MT_FRAME_SIZE, CAMERA_RESOLUTION_WIDTH, CAMERA_RESOLUTION_HEIGHT), "Failed to set frame size on H264 MFT out type.\n");
		CHECK_HR(MFSetAttributeRatio(m_pMFTOutputMediaType, MF_MT_FRAME_RATE, TARGET_FRAME_RATE, 1), "Failed to set frame rate on H264 MFT out type.\n");
		CHECK_HR(MFSetAttributeRatio(m_pMFTOutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.\n");
		m_pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);	// 2 = Progressive scan, i.e. non-interlaced.
		m_pMFTOutputMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

		CHECK_HR(m_pTransform->SetOutputType(0, m_pMFTOutputMediaType, 0), "Failed to set output media type on H.264 encoder MFT.\n");

		MFCreateMediaType(&m_pMFTInputMediaType);
		m_pMFTInputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		m_pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
		CHECK_HR(MFSetAttributeSize(m_pMFTInputMediaType, MF_MT_FRAME_SIZE, CAMERA_RESOLUTION_WIDTH, CAMERA_RESOLUTION_HEIGHT), "Failed to set frame size on H264 MFT out type.\n");
		CHECK_HR(MFSetAttributeRatio(m_pMFTInputMediaType, MF_MT_FRAME_RATE, TARGET_FRAME_RATE, 1), "Failed to set frame rate on H264 MFT out type.\n");
		CHECK_HR(MFSetAttributeRatio(m_pMFTInputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.\n");
		m_pMFTInputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);

		CHECK_HR(m_pTransform->SetInputType(0, m_pMFTInputMediaType, 0), "Failed to set input media type on H.264 encoder MFT.\n");

		CHECK_HR(m_pTransform->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 MFT.\n");
		if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
			printf("E: ApplyTransform() pTransform->GetInputStatus() not accept data.\n");
			goto done;
		}


		CHECK_HR(m_pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 MFT.\n");
		CHECK_HR(m_pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 MFT.\n");
		CHECK_HR(m_pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 MFT.\n");


		SafeRelease(pConfig);
		for (DWORD i = 0; i < count; i++)
		{
			SafeRelease(&ppDevices[i]);
		}
		CoTaskMemFree(ppDevices);
		SafeRelease(spTransformUnk);
		m_isInit = true;
		return true;

	done:
		SafeRelease(pConfig);
		for (DWORD i = 0; i < count; i++)
		{
			SafeRelease(&ppDevices[i]);
		}
		CoTaskMemFree(ppDevices);
		SafeRelease(spTransformUnk);
		m_isInit = false;
		return false;
	}

	bool readFrame(BYTE** buff, DWORD& len)
	{
		
		IMFSample *videoSample = NULL;
		MFT_OUTPUT_STREAM_INFO StreamInfo;
		IMFSample *mftOutSample = NULL;
		IMFMediaBuffer *pBuffer = NULL;
		LONGLONG llVideoTimeStamp, llSampleDuration;
		DWORD streamIndex, flags;
		MFT_OUTPUT_DATA_BUFFER _outputDataBuffer;
		HRESULT mftProcessOutput = S_OK;
		DWORD processOutputStatus = 0;
		DWORD mftOutFlags;
		bool frameSent = false;



		CHECK_HR(m_pSourceReader->ReadSample(
			MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			0,                              // Flags.
			&streamIndex,                   // Receives the actual stream index. 
			&flags,                         // Receives status flags.
			&llVideoTimeStamp,              // Receives the time stamp.
			&videoSample                    // Receives the sample or NULL.
		), "Error reading video sample.");

		if (videoSample)
		{

			CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.\n");
			CHECK_HR(videoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.\n");

			HRESULT hr = m_pTransform->ProcessInput(0, videoSample, 0);
			CHECK_HR(hr, "The resampler H264 ProcessInput call failed.\n");

			CHECK_HR(m_pTransform->GetOutputStatus(&mftOutFlags), "H264 MFT GetOutputStatus failed.\n");

			if (mftOutFlags == MFT_OUTPUT_STATUS_SAMPLE_READY)
			{
				printf("Sample ready.\n");

				CHECK_HR(m_pTransform->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from H264 MFT.\n");

				CHECK_HR(MFCreateSample(&mftOutSample), "Failed to create MF sample.\n");
				CHECK_HR(MFCreateMemoryBuffer(StreamInfo.cbSize, &pBuffer), "Failed to create memory buffer.\n");
				CHECK_HR(mftOutSample->AddBuffer(pBuffer), "Failed to add sample to buffer.\n");

				while (true)
				{
					_outputDataBuffer.dwStreamID = 0;
					_outputDataBuffer.dwStatus = 0;
					_outputDataBuffer.pEvents = NULL;
					_outputDataBuffer.pSample = mftOutSample;

					mftProcessOutput = m_pTransform->ProcessOutput(0, 1, &_outputDataBuffer, &processOutputStatus);

					if (mftProcessOutput != MF_E_TRANSFORM_NEED_MORE_INPUT)
					{
						CHECK_HR(_outputDataBuffer.pSample->SetSampleTime(llVideoTimeStamp), "Error setting MFT sample time.\n");
						CHECK_HR(_outputDataBuffer.pSample->SetSampleDuration(llSampleDuration), "Error setting MFT sample duration.\n");

						IMFMediaBuffer *buf = NULL;
						DWORD bufLength;
						CHECK_HR(_outputDataBuffer.pSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.\n");
						CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.\n");
						printf("Writing sample \n");


						//printf("Writing sample %i, spacing %I64dms, sample time %I64d, sample duration %I64d, sample size %i.\n", frameCount, now - _lastSendAt, llVideoTimeStamp, llSampleDuration, bufLength);

						BYTE * rawBuffer = NULL;
						buf->Lock(&rawBuffer, NULL, NULL);
						len = bufLength;
						*buff = new BYTE[len];
						memmove(*buff, rawBuffer, len);
						buf->Unlock();

						SafeRelease(&buf);

						frameSent = true;
					}

					break;
				}
			}


		}


		done:

		SafeRelease(videoSample);
		SafeRelease(mftOutSample);
		SafeRelease(pBuffer);
		return frameSent;
		
	}

	void deInit()
	{
		SafeRelease(m_pSource);
		SafeRelease(m_pSourceReader);
		SafeRelease(m_pSrcOutMediaType);
		SafeRelease(m_pMFTInputMediaType); 
		SafeRelease(m_pMFTOutputMediaType);
		SafeRelease(m_pTransform);
	}

public:
	static const int CAMERA_RESOLUTION_WIDTH = 800; // 800; // 1280;
	static const int CAMERA_RESOLUTION_HEIGHT = 600; // 600; //  1024;
	static const int TARGET_FRAME_RATE = 30;// 5; 15; 30	// Note that this if the video device does not support this frame rate the video source reader will fail to initialise.
	static const int TARGET_AVERAGE_BIT_RATE = 345600000; // Adjusting this affects the quality of the H264 bit stream.
	static const int WEBCAM_DEVICE_INDEX = 0;	// <--- Set to 0 to use default system webcam.

public:
	IMFMediaSource *m_pSource{ NULL };
	IMFSourceReader *m_pSourceReader{ NULL };
	IMFMediaType *m_pSrcOutMediaType{ NULL }, *m_pMFTInputMediaType{ NULL }, *m_pMFTOutputMediaType{ NULL };
	IMFTransform *m_pTransform{ NULL };


private:
	bool m_isInit{ false };
};

MFCamera g_camera;

int main()
{
	CHECK_HR(CoInitializeEx(0, COINIT_MULTITHREADED), "CoInitializeEx error!\n");
	CHECK_HR(MFStartup(MF_VERSION), "MFStartup error!\n");

	if (!g_camera.init())
	{
		printf("camera init error");
		goto done;
	}

	int count = 0;

	while (count < 100)
	{
		count++;
		BYTE* buff = NULL;
		DWORD len;
		g_camera.readFrame(&buff, len);

		if (buff)
		{
			delete buff;
			buff = NULL;
		}
		
	}

	g_camera.deInit();
	return 0;
done:
	return 1;
}

/*int main()
{
	static const int CAMERA_RESOLUTION_WIDTH = 800; // 800; // 1280;
	static const int CAMERA_RESOLUTION_HEIGHT = 600; // 600; //  1024;
	static const int TARGET_FRAME_RATE = 30;// 5; 15; 30	// Note that this if the video device does not support this frame rate the video source reader will fail to initialise.
	static const int TARGET_AVERAGE_BIT_RATE = 345600000; // Adjusting this affects the quality of the H264 bit stream.
	static const int WEBCAM_DEVICE_INDEX = 0;	// <--- Set to 0 to use default system webcam.
	

	CHECK_HR(CoInitializeEx(0, COINIT_MULTITHREADED), "CoInitializeEx error!\n");
	CHECK_HR(MFStartup(MF_VERSION), "MFStartup error!\n");

	IMFMediaSource *pSource = NULL;
	IMFAttributes *pConfig = NULL;
	IMFActivate **ppDevices = NULL;
	IMFSourceReader *pSourceReader = NULL;
	IMFMediaType *pSrcOutMediaType = NULL, *pMFTInputMediaType = NULL, *pMFTOutputMediaType = NULL;
	IUnknown *spTransformUnk = NULL;
	IMFTransform *pTransform = NULL;
	UINT32 count = 0;
	DWORD mftStatus = 0;

	// Create an attribute store to hold the search criteria.
	CHECK_HR(MFCreateAttributes(&pConfig, 1), "MFCreateAttributes error\n");

	// Request video capture devices.
	CHECK_HR(pConfig->SetGUID(
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
	), "pConfig->SetGUID error");

	// Enumerate the devices
	CHECK_HR(MFEnumDeviceSources(pConfig, &ppDevices, &count), "MFEnumDeviceSources error\n");

	// Create a media source for the first device in the list.
	CHECK_HR(ppDevices[WEBCAM_DEVICE_INDEX]->ActivateObject(IID_PPV_ARGS(&pSource)), "ppDevices[0]->ActivateObject error\n");

	// Create a source reader.
	CHECK_HR(MFCreateSourceReaderFromMediaSource(
		pSource,
		pConfig,
		&pSourceReader), "MFCreateSourceReaderFromMediaSource error \n");

	//EnumerateCaptureFormats(pSource);

	MFCreateMediaType(&pSrcOutMediaType);
	pSrcOutMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pSrcOutMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
	MFSetAttributeSize(pSrcOutMediaType, MF_MT_FRAME_SIZE, CAMERA_RESOLUTION_WIDTH, CAMERA_RESOLUTION_HEIGHT);
	CHECK_HR(MFSetAttributeRatio(pSrcOutMediaType, MF_MT_FRAME_RATE, TARGET_FRAME_RATE, 1), "Failed to set frame rate on video device out type.\n");

	CHECK_HR(pSourceReader->SetCurrentMediaType(0, NULL, pSrcOutMediaType), "Failed to set media type on source reader.\n");


	CHECK_HR(CoCreateInstance(CLSID_CMSH264EncoderMFT, NULL, CLSCTX_INPROC_SERVER,
		IID_IUnknown, (void**)&spTransformUnk), "Failed to create H264 encoder MFT.\n");

	CHECK_HR(spTransformUnk->QueryInterface(IID_PPV_ARGS(&pTransform)), "Failed to get IMFTransform interface from H264 encoder MFT object.\n");


	MFCreateMediaType(&pMFTOutputMediaType);
	pMFTOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, TARGET_AVERAGE_BIT_RATE), "Failed to set average bit rate on H264 output media type.\n");
	CHECK_HR(MFSetAttributeSize(pMFTOutputMediaType, MF_MT_FRAME_SIZE, CAMERA_RESOLUTION_WIDTH, CAMERA_RESOLUTION_HEIGHT), "Failed to set frame size on H264 MFT out type.\n");
	CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_FRAME_RATE, TARGET_FRAME_RATE, 1), "Failed to set frame rate on H264 MFT out type.\n");
	CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.\n");
	pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);	// 2 = Progressive scan, i.e. non-interlaced.
	pMFTOutputMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

	CHECK_HR(pTransform->SetOutputType(0, pMFTOutputMediaType, 0), "Failed to set output media type on H.264 encoder MFT.\n");

	MFCreateMediaType(&pMFTInputMediaType);
	pMFTInputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
	CHECK_HR(MFSetAttributeSize(pMFTInputMediaType, MF_MT_FRAME_SIZE, CAMERA_RESOLUTION_WIDTH, CAMERA_RESOLUTION_HEIGHT), "Failed to set frame size on H264 MFT out type.\n");
	CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType, MF_MT_FRAME_RATE, TARGET_FRAME_RATE, 1), "Failed to set frame rate on H264 MFT out type.\n");
	CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Failed to set aspect ratio on H264 MFT out type.\n");
	pMFTInputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2);

	CHECK_HR(pTransform->SetInputType(0, pMFTInputMediaType, 0), "Failed to set input media type on H.264 encoder MFT.\n");

	CHECK_HR(pTransform->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 MFT.\n");
	if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
		printf("E: ApplyTransform() pTransform->GetInputStatus() not accept data.\n");
		goto done;
	}


	CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 MFT.\n");
	CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 MFT.\n");
	CHECK_HR(pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 MFT.\n");


	int frameCount = 0;
	DWORD mftOutFlags;
	long int _lastSendAt = ::GetTickCount();

	while (frameCount < 1000)
	{

		::Sleep(10);

		IMFSample *videoSample = NULL;
		MFT_OUTPUT_STREAM_INFO StreamInfo;
		IMFSample *mftOutSample = NULL;
		IMFMediaBuffer *pBuffer = NULL;
		LONGLONG llVideoTimeStamp, llSampleDuration;
		DWORD streamIndex, flags;
		MFT_OUTPUT_DATA_BUFFER _outputDataBuffer;
		HRESULT mftProcessOutput = S_OK;
		DWORD processOutputStatus = 0;



		CHECK_HR(pSourceReader->ReadSample(
			MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			0,                              // Flags.
			&streamIndex,                   // Receives the actual stream index. 
			&flags,                         // Receives status flags.
			&llVideoTimeStamp,              // Receives the time stamp.
			&videoSample                    // Receives the sample or NULL.
		), "Error reading video sample.");

		if (videoSample)
		{
			frameCount++;

			CHECK_HR(videoSample->SetSampleTime(llVideoTimeStamp), "Error setting the video sample time.\n");
			CHECK_HR(videoSample->GetSampleDuration(&llSampleDuration), "Error getting video sample duration.\n");

			HRESULT hr = pTransform->ProcessInput(0, videoSample, 0);
			CHECK_HR(hr, "The resampler H264 ProcessInput call failed.\n");

			CHECK_HR(pTransform->GetOutputStatus(&mftOutFlags), "H264 MFT GetOutputStatus failed.\n");

			if (mftOutFlags == MFT_OUTPUT_STATUS_SAMPLE_READY)
			{
				printf("Sample ready.\n");

				CHECK_HR(pTransform->GetOutputStreamInfo(0, &StreamInfo), "Failed to get output stream info from H264 MFT.\n");

				CHECK_HR(MFCreateSample(&mftOutSample), "Failed to create MF sample.\n");
				CHECK_HR(MFCreateMemoryBuffer(StreamInfo.cbSize, &pBuffer), "Failed to create memory buffer.\n");
				CHECK_HR(mftOutSample->AddBuffer(pBuffer), "Failed to add sample to buffer.\n");

				while (true)
				{
					_outputDataBuffer.dwStreamID = 0;
					_outputDataBuffer.dwStatus = 0;
					_outputDataBuffer.pEvents = NULL;
					_outputDataBuffer.pSample = mftOutSample;

					mftProcessOutput = pTransform->ProcessOutput(0, 1, &_outputDataBuffer, &processOutputStatus);

					if (mftProcessOutput != MF_E_TRANSFORM_NEED_MORE_INPUT)
					{
						CHECK_HR(_outputDataBuffer.pSample->SetSampleTime(llVideoTimeStamp), "Error setting MFT sample time.\n");
						CHECK_HR(_outputDataBuffer.pSample->SetSampleDuration(llSampleDuration), "Error setting MFT sample duration.\n");

						IMFMediaBuffer *buf = NULL;
						DWORD bufLength;
						CHECK_HR(_outputDataBuffer.pSample->ConvertToContiguousBuffer(&buf), "ConvertToContiguousBuffer failed.\n");
						CHECK_HR(buf->GetCurrentLength(&bufLength), "Get buffer length failed.\n");

						auto now = GetTickCount();

						printf("Writing sample %i, spacing %I64dms, sample time %I64d, sample duration %I64d, sample size %i.\n", frameCount, now - _lastSendAt, llVideoTimeStamp, llSampleDuration, bufLength);

						BYTE * rawBuffer = NULL;
						buf->Lock(&rawBuffer, NULL, NULL);

						buf->Unlock();

						SafeRelease(&buf);

						_lastSendAt = GetTickCount();

					}

					SafeRelease(&pBuffer);
					SafeRelease(&mftOutSample);

					break;
				}
			}


		}

		SafeRelease(videoSample);
	}







done:
	SafeRelease(&pConfig);

	for (DWORD i = 0; i < count; i++)
	{
		SafeRelease(&ppDevices[i]);
	}
	CoTaskMemFree(ppDevices);

    return 1;
}*/



HRESULT EnumerateCaptureFormats(IMFMediaSource *pSource)
{
	IMFPresentationDescriptor *pPD = NULL;
	IMFStreamDescriptor *pSD = NULL;
	IMFMediaTypeHandler *pHandler = NULL;
	IMFMediaType *pType = NULL;

	HRESULT hr = pSource->CreatePresentationDescriptor(&pPD);
	if (FAILED(hr))
	{
		goto done;
	}

	BOOL fSelected;
	hr = pPD->GetStreamDescriptorByIndex(0, &fSelected, &pSD);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = pSD->GetMediaTypeHandler(&pHandler);
	if (FAILED(hr))
	{
		goto done;
	}

	DWORD cTypes = 0;
	hr = pHandler->GetMediaTypeCount(&cTypes);
	if (FAILED(hr))
	{
		goto done;
	}

	for (DWORD i = 0; i < cTypes; i++)
	{
		hr = pHandler->GetMediaTypeByIndex(i, &pType);
		if (FAILED(hr))
		{
			goto done;
		}

		LogMediaType(pType);
		OutputDebugString(L"\n");

		SafeRelease(&pType);
	}

done:
	SafeRelease(&pPD);
	SafeRelease(&pSD);
	SafeRelease(&pHandler);
	SafeRelease(&pType);
	return hr;
}


HRESULT LogMediaType(IMFMediaType *pType)
{
	UINT32 count = 0;

	HRESULT hr = pType->GetCount(&count);
	if (FAILED(hr))
	{
		return hr;
	}

	if (count == 0)
	{
		DBGMSG(L"Empty media type.\n");
	}

	for (UINT32 i = 0; i < count; i++)
	{
		hr = LogAttributeValueByIndex(pType, i);
		if (FAILED(hr))
		{
			break;
		}
	}
	return hr;
}

HRESULT LogAttributeValueByIndex(IMFAttributes *pAttr, DWORD index)
{
	WCHAR *pGuidName = NULL;
	WCHAR *pGuidValName = NULL;

	GUID guid = { 0 };

	PROPVARIANT var;
	PropVariantInit(&var);

	HRESULT hr = pAttr->GetItemByIndex(index, &guid, &var);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = GetGUIDName(guid, &pGuidName);
	if (FAILED(hr))
	{
		goto done;
	}

	DBGMSG(L"\t%s\t", pGuidName);

	hr = SpecialCaseAttributeValue(guid, var);
	if (FAILED(hr))
	{
		goto done;
	}
	if (hr == S_FALSE)
	{
		switch (var.vt)
		{
		case VT_UI4:
			DBGMSG(L"%d", var.ulVal);
			break;

		case VT_UI8:
			DBGMSG(L"%I64d", var.uhVal);
			break;

		case VT_R8:
			DBGMSG(L"%f", var.dblVal);
			break;

		case VT_CLSID:
			hr = GetGUIDName(*var.puuid, &pGuidValName);
			if (SUCCEEDED(hr))
			{
				DBGMSG(pGuidValName);
			}
			break;

		case VT_LPWSTR:
			DBGMSG(var.pwszVal);
			break;

		case VT_VECTOR | VT_UI1:
			DBGMSG(L"<<byte array>>");
			break;

		case VT_UNKNOWN:
			DBGMSG(L"IUnknown");
			break;

		default:
			DBGMSG(L"Unexpected attribute type (vt = %d)", var.vt);
			break;
		}
	}

done:
	DBGMSG(L"\n");
	CoTaskMemFree(pGuidName);
	CoTaskMemFree(pGuidValName);
	PropVariantClear(&var);
	return hr;
}

HRESULT GetGUIDName(const GUID& guid, WCHAR **ppwsz)
{
	HRESULT hr = S_OK;
	WCHAR *pName = NULL;

	LPCWSTR pcwsz = GetGUIDNameConst(guid);
	if (pcwsz)
	{
		size_t cchLength = 0;

		hr = StringCchLength(pcwsz, STRSAFE_MAX_CCH, &cchLength);
		if (FAILED(hr))
		{
			goto done;
		}

		pName = (WCHAR*)CoTaskMemAlloc((cchLength + 1) * sizeof(WCHAR));

		if (pName == NULL)
		{
			hr = E_OUTOFMEMORY;
			goto done;
		}

		hr = StringCchCopy(pName, cchLength + 1, pcwsz);
		if (FAILED(hr))
		{
			goto done;
		}
	}
	else
	{
		hr = StringFromCLSID(guid, &pName);
	}

done:
	if (FAILED(hr))
	{
		*ppwsz = NULL;
		CoTaskMemFree(pName);
	}
	else
	{
		*ppwsz = pName;
	}
	return hr;
}

void LogUINT32AsUINT64(const PROPVARIANT& var)
{
	UINT32 uHigh = 0, uLow = 0;
	Unpack2UINT32AsUINT64(var.uhVal.QuadPart, &uHigh, &uLow);
	DBGMSG(L"%d x %d", uHigh, uLow);
}

float OffsetToFloat(const MFOffset& offset)
{
	return offset.value + (static_cast<float>(offset.fract) / 65536.0f);
}

HRESULT LogVideoArea(const PROPVARIANT& var)
{
	if (var.caub.cElems < sizeof(MFVideoArea))
	{
		return MF_E_BUFFERTOOSMALL;
	}

	MFVideoArea *pArea = (MFVideoArea*)var.caub.pElems;

	DBGMSG(L"(%f,%f) (%d,%d)", OffsetToFloat(pArea->OffsetX), OffsetToFloat(pArea->OffsetY),
		pArea->Area.cx, pArea->Area.cy);
	return S_OK;
}

// Handle certain known special cases.
HRESULT SpecialCaseAttributeValue(GUID guid, const PROPVARIANT& var)
{
	if ((guid == MF_MT_FRAME_RATE) || (guid == MF_MT_FRAME_RATE_RANGE_MAX) ||
		(guid == MF_MT_FRAME_RATE_RANGE_MIN) || (guid == MF_MT_FRAME_SIZE) ||
		(guid == MF_MT_PIXEL_ASPECT_RATIO))
	{
		// Attributes that contain two packed 32-bit values.
		LogUINT32AsUINT64(var);
	}
	else if ((guid == MF_MT_GEOMETRIC_APERTURE) ||
		(guid == MF_MT_MINIMUM_DISPLAY_APERTURE) ||
		(guid == MF_MT_PAN_SCAN_APERTURE))
	{
		// Attributes that an MFVideoArea structure.
		return LogVideoArea(var);
	}
	else
	{
		return S_FALSE;
	}
	return S_OK;
}

void DBGMSG(PCWSTR format, ...)
{
	va_list args;
	va_start(args, format);

	WCHAR msg[MAX_PATH];

	if (SUCCEEDED(StringCbVPrintf(msg, sizeof(msg), format, args)))
	{
		OutputDebugString(msg);
	}
}

#ifndef IF_EQUAL_RETURN
#define IF_EQUAL_RETURN(param, val) if(val == param) return L#val
#endif

LPCWSTR GetGUIDNameConst(const GUID& guid)
{
	IF_EQUAL_RETURN(guid, MF_MT_MAJOR_TYPE);
	IF_EQUAL_RETURN(guid, MF_MT_MAJOR_TYPE);
	IF_EQUAL_RETURN(guid, MF_MT_SUBTYPE);
	IF_EQUAL_RETURN(guid, MF_MT_ALL_SAMPLES_INDEPENDENT);
	IF_EQUAL_RETURN(guid, MF_MT_FIXED_SIZE_SAMPLES);
	IF_EQUAL_RETURN(guid, MF_MT_COMPRESSED);
	IF_EQUAL_RETURN(guid, MF_MT_SAMPLE_SIZE);
	IF_EQUAL_RETURN(guid, MF_MT_WRAPPED_TYPE);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_NUM_CHANNELS);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_SAMPLES_PER_SECOND);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_AVG_BYTES_PER_SECOND);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_BLOCK_ALIGNMENT);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_BITS_PER_SAMPLE);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_VALID_BITS_PER_SAMPLE);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_SAMPLES_PER_BLOCK);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_CHANNEL_MASK);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_FOLDDOWN_MATRIX);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_PEAKREF);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_PEAKTARGET);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_AVGREF);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_AVGTARGET);
	IF_EQUAL_RETURN(guid, MF_MT_AUDIO_PREFER_WAVEFORMATEX);
	IF_EQUAL_RETURN(guid, MF_MT_AAC_PAYLOAD_TYPE);
	IF_EQUAL_RETURN(guid, MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION);
	IF_EQUAL_RETURN(guid, MF_MT_FRAME_SIZE);
	IF_EQUAL_RETURN(guid, MF_MT_FRAME_RATE);
	IF_EQUAL_RETURN(guid, MF_MT_FRAME_RATE_RANGE_MAX);
	IF_EQUAL_RETURN(guid, MF_MT_FRAME_RATE_RANGE_MIN);
	IF_EQUAL_RETURN(guid, MF_MT_PIXEL_ASPECT_RATIO);
	IF_EQUAL_RETURN(guid, MF_MT_DRM_FLAGS);
	IF_EQUAL_RETURN(guid, MF_MT_PAD_CONTROL_FLAGS);
	IF_EQUAL_RETURN(guid, MF_MT_SOURCE_CONTENT_HINT);
	IF_EQUAL_RETURN(guid, MF_MT_VIDEO_CHROMA_SITING);
	IF_EQUAL_RETURN(guid, MF_MT_INTERLACE_MODE);
	IF_EQUAL_RETURN(guid, MF_MT_TRANSFER_FUNCTION);
	IF_EQUAL_RETURN(guid, MF_MT_VIDEO_PRIMARIES);
	IF_EQUAL_RETURN(guid, MF_MT_CUSTOM_VIDEO_PRIMARIES);
	IF_EQUAL_RETURN(guid, MF_MT_YUV_MATRIX);
	IF_EQUAL_RETURN(guid, MF_MT_VIDEO_LIGHTING);
	IF_EQUAL_RETURN(guid, MF_MT_VIDEO_NOMINAL_RANGE);
	IF_EQUAL_RETURN(guid, MF_MT_GEOMETRIC_APERTURE);
	IF_EQUAL_RETURN(guid, MF_MT_MINIMUM_DISPLAY_APERTURE);
	IF_EQUAL_RETURN(guid, MF_MT_PAN_SCAN_APERTURE);
	IF_EQUAL_RETURN(guid, MF_MT_PAN_SCAN_ENABLED);
	IF_EQUAL_RETURN(guid, MF_MT_AVG_BITRATE);
	IF_EQUAL_RETURN(guid, MF_MT_AVG_BIT_ERROR_RATE);
	IF_EQUAL_RETURN(guid, MF_MT_MAX_KEYFRAME_SPACING);
	IF_EQUAL_RETURN(guid, MF_MT_DEFAULT_STRIDE);
	IF_EQUAL_RETURN(guid, MF_MT_PALETTE);
	IF_EQUAL_RETURN(guid, MF_MT_USER_DATA);
	IF_EQUAL_RETURN(guid, MF_MT_AM_FORMAT_TYPE);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG_START_TIME_CODE);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG2_PROFILE);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG2_LEVEL);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG2_FLAGS);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG_SEQUENCE_HEADER);
	IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_SRC_PACK_0);
	IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_CTRL_PACK_0);
	IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_SRC_PACK_1);
	IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_CTRL_PACK_1);
	IF_EQUAL_RETURN(guid, MF_MT_DV_VAUX_SRC_PACK);
	IF_EQUAL_RETURN(guid, MF_MT_DV_VAUX_CTRL_PACK);
	IF_EQUAL_RETURN(guid, MF_MT_ARBITRARY_HEADER);
	IF_EQUAL_RETURN(guid, MF_MT_ARBITRARY_FORMAT);
	IF_EQUAL_RETURN(guid, MF_MT_IMAGE_LOSS_TOLERANT);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG4_SAMPLE_DESCRIPTION);
	IF_EQUAL_RETURN(guid, MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY);
	IF_EQUAL_RETURN(guid, MF_MT_ORIGINAL_4CC);
	IF_EQUAL_RETURN(guid, MF_MT_ORIGINAL_WAVE_FORMAT_TAG);

	// Media types

	IF_EQUAL_RETURN(guid, MFMediaType_Audio);
	IF_EQUAL_RETURN(guid, MFMediaType_Video);
	IF_EQUAL_RETURN(guid, MFMediaType_Protected);
	IF_EQUAL_RETURN(guid, MFMediaType_SAMI);
	IF_EQUAL_RETURN(guid, MFMediaType_Script);
	IF_EQUAL_RETURN(guid, MFMediaType_Image);
	IF_EQUAL_RETURN(guid, MFMediaType_HTML);
	IF_EQUAL_RETURN(guid, MFMediaType_Binary);
	IF_EQUAL_RETURN(guid, MFMediaType_FileTransfer);

	IF_EQUAL_RETURN(guid, MFVideoFormat_AI44); //     FCC('AI44')
	IF_EQUAL_RETURN(guid, MFVideoFormat_ARGB32); //   D3DFMT_A8R8G8B8 
	IF_EQUAL_RETURN(guid, MFVideoFormat_AYUV); //     FCC('AYUV')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DV25); //     FCC('dv25')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DV50); //     FCC('dv50')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DVH1); //     FCC('dvh1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DVSD); //     FCC('dvsd')
	IF_EQUAL_RETURN(guid, MFVideoFormat_DVSL); //     FCC('dvsl')
	IF_EQUAL_RETURN(guid, MFVideoFormat_H264); //     FCC('H264')
	IF_EQUAL_RETURN(guid, MFVideoFormat_I420); //     FCC('I420')
	IF_EQUAL_RETURN(guid, MFVideoFormat_IYUV); //     FCC('IYUV')
	IF_EQUAL_RETURN(guid, MFVideoFormat_M4S2); //     FCC('M4S2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MJPG);
	IF_EQUAL_RETURN(guid, MFVideoFormat_MP43); //     FCC('MP43')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MP4S); //     FCC('MP4S')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MP4V); //     FCC('MP4V')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MPG1); //     FCC('MPG1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MSS1); //     FCC('MSS1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_MSS2); //     FCC('MSS2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_NV11); //     FCC('NV11')
	IF_EQUAL_RETURN(guid, MFVideoFormat_NV12); //     FCC('NV12')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P010); //     FCC('P010')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P016); //     FCC('P016')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P210); //     FCC('P210')
	IF_EQUAL_RETURN(guid, MFVideoFormat_P216); //     FCC('P216')
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB24); //    D3DFMT_R8G8B8 
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB32); //    D3DFMT_X8R8G8B8 
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB555); //   D3DFMT_X1R5G5B5 
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB565); //   D3DFMT_R5G6B5 
	IF_EQUAL_RETURN(guid, MFVideoFormat_RGB8);
	IF_EQUAL_RETURN(guid, MFVideoFormat_UYVY); //     FCC('UYVY')
	IF_EQUAL_RETURN(guid, MFVideoFormat_v210); //     FCC('v210')
	IF_EQUAL_RETURN(guid, MFVideoFormat_v410); //     FCC('v410')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WMV1); //     FCC('WMV1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WMV2); //     FCC('WMV2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WMV3); //     FCC('WMV3')
	IF_EQUAL_RETURN(guid, MFVideoFormat_WVC1); //     FCC('WVC1')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y210); //     FCC('Y210')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y216); //     FCC('Y216')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y410); //     FCC('Y410')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y416); //     FCC('Y416')
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y41P);
	IF_EQUAL_RETURN(guid, MFVideoFormat_Y41T);
	IF_EQUAL_RETURN(guid, MFVideoFormat_YUY2); //     FCC('YUY2')
	IF_EQUAL_RETURN(guid, MFVideoFormat_YV12); //     FCC('YV12')
	IF_EQUAL_RETURN(guid, MFVideoFormat_YVYU);

	IF_EQUAL_RETURN(guid, MFAudioFormat_PCM); //              WAVE_FORMAT_PCM 
	IF_EQUAL_RETURN(guid, MFAudioFormat_Float); //            WAVE_FORMAT_IEEE_FLOAT 
	IF_EQUAL_RETURN(guid, MFAudioFormat_DTS); //              WAVE_FORMAT_DTS 
	IF_EQUAL_RETURN(guid, MFAudioFormat_Dolby_AC3_SPDIF); //  WAVE_FORMAT_DOLBY_AC3_SPDIF 
	IF_EQUAL_RETURN(guid, MFAudioFormat_DRM); //              WAVE_FORMAT_DRM 
	IF_EQUAL_RETURN(guid, MFAudioFormat_WMAudioV8); //        WAVE_FORMAT_WMAUDIO2 
	IF_EQUAL_RETURN(guid, MFAudioFormat_WMAudioV9); //        WAVE_FORMAT_WMAUDIO3 
	IF_EQUAL_RETURN(guid, MFAudioFormat_WMAudio_Lossless); // WAVE_FORMAT_WMAUDIO_LOSSLESS 
	IF_EQUAL_RETURN(guid, MFAudioFormat_WMASPDIF); //         WAVE_FORMAT_WMASPDIF 
	IF_EQUAL_RETURN(guid, MFAudioFormat_MSP1); //             WAVE_FORMAT_WMAVOICE9 
	IF_EQUAL_RETURN(guid, MFAudioFormat_MP3); //              WAVE_FORMAT_MPEGLAYER3 
	IF_EQUAL_RETURN(guid, MFAudioFormat_MPEG); //             WAVE_FORMAT_MPEG 
	IF_EQUAL_RETURN(guid, MFAudioFormat_AAC); //              WAVE_FORMAT_MPEG_HEAAC 
	IF_EQUAL_RETURN(guid, MFAudioFormat_ADTS); //             WAVE_FORMAT_MPEG_ADTS_AAC 

	return NULL;
}