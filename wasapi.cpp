#include "include.hpp"
#include "wasapi.hpp"

const CLSID CLSID_MMDeviceEnumerator = { 0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e} };
const IID   IID_IMMDeviceEnumerator = { 0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6} };
const IID   IID_IMMNotificationClient = { 0x7991eec9, 0x7e89, 0x4d85, {0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0} };
const IID   IID_IAudioClient = { 0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2} };
const IID   IID_IAudioClient3 = { 0x7ed4ee07, 0x8e67, 0x4cd4, {0x8c, 0x1a, 0x2b, 0x7a, 0x59, 0x87, 0xad, 0x42} };
const IID   IID_IAudioRenderClient = { 0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2} };
const IID   IID_IAudioSessionControl = { 0xf4b1a599, 0x7266, 0x4319, {0xa8, 0xca, 0xe7, 0x0a, 0xcb, 0x11, 0xe8, 0xcd} };
const IID   IID_IAudioSessionEvents = { 0x24918acc, 0x64b3, 0x37c1, {0x8c, 0xa9, 0x74, 0xa6, 0x6e, 0x99, 0x57, 0xa8} };
const IID   IID_IMMEndpoint = { 0x1be09788, 0x6894, 0x4089, {0x85, 0x86, 0x9a, 0x2a, 0x6c, 0x26, 0x5a, 0xc5} };
const IID   IID_IAudioClockAdjustment = { 0xf6e4c0a0, 0x46d9, 0x4fb8, {0xbe, 0x21, 0x57, 0xa3, 0xef, 0x2b, 0x62, 0x6c} };
const IID   IID_IAudioCaptureClient = { 0xc8adbd64, 0xe71e, 0x48a0, {0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17} };
const IID   IID_IAudioClock = { 0xcd63314f, 0x3fba, 0x4a1b, {0x81, 0x2c, 0xef, 0x96, 0x35, 0x87, 0x28, 0xe7} };

