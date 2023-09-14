#pragma once

#include "debug.hpp"
#include "define.hpp"

void createThread(LPTHREAD_START_ROUTINE, LPVOID);
void allocateMemory(uint64, void**);
void freeMemory(void*);

void createEvent(HANDLE*);
void createSemaphore(uint, uint, HANDLE*);
void waitForSemaphore(HANDLE);