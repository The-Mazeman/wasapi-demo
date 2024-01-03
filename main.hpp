#pragma once

#include "wasapi.hpp"
#include "debug.hpp"
#include "platformWindows.hpp"

struct OutputLoaderInfo
{
	float* sampleChunk;
	void* wasapi;
	HANDLE startLoaderEvent;
	HANDLE finishLoaderEvent;
	HANDLE exitEvent;

	uint bufferFrameCount;
	uint padding;
};