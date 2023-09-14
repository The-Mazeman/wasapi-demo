#pragma once

#include "include.hpp"
#include "define.hpp"
#include "debug.hpp"
#include "platformWindows.hpp"

struct AudioEndpointFormat
{
	uint8 channelCount;
	uint8 bitDepth;
	uint16 bufferFrameCount;
	uint32 sampleRate;
	uint32 type;
};
struct WasapiState
{
	AudioEndpointFormat format;
	uint padding;

	IAudioClient* audioClient;
	IAudioRenderClient* renderClient;
	IAudioClock* audioClock;

	HANDLE audioCallback;
	HANDLE bufferLoaderStartEvent;
	HANDLE endpointLoaderStartEvent;
	HANDLE endpointLoaderFinishEvent;
	HANDLE exitSemaphore;

	uint endpointBufferFrameCount;
	uint endpointDeviceFrequency;
};
struct EndpointControllerInfo
{
	IAudioClient* audioClient;
	HANDLE exitSemaphore;

	HANDLE endpointLoaderStartEvent;
	HANDLE endpointLoaderFinishEvent;
	HANDLE audioCallback;

	uint bufferFrameCount;
	uint padding;
};
struct EndpointLoaderInfo
{
	IAudioRenderClient* renderClient;
	HANDLE exitSemaphore;

	HANDLE bufferLoaderStartEvent;
	HANDLE bufferLoaderFinishEvent;
	HANDLE endpointLoaderStartEvent;
	HANDLE endpointLoaderFinishEvent;

	void* inputBuffer;
	uint bufferFrameCount;
	uint padding;
};

void wasapiCreate(void**);
void wasapiInitializeEndpoint(void*, AudioEndpointFormat*);
void wasapiInitializePlayback(void*, HANDLE, HANDLE, void**);
void wasapiPreload(void*);
void wasapiStartPlayback(void*);
void wasapiStopPlayback(void*);
