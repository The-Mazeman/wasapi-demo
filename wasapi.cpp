#include "include.hpp"
#include "wasapi.hpp"

START_SCOPE(wasapi)
// send the audio callback to the user thread
void sendLoadSignal(IAudioClient* audioClient, HANDLE loaderStartEvent, HANDLE loaderFinishEvent, uint frameCount)
{
	uint paddingFrameCount;
	audioClient->GetCurrentPadding(&paddingFrameCount);
	if (paddingFrameCount < frameCount) // send the callback only if there's space for frameCount
	{
		SetEvent(loaderStartEvent);
		WaitForSingleObject(loaderFinishEvent, INFINITE); // wait for the user to load
		// if we don't wait, it will keep sending the signal 
		// which means the user will get the signal but won't find space for the samples
	}
}
DWORD WINAPI endpointController(LPVOID parameter)
{
	State* state = (State*)parameter;
	IAudioClient* audioClient = state->audioClient;
	HANDLE loaderStartEvent = state->loaderStartEvent;
	HANDLE loaderFinishEvent = state->loaderFinishEvent;
	HANDLE audioCallback = state->audioCallback;
	HANDLE exitSemaphore = state->exitSemaphore;
	HANDLE waitHandle[] = {audioCallback, exitSemaphore};
	uint frameCount = state->endpointBufferFrameCount;
	uint running = 1;
	while (running)
	{
		uint signal = WaitForMultipleObjects(2, waitHandle, 0, INFINITE); // wait for the wasapi callback
		switch (signal)
		{
			case WAIT_OBJECT_0:
			{
				sendLoadSignal(audioClient, loaderStartEvent, loaderFinishEvent, frameCount);
				break;
			}
			case WAIT_OBJECT_0 + 1:
			{
				running = 0; // time to exit
			}
		}
	}
	return 0;
}

