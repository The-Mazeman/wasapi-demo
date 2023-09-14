 #include "include.hpp"
#include "main.hpp"

void fillZero(float* outputBuffer, uint frameCount)
{
	uint framesPerAVX2 = 32 / 8;
	uint iterationCount = frameCount / framesPerAVX2;

	__m256 zeroAVX2 = _mm256_setzero_ps();
	__m256* destinationAVX2 = (__m256*)outputBuffer;
	for (uint i = 0; i != iterationCount; ++i)
	{
		_mm256_store_ps((float*)destinationAVX2, zeroAVX2);
		++destinationAVX2;
	}
}
DWORD WINAPI outputLoader(LPVOID parameter)
{
	OutputLoaderInfo* outputLoaderInfo = (OutputLoaderInfo*)parameter;

	float* outputBuffer = outputLoaderInfo->outputBuffer;
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
				fillZero(outputBuffer, frameCount);
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
void createOutputLoader(HANDLE startLoaderEvent, HANDLE finishLoaderEvent, HANDLE exitEvent)
{
	OutputLoaderInfo* outputLoaderInfo = {};
	allocateMemory(sizeof(OutputLoaderInfo), (void**)&outputLoaderInfo);
	outputLoaderInfo->startLoaderEvent = startLoaderEvent;
	outputLoaderInfo->finishLoaderEvent = finishLoaderEvent;
	outputLoaderInfo->exitEvent = exitEvent;

	createThread(outputLoader, outputLoaderInfo);
}
int main(void)
{
	void* wasapiHandle;
	wasapiCreate(&wasapiHandle);

	AudioEndpointFormat format = {};
	format.type = WAVE_FORMAT_IEEE_FLOAT;
	format.bitDepth = 32;
	format.channelCount = 2;
	format.sampleRate = 48000;
	format.bufferFrameCount = 1024;

	wasapiInitializeEndpoint(wasapiHandle, &format);

	HANDLE startLoaderEvent;
	createEvent(&startLoaderEvent);

	HANDLE finishLoaderEvent;
	createEvent(&finishLoaderEvent);

	void* outputBuffer;
	wasapiInitializePlayback(wasapiHandle, startLoaderEvent, finishLoaderEvent, &outputBuffer);

	HANDLE exitEvent;
	createEvent(&exitEvent);

	createOutputLoader(startLoaderEvent, finishLoaderEvent, exitEvent);
	wasapiPreload(wasapiHandle);
	wasapiStartPlayback(wasapiHandle);

	Sleep(8000);

	wasapiStopPlayback(wasapiHandle);
	SetEvent(exitEvent);

	wasapiFree(wasapiHandle);
}