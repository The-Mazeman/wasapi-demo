#include "include.hpp"
#include "platformWindows.hpp"

void createThread(LPTHREAD_START_ROUTINE startRoutine, LPVOID parameter)
{
	HANDLE threadHandle = CreateThread(0, 0, startRoutine, parameter, 0, 0);
	assert(threadHandle != 0);
	if (threadHandle)
	{
		CloseHandle(threadHandle);
	}
}
void createEvent(HANDLE* handle)
{
	HANDLE eventHandle = CreateEvent(0, 0, 0, 0);
	assert(eventHandle != 0);
	if (handle)
	{
		*handle = eventHandle;
	}
}
void getProcessHeap(HANDLE* handle)
{
	HANDLE processHeapHandle = GetProcessHeap();
	assert(processHeapHandle != 0);
	if (handle)
	{
		*handle = processHeapHandle;
	}
}
void allocateMemory(uint64 size, void** memory)
{
	HANDLE processHeapHandle;
	getProcessHeap(&processHeapHandle);
	void* memoryPointer = HeapAlloc(processHeapHandle, HEAP_ZERO_MEMORY, size);
	assert(memoryPointer != 0);
	*memory = (char*)memoryPointer;
}
void freeMemory(void* memory)
{
	HANDLE processHeapHandle;
	getProcessHeap(&processHeapHandle);
	BOOL memoryReleased = HeapFree(processHeapHandle, 0, memory);
	assert(memoryReleased != 0);
}
void createSemaphore(uint initialCount, uint maximumCount, HANDLE* handle)
{
	HANDLE semaphore = CreateSemaphore(0, (int)initialCount, (int)maximumCount, 0);
	assert(semaphore != 0);
	if (handle)
	{
		*handle = semaphore;
	}
}
void waitForSemaphore(HANDLE semaphore)
{
	while (1)
	{
		DWORD result = WaitForSingleObject(semaphore, 0);
		if (result == WAIT_TIMEOUT)
		{
			break;
		}
		ReleaseSemaphore(semaphore, 1, 0);
		Sleep(1);
	}
}