void createAudioClient(void* wasapi)
{
	State* state = (State*)wasapi;
	HRESULT result;
	result = CoInitializeEx(0, 0);
	assert(result == S_OK);

	// get all the interface pointers
	IMMDeviceEnumerator* audioEndpointEnumerator = {}; 
	result = CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, IID_PPV_ARGS(&audioEndpointEnumerator));
	assert(result == S_OK);

	state->audioEndpointEnumerator = audioEndpointEnumerator;

	IMMDevice* audioEndpoint = {};
	result = audioEndpointEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &audioEndpoint); // get default output
	assert(result == S_OK);

	state->audioEndpoint = audioEndpoint;

	IAudioClient* audioClient = {};
	result = audioEndpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, (void**)&audioClient); // activate the output
	assert(result == S_OK);

	state->audioClient = audioClient;
}
void create(void** wasapi)
{
	State* state = {};
	allocateMemory(sizeof(State), (void**)&state);
	createAudioClient(state);

	*wasapi = state;
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
void initializeEndpoint(void* wasapi, AudioEndpointFormat* format)
{
	State* state = (State*)wasapi;
	state->format = *format;

	IAudioClient* audioClient = state->audioClient;

	HRESULT result;

	WAVEFORMATEX audioEngineFormat = {};
	audioEngineFormat.wFormatTag = (WORD)format->type;
	audioEngineFormat.nChannels = format->channelCount;
	audioEngineFormat.nSamplesPerSec = format->sampleRate;
	audioEngineFormat.nAvgBytesPerSec = format->sampleRate * format->channelCount * (format->bitDepth / 8);
	audioEngineFormat.nBlockAlign = 8;
	audioEngineFormat.wBitsPerSample = format->bitDepth;
	audioEngineFormat.cbSize = 0;

	state->endpointBufferFrameCount = format->bufferFrameCount;
	format->bufferFrameCount = format->bufferFrameCount;

	// calculate the duration of the buffer since wasapi wants it
	uint64 bufferDuration = (format->bufferFrameCount * 10000000ull) / format->sampleRate;
	bufferDuration *= 2;

	// initialize the output with parameters
	// i am sharing the output with other programs and using events for signaling
	result = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, (REFERENCE_TIME)bufferDuration, 0, &audioEngineFormat, 0);
	assert(result == S_OK);

	HANDLE audioCallback;
	createEvent(&audioCallback);

	result = audioClient->SetEventHandle(audioCallback); // callback event
	assert(result == S_OK);

	state->audioCallback = audioCallback;

	HANDLE exitSemaphore = {};
	createSemaphore(0, 1, &exitSemaphore); // used to exit the thread 
	state->exitSemaphore = exitSemaphore;

	IAudioRenderClient* audioRenderClient = {};
	result = audioClient->GetService(IID_PPV_ARGS(&audioRenderClient));// get the interface to get output buffer
	assert(result == S_OK);

	state->renderClient = audioRenderClient;

	IAudioClock* audioClock = {};
	result = audioClient->GetService(IID_PPV_ARGS(&audioClock)); // with clock you can calculate elapsed time 
	assert(result == S_OK);

	state->audioClock = audioClock;

	uint64 frequency;
	result = audioClock->GetFrequency(&frequency); // divide by this frequency to get time in secs
	assert(result == S_OK);

	state->endpointDeviceFrequency = (uint)frequency;
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
void getBuffer(void* wasapi, float** outputBuffer, uint frameCount)
{
	State* state = (State*)wasapi;
	IAudioRenderClient* renderClient = state->renderClient;
	HRESULT result = renderClient->GetBuffer(frameCount, (BYTE**)outputBuffer); // get output buffer
	assert(result == S_OK);
}
void releaseBuffer(void* wasapi, uint frameCount)
{
	State* state = (State*)wasapi;
	IAudioRenderClient* renderClient = state->renderClient;
	renderClient->ReleaseBuffer(frameCount, 0); // release the output buffer
}
void preparePlayback(void* wasapi, HANDLE loaderStartEvent, HANDLE loaderFinishEvent)
{
	State* state = (State*)wasapi;
	// store the user events 
	state->loaderStartEvent = loaderStartEvent;
	state->loaderFinishEvent = loaderFinishEvent;

	SetEvent(loaderStartEvent); // signal the user to load the buffer before playback
	createThread(endpointController, state);
	WaitForSingleObject(loaderFinishEvent, INFINITE); // wait for the user to finish loading
}
void startPlayback(void* wasapi)
{
	State* state = (State*)wasapi;
	IAudioClient* audioClient = state->audioClient;
	audioClient->Start(); // start the playback
}
void flushBuffer(IAudioClient* audioClient)
{
	uint paddingFrameCount = 1u;
	while (paddingFrameCount != 0)
	{
		// while there are valid frames in the buffer keep playing
		audioClient->GetCurrentPadding(&paddingFrameCount); 
		Sleep(1);
	}
}
void stopPlayback(void* wasapi)
{
	State* state = (State*)wasapi;

	IAudioClient* audioClient = state->audioClient;
	audioClient->Stop(); // stop playback

	HANDLE exitSemaphore = state->exitSemaphore;
	ReleaseSemaphore(exitSemaphore, 1, 0); // signal the controller thread to exit
	waitForSemaphore(exitSemaphore); // wait for the thread to exit

	audioClient->Start(); // start the playback again
	flushBuffer(audioClient);                 
	audioClient->Stop();
	audioClient->Reset();  // resets the clock
}
void free(void* wasapi)
{
	State* state = (State*)wasapi;

	IMMDeviceEnumerator* audioEndpointEnumerator = state->audioEndpointEnumerator;
	audioEndpointEnumerator->Release();

	IMMDevice* audioEndpoint = state->audioEndpoint;
	audioEndpoint->Release();

	IAudioRenderClient* renderClient = state->renderClient;
	renderClient->Release();

	IAudioClock* audioClock = state->audioClock;
	audioClock->Release();

	IAudioClient* audioClient = state->audioClient;
	audioClient->Release();

	CloseHandle(state->exitSemaphore);
	CloseHandle(state->audioCallback);
}
END_SCOPE