 #include "include.hpp"
#include "main.hpp"

void fillSample(float* input, void* wasapi, uint frameCount)
{
	float* output = {};
	wasapi::getBuffer(wasapi, &output, frameCount);

	uint floatsPerAVX2 = sizeof(__m256) / sizeof(float);
	uint framesPerAVX2 = sizeof(__m256) / (sizeof(float) * 2);
	uint iterationCount = frameCount / framesPerAVX2;

	for (uint i = 0; i != iterationCount; ++i)
	{
		__m256 sample = _mm256_load_ps(input);
		_mm256_store_ps(output, sample);
		input += floatsPerAVX2;
		output += floatsPerAVX2;
	}

	wasapi::releaseBuffer(wasapi, frameCount);
}
DWORD WINAPI outputLoader(LPVOID parameter)
{
	OutputLoaderInfo* outputLoaderInfo = (OutputLoaderInfo*)parameter;

	float* sampleChunk = outputLoaderInfo->sampleChunk;
	void* wasapi = outputLoaderInfo->wasapi;
	uint frameCount = outputLoaderInfo->bufferFrameCount;

	HANDLE finishLoaderEvent = outputLoaderInfo->finishLoaderEvent;
	HANDLE startLoaderEvent = outputLoaderInfo->startLoaderEvent;
	HANDLE exitEvent = outputLoaderInfo->exitEvent;
	HANDLE waitHandle[] = {startLoaderEvent, exitEvent};
	uint running = 1;
	while(running)
	{
		uint signal = WaitForMultipleObjects(2, waitHandle, 0, INFINITE);
		switch(signal)
		{
			case WAIT_OBJECT_0:
			{
				fillSample(sampleChunk, wasapi, frameCount);
				SetEvent(finishLoaderEvent);
				break;
			}
			case WAIT_OBJECT_0 + 1:
			{
				freeMemory(outputLoaderInfo);
				running = 0;
			}
		}
	}
	return 0;
}
int main(void)
{
	// create a sin waveform
	float sampleChunk[2048] = {};
	float step = (2.0f * (float)M_PI) / 32.0f;
	for (uint i = 0; i != 1024; ++i)
	{
		sampleChunk[i * 2] = sinf(step * (float)i);
		sampleChunk[(i * 2) + 1] = sinf(step * (float)i);
	}
	void* wasapi;
	wasapi::create(&wasapi);

	// a common playback format
	AudioEndpointFormat format = {};
	format.type = WAVE_FORMAT_IEEE_FLOAT;
	format.bitDepth = 32;
	format.channelCount = 2;
	format.sampleRate = 48000;
	format.bufferFrameCount = 1024;

	wasapi::initializeEndpoint(wasapi, &format);

	HANDLE startLoaderEvent;
	createEvent(&startLoaderEvent);

	HANDLE finishLoaderEvent;
	createEvent(&finishLoaderEvent);

	HANDLE exitEvent;
	createEvent(&exitEvent);

	// a struct containing the required stuff
	OutputLoaderInfo* outputLoaderInfo = {};
	allocateMemory(sizeof(OutputLoaderInfo), (void**)&outputLoaderInfo);
	outputLoaderInfo->sampleChunk = (float*)sampleChunk;
	outputLoaderInfo->wasapi = wasapi;
	outputLoaderInfo->startLoaderEvent = startLoaderEvent;
	outputLoaderInfo->finishLoaderEvent = finishLoaderEvent;
	outputLoaderInfo->exitEvent = exitEvent;
	outputLoaderInfo->bufferFrameCount = format.bufferFrameCount;
	createThread(outputLoader, outputLoaderInfo);

	wasapi::preparePlayback(wasapi, startLoaderEvent, finishLoaderEvent); // give wasapi the start and finish event

	wasapi::startPlayback(wasapi);

	Sleep(1000); // wait for 10 sec

	wasapi::stopPlayback(wasapi); 

	// clean up...
	SetEvent(exitEvent);

	wasapi::free(wasapi);

	CloseHandle(startLoaderEvent);
	CloseHandle(finishLoaderEvent);
	CloseHandle(exitEvent);
}