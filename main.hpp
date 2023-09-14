#pragma once

#include "wasapi.hpp"
#include "debug.hpp"
#include "platformWindows.hpp"

struct OutputLoaderInfo
{
	HANDLE startLoaderEvent;
	HANDLE finishLoaderEvent;
	HANDLE exitEvent;

	float* outputBuffer;
	uint bufferFrameCount;
	uint padding;
};