void sendLoadSignal(IAudioClient* audioClient, HANDLE endpointLoaderStartEvent, HANDLE endpointLoaderFinishEvent, uint frameCount)
{
	uint paddingFrameCount;
	audioClient->lpVtbl->GetCurrentPadding(audioClient, &paddingFrameCount);
	if (paddingFrameCount < frameCount)
	{
		SetEvent(endpointLoaderStartEvent);
		WaitForSingleObject(endpointLoaderFinishEvent, INFINITE);
	}
}
DWORD WINAPI endpointController(LPVOID parameter)
{
	EndpointControllerInfo* endpointControllerInfo = (EndpointControllerInfo*)parameter;
	IAudioClient* audioClient = endpointControllerInfo->audioClient;
	HANDLE endpointLoaderStartEvent = endpointControllerInfo->endpointLoaderStartEvent;
	HANDLE endpointLoaderFinishEvent = endpointControllerInfo->endpointLoaderFinishEvent;
	HANDLE audioCallback = endpointControllerInfo->audioCallback;
	HANDLE exitSemaphore = endpointControllerInfo->exitSemaphore;
	HANDLE waitHandle[] = {audioCallback, exitSemaphore};
	uint frameCount = endpointControllerInfo->bufferFrameCount;
	uint running = 1;
	while (running)
	{
		uint signal = WaitForMultipleObjects(2, waitHandle, 0, INFINITE);
		switch (signal)
		{
			case WAIT_OBJECT_0:
			{
				sendLoadSignal(audioClient, endpointLoaderStartEvent, endpointLoaderFinishEvent, frameCount);
				break;
			}
			case WAIT_OBJECT_0 + 1:
			{
				freeMemory(endpointControllerInfo);
				running = 0;
			}
		}
	}
	return 0;
}
void transferSample(float* source, float* destination, uint frameCount)
{
	uint framesPerAVX2 = 32 / 8;
	uint iterationCount = frameCount / framesPerAVX2;

	__m256* sourceAVX = (__m256*)source;
	__m256* destinationAVX2 = (__m256*)destination;
	for (uint i = 0; i != iterationCount; ++i)
	{
		_mm256_store_ps((float*)destinationAVX2, *sourceAVX);
		++destinationAVX2;
		++sourceAVX;
	}
}
void loadEndpoint(IAudioRenderClient* renderClient, float* inputBuffer, uint frameCount)
{
	float* audioEngineFrame = {};
	HRESULT result = renderClient->lpVtbl->GetBuffer(renderClient, frameCount, (BYTE**)&audioEngineFrame);
	assert(result == S_OK);
	transferSample(inputBuffer, audioEngineFrame, frameCount);
	renderClient->lpVtbl->ReleaseBuffer(renderClient, frameCount, 0);
}
DWORD WINAPI endpointLoaderStereoFloat(LPVOID parameter)
{
	EndpointLoaderInfo* endpointLoaderInfo = (EndpointLoaderInfo*)parameter;
	HANDLE bufferLoaderStartEvent = endpointLoaderInfo->bufferLoaderStartEvent;
	HANDLE bufferLoaderFinishEvent = endpointLoaderInfo->bufferLoaderFinishEvent;

	IAudioRenderClient* renderClient = endpointLoaderInfo->renderClient;
	float* inputBuffer = (float*)endpointLoaderInfo->inputBuffer;
	uint frameCount = endpointLoaderInfo->bufferFrameCount;
	
	HANDLE exitSemaphore = endpointLoaderInfo->exitSemaphore;
	HANDLE endpointLoaderStartEvent = endpointLoaderInfo->endpointLoaderStartEvent;
	HANDLE endpointLoaderFinishEvent = endpointLoaderInfo->endpointLoaderFinishEvent;

	HANDLE waitHandle[] = {endpointLoaderStartEvent, exitSemaphore};
	uint running = 1;
	while(running)
	{
		uint signal = WaitForMultipleObjects(2, waitHandle, 0, INFINITE);
		switch(signal)
		{
			case WAIT_OBJECT_0:
			{
				WaitForSingleObject(bufferLoaderFinishEvent, INFINITE);
				loadEndpoint(renderClient, inputBuffer, frameCount);
				SetEvent(bufferLoaderStartEvent);
				SetEvent(endpointLoaderFinishEvent);
				break;
			}
			case WAIT_OBJECT_0 + 1:
			{
				freeMemory(endpointLoaderInfo);
				freeMemory(inputBuffer);
				running = 0;
			}
		}
	}
	return 0;
}
void createInputBuffer(WasapiState* wasapi, EndpointLoaderInfo* endpointLoaderInfo)
{
	uint channelCount = wasapi->format.channelCount;
	uint byteDepth = wasapi->format.bitDepth / 8u;
	uint64 frameSize = (uint64)channelCount * byteDepth;
	uint frameCount = wasapi->endpointBufferFrameCount;
	uint64 bufferSize = frameSize * frameCount;

	void* inputBuffer;
	allocateMemory(bufferSize, &inputBuffer);
	endpointLoaderInfo->inputBuffer = inputBuffer;
}
void createAudioClient(WasapiState* wasapi)
{
	HRESULT result;
	result = CoInitializeEx(0, 0);
	assert(result == S_OK);

	IMMDeviceEnumerator* audioEndpointEnumerator = {}; 
	result = CoCreateInstance(CLSID_MMDeviceEnumerator, 0, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&audioEndpointEnumerator);
	assert(result == S_OK);

	wasapi->audioEndpointEnumerator = audioEndpointEnumerator;

	IMMDevice* audioEndpoint = {};
	result = audioEndpointEnumerator->lpVtbl->GetDefaultAudioEndpoint(audioEndpointEnumerator, eRender, eConsole, &audioEndpoint);
	assert(result == S_OK);

	wasapi->audioEndpoint = audioEndpoint;

	IAudioClient* audioClient = {};
	result = audioEndpoint->lpVtbl->Activate(audioEndpoint, IID_IAudioClient, CLSCTX_ALL, 0, (void**)&audioClient);
	assert(result == S_OK);

	wasapi->audioClient = audioClient;
}
void wasapiCreate(void** wasapiHandle)
{
	WasapiState* wasapi = {};
	allocateMemory(sizeof(WasapiState), (void**)&wasapi);
	createAudioClient(wasapi);

	*wasapiHandle = wasapi;
}
ushort getAudioEngineSubFormat(WAVEFORMATEXTENSIBLE* audioEngineFormat)
{
	ushort format = 0;
	if (IsEqualGUID(audioEngineFormat->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
	{
		format = 3;
	}
	if (IsEqualGUID(audioEngineFormat->SubFormat, KSDATAFORMAT_SUBTYPE_PCM))
	{
		format = 1;
	}
	return format;
}
void wasapiInitializeEndpoint(void* wasapiHandle, AudioEndpointFormat* format)
{
	WasapiState* wasapi = (WasapiState*)wasapiHandle;
	wasapi->format = *format;

	IAudioClient* audioClient = wasapi->audioClient;

	HRESULT result;

	WAVEFORMATEX audioEngineFormat = {};
	audioEngineFormat.wFormatTag = (WORD)format->type;
	audioEngineFormat.nChannels = format->channelCount;
	audioEngineFormat.nSamplesPerSec = format->sampleRate;
	audioEngineFormat.nAvgBytesPerSec = format->sampleRate * format->channelCount * (format->bitDepth / 8);
	audioEngineFormat.nBlockAlign = 8;
	audioEngineFormat.wBitsPerSample = format->bitDepth;
	audioEngineFormat.cbSize = 0;

	wasapi->endpointBufferFrameCount = format->bufferFrameCount;
	format->bufferFrameCount = format->bufferFrameCount;

	uint64 bufferDuration = (format->bufferFrameCount * 10000000ull) / format->sampleRate;
	bufferDuration *= 2;

	result = audioClient->lpVtbl->Initialize(audioClient, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, (REFERENCE_TIME)bufferDuration, 0, &audioEngineFormat, 0);
	assert(result == S_OK);

	HANDLE audioCallback;
	createEvent(&audioCallback);

	result = audioClient->lpVtbl->SetEventHandle(audioClient, audioCallback);
	assert(result == S_OK);

	wasapi->audioCallback = audioCallback;

	IAudioRenderClient* audioRenderClient = {};
	result = audioClient->lpVtbl->GetService(audioClient, IID_IAudioRenderClient, (void**)&audioRenderClient);
	assert(result == S_OK);

	wasapi->renderClient = audioRenderClient;

	IAudioClock* audioClock = {};
	result = audioClient->lpVtbl->GetService(audioClient, IID_IAudioClock, (void**)&audioClock);
	assert(result == S_OK);

	wasapi->audioClock = audioClock;

	uint64 frequency;
	result = audioClock->lpVtbl->GetFrequency(audioClock, &frequency);
	assert(result == S_OK);

	wasapi->endpointDeviceFrequency = (uint)frequency;
}
void szudzikHash(uint64 a, uint64 b, uint64* hash)
{
	if (a >= b)
	{
		*hash = a * a + a + b;
	}
	else
	{
		*hash = a + b * b;
	}
}
void wasapiInitializePlayback(void* wasapiHandle, HANDLE bufferLoaderStartEvent, HANDLE bufferLoaderFinishEvent, void** outputBuffer)
{
	WasapiState* wasapi = (WasapiState*)wasapiHandle;
	wasapi->bufferLoaderStartEvent = bufferLoaderStartEvent;

	HANDLE endpointLoaderStartEvent;
	createEvent(&endpointLoaderStartEvent);
	wasapi->endpointLoaderStartEvent = endpointLoaderStartEvent;

	HANDLE endpointLoaderFinishEvent;
	createEvent(&endpointLoaderFinishEvent);
	wasapi->endpointLoaderFinishEvent = endpointLoaderFinishEvent;

	HANDLE exitSemaphore;
	createSemaphore(0, 2, &exitSemaphore);
	wasapi->exitSemaphore = exitSemaphore;

	EndpointControllerInfo* endpointControllerInfo = {};
	allocateMemory(sizeof(EndpointControllerInfo), (void**)&endpointControllerInfo);
	endpointControllerInfo->endpointLoaderStartEvent = endpointLoaderStartEvent;
	endpointControllerInfo->endpointLoaderFinishEvent = endpointLoaderFinishEvent;
	endpointControllerInfo->audioCallback = wasapi->audioCallback;
	endpointControllerInfo->audioClient = wasapi->audioClient;
	endpointControllerInfo->exitSemaphore = exitSemaphore;
	endpointControllerInfo->bufferFrameCount = wasapi->endpointBufferFrameCount;
	createThread(endpointController, endpointControllerInfo);


	EndpointLoaderInfo* endpointLoaderInfo = {};
	allocateMemory(sizeof(EndpointLoaderInfo), (void**)&endpointLoaderInfo);

	endpointLoaderInfo->renderClient = wasapi->renderClient;
	endpointLoaderInfo->endpointLoaderStartEvent = endpointLoaderStartEvent;
	endpointLoaderInfo->endpointLoaderFinishEvent = endpointLoaderFinishEvent;
	endpointLoaderInfo->bufferLoaderStartEvent = bufferLoaderStartEvent;
	endpointLoaderInfo->bufferLoaderFinishEvent = bufferLoaderFinishEvent;
	endpointLoaderInfo->bufferFrameCount = wasapi->endpointBufferFrameCount;
	endpointLoaderInfo->exitSemaphore = exitSemaphore;

	createInputBuffer(wasapi, endpointLoaderInfo);
	*outputBuffer = endpointLoaderInfo->inputBuffer;

	uint64 hash;
	szudzikHash(wasapi->format.channelCount, wasapi->format.bitDepth, &hash);
	szudzikHash(hash, wasapi->format.type, &hash);

	switch(hash)
	{
		case 1053705: // Stereo 32Bit Float
		{
			createThread(endpointLoaderStereoFloat, endpointLoaderInfo);
			break;
		}
	}
}
void wasapiPreload(void* wasapiHandle)
{
	WasapiState* wasapi = (WasapiState*)wasapiHandle;
	HANDLE bufferLoaderStartEvent = wasapi->bufferLoaderStartEvent;
	SetEvent(bufferLoaderStartEvent);
	HANDLE endpointLoaderStartEvent = wasapi->endpointLoaderStartEvent;
	SetEvent(endpointLoaderStartEvent);
	HANDLE endpointLoaderFinishEvent = wasapi->endpointLoaderFinishEvent;
	WaitForSingleObject(endpointLoaderFinishEvent, INFINITE);
}
void wasapiStartPlayback(void* wasapiHandle)
{
	WasapiState* wasapi = (WasapiState*)wasapiHandle;
	IAudioClient* audioClient = wasapi->audioClient;
	audioClient->lpVtbl->Start(audioClient);
}
void flushBuffer(IAudioClient* audioClient)
{
	uint paddingFrameCount = 1u;
	while (paddingFrameCount != 0)
	{
		audioClient->lpVtbl->GetCurrentPadding(audioClient, &paddingFrameCount);
		Sleep(1);
	}
}
void wasapiStopPlayback(void* wasapiHandle)
{
	WasapiState* wasapi = (WasapiState*)wasapiHandle;
	IAudioClient* audioClient = wasapi->audioClient;
	audioClient->lpVtbl->Stop(audioClient);

	HANDLE exitSemaphore = wasapi->exitSemaphore;
	ReleaseSemaphore(exitSemaphore, 2, 0);
	waitForSemaphore(exitSemaphore);

	// play all the samples so that no padding remains

	audioClient->lpVtbl->Start(audioClient);
	flushBuffer(audioClient);                 
	audioClient->lpVtbl->Stop(audioClient);
	audioClient->lpVtbl->Reset(audioClient);  
}
void wasapiFree(void* wasapiHandle)
{
	WasapiState* wasapi = (WasapiState*)wasapiHandle;

	IMMDeviceEnumerator* audioEndpointEnumerator = wasapi->audioEndpointEnumerator;
	audioEndpointEnumerator->lpVtbl->Release(audioEndpointEnumerator);

	IMMDevice* audioEndpoint = wasapi->audioEndpoint;
	audioEndpoint->lpVtbl->Release(audioEndpoint);

	IAudioRenderClient* renderClient = wasapi->renderClient;
	renderClient->lpVtbl->Release(renderClient);

	IAudioClock* audioClock = wasapi->audioClock;
	audioClock->lpVtbl->Release(audioClock);

	IAudioClient* audioClient = wasapi->audioClient;
	audioClient->lpVtbl->Release(audioClient);

}
