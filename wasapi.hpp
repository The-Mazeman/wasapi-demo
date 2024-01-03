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
START_SCOPE(wasapi)
struct State
{
	AudioEndpointFormat format;
	uint padding;
	IMMDeviceEnumerator* audioEndpointEnumerator;
	IMMDevice* audioEndpoint;
	IAudioClient* audioClient;
	IAudioRenderClient* renderClient;
	IAudioClock* audioClock;

	HANDLE audioCallback;
	HANDLE loaderStartEvent;
	HANDLE loaderFinishEvent;
	HANDLE exitSemaphore;

	uint endpointBufferFrameCount;
	uint endpointDeviceFrequency;
};

void create(void**);
void initializeEndpoint(void*, AudioEndpointFormat*);
void preparePlayback(void* wasapi, HANDLE loaderStartEvent, HANDLE loaderFinishEvent);
void startPlayback(void* wasapi);
void getBuffer(void* wasapi, float** outputBuffer, uint frameCount);
void releaseBuffer(void* wasapi, uint frameCount);
void stopPlayback(void* wasapi);
void free(void* wasapi);

END_SCOPE