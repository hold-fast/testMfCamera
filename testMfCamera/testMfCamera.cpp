// testMfCamera.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


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


int main()
{
	static const int CAMERA_RESOLUTION_WIDTH = 800;//640; // 800; // 1280;
	static const int CAMERA_RESOLUTION_HEIGHT = 600;//480; // 600; //  1024;
	static const int TARGET_FRAME_RATE = 30;// 5; 15; 30	// Note that this if the video device does not support this frame rate the video source reader will fail to initialise.
	static const int TARGET_AVERAGE_BIT_RATE = 1000000; // Adjusting this affects the quality of the H264 bit stream.
	static const int WEBCAM_DEVICE_INDEX = 0;	// <--- Set to 0 to use default system webcam.

	CHECK_HR(CoInitializeEx(0, COINIT_MULTITHREADED), "CoInitializeEx error!\n");
	CHECK_HR(MFStartup(MF_VERSION), "MFStartup error!\n");

	IMFMediaSource *pSource = NULL;
	IMFAttributes *pConfig = NULL;
	IMFActivate **ppDevices = NULL;
	IMFSourceReader *pSourceReader = NULL;
	UINT32 count = 0;

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



done:
	SafeRelease(&pConfig);

	for (DWORD i = 0; i < count; i++)
	{
		SafeRelease(&ppDevices[i]);
	}
	CoTaskMemFree(ppDevices);

    return 1;
}

