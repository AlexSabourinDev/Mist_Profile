#ifndef __MIST_PROFILER_H
#define __MIST_PROFILER_H

/*
Mist_Profiler Usage, License: MIT

About:
The Mist_Profiler is a single header utility that generates chrome:\\tracing json that can be then imported and viewed using chrome's
tracing utility. Mist_Profiler is completely thread safe and attempts to minimize contention between thread.

Usage:
Using Mist_Profiler is simple,

SETUP/TEARDOWN:
- #define MIST_PROFILE_IMPLEMENTATION before including the header file in one of your source files.
- #define MIST_PROFILE_ENABLE to enable profiling
- Mist_ProfileInit(); to init
- Mist_ProfileTerminate(); to close

NOTE: Chrome://tracing matches these calls by name and category assuring a unique name is important.

WARNING: Category and Name are not stored, their lifetime must exist either until program termination or until the next call to Mist_FlushThreadBuffer and Mist_Flush.

USAGE:
Once a significant amount of samples have been gathered, samples have to be flushed.
A simple way to do this is shown below

{
	// Buffers are only added to the list once they are full. This minimizes contention and allows simple modification of the buffers.
	// The buffers are also stored as thread local globals, and thus must be flushed from their respective threads.
	if(Mist_ProfileListSize() == 0)
	{
		// Adds the current buffer to the list of buffers even if it hasn't been filled up yet.
		Mist_FlushThreadBuffer();
	}

	char* print;
	size_t bufferSize;
	Mist_FlushAlloc(&print, &bufferSize);

	fprintf(fileHandle, "%s", mist_ProfilePreface);
	fprintf(fileHandle, "%s", print);
	fprintf(fileHandle, "%s", mist_ProfilePostface);

	free(print);
}

THREADING:

It is important to call Mist_FlushThreadBuffer() before shutting down a thread and 
before the last flush as the remaining buffers might have some samples left in them.

A multithreaded program could have the format:
{
	Init Profiler
	Add mist_ProfilePreface to the file

	Startup job threads
		Create samples in these threads
		At thread termination, call Mist_FlushThreadBuffer()

	Startup flushing thread
		Call Mist_ProfileListSize()
		If there are buffers, flush them to a file
		At thread termination, call Mist_FlushThreadBuffer()

	Kill all the threads
	Call Mist_FlushAlloc() to flush the remaining buffers

	Print to a file
	Add mist_ProfilePostface to the file

	Terminate the profiler
}

*/

/* -API- */

#include <stdint.h>

#define MIST_PROFILE_TYPE_BEGIN 'B'
#define MIST_PROFILE_TYPE_END 'E'
#define MIST_PROFILE_TYPE_INSTANT 'I'

#ifdef MIST_PROFILE_ENABLED

#define MIST_PROFILE_BEGIN(cat, name) Mist_WriteProfileSample(Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_BEGIN));
#define MIST_PROFILE_END(cat, name) Mist_WriteProfileSample(Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_END));
#define MIST_PROFILE_EVENT(cat, name) Mist_WriteProfileSample(Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_INSTANT));

#else

#define MIST_PROFILE_BEGIN(cat, name)
#define MIST_PROFILE_END(cat, name)
#define MIST_PROFILE_EVENT(cat, name)

#endif

typedef struct
{
	uint64_t timeStamp;

	const char* category;
	const char* name;

	uint16_t processorID;
	uint16_t threadID;

	char eventType;

}  Mist_ProfileSample;

static const char* mist_ProfilePreface = "{\"traceEvents\":[{},";
static const char* mist_ProfilePostface = "{}]}";

void Mist_ProfileInit(void);
void Mist_ProfileTerminate(void);

Mist_ProfileSample Mist_CreateProfileSample(const char* category, const char* name, uint64_t timeStamp, char eventType);
void Mist_WriteProfileSample(Mist_ProfileSample sample);

uint16_t Mist_ProfileListSize();

size_t Mist_ProfileStringSize(void);
void Mist_Flush(char* buffer, size_t* maxBufferSize);
void Mist_FlushAlloc(char** buffer, size_t* bufferSize);
void Mist_Free(char* buffer);
void Mist_FlushThreadBuffer(void);

uint64_t Mist_TimeStamp(void);



/* -Implementation- */
#ifdef MIST_PROFILE_IMPLEMENTATION

/* -Platform Macros- */

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__)
	#define MIST_WIN 1
#elif defined(macintosh) || defined(Macintosh) || defined(__APPLE__) && defined(__MACH__)
	#define MIST_MAC 1
#elif defined(__unix__) || defined(__unix)
	#define MIST_UNIX 1
#else
	#error "Mist unsupported platform!"
#endif

#if defined(_MSC_VER)
	#define MIST_MSVC 1
#elif defined(__GNUC__)
	#define MIST_GCC 1
#else
	#error "Mist unsupported compiler!"
#endif

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>

#if MIST_WIN
#include <Windows.h>
#endif

#define MIST_UNUSED(a) (void)a

/* -Threads- */

#if MIST_MSVC
	#define MIST_THREAD_LOCAL __declspec( thread )
#else
	#error "Mist thread local storage not implemented!"
#endif

#if MIST_WIN
	typedef CRITICAL_SECTION Mist_Lock;
#else
	#error "Mist Mutex not implemented!"
#endif

void Mist_InitLock(Mist_Lock* lock)
{
#if MIST_WIN
	InitializeCriticalSection(lock);
#else
	#error "Mist_CreateLock not implemented!"
#endif
}

void Mist_TerminateLock(Mist_Lock* lock)
{
#if MIST_WIN
	DeleteCriticalSection(lock);
#else
	#error "Mist_DestroyLock not implemented!"
#endif
}

void Mist_LockSection(Mist_Lock* lock)
{
#if MIST_WIN
	EnterCriticalSection(lock);
#else
	#error "Mist_LockMutex not implemented!"
#endif
}

void Mist_UnlockSection(Mist_Lock* lock)
{
#if MIST_WIN
	LeaveCriticalSection(lock);
#else
	#error "Mist_UnlockMutex not implemented!"
#endif
}

uint16_t Mist_GetThreadID( void )
{
#if MIST_WIN
	return (uint16_t)GetCurrentThreadId();
#else
	#error "Mist_GetThreadId not implemented!"
#endif
}

uint16_t Mist_GetProcessID( void )
{
#if MIST_WIN
	return (uint16_t)GetProcessId(GetCurrentProcess());
#else
	#error "Mist_GetProcessID not implemented!"
#endif
}

/* -Timer- */

uint64_t Mist_TimeStamp( void )
{
#if MIST_WIN
	LARGE_INTEGER frequency;
	BOOL queryResult = FALSE;
	MIST_UNUSED(queryResult);

	queryResult = QueryPerformanceFrequency(&frequency);
	assert(queryResult == TRUE);

	LARGE_INTEGER time;
	queryResult = QueryPerformanceCounter(&time);
	assert(queryResult == TRUE);

	uint64_t microSeconds = (uint64_t)time.QuadPart * 1000000;
	return microSeconds / (uint64_t)frequency.QuadPart;
#else
	#error "Mist_TimeStamp not implemented!"
#endif
}

/* -Profiler- */

Mist_ProfileSample Mist_CreateProfileSample(const char* category, const char* name, uint64_t timeStamp, char eventType)
{
	Mist_ProfileSample sample;
	sample.timeStamp = timeStamp;
	sample.category = category;
	sample.name = name;
	sample.processorID = Mist_GetProcessID();
	sample.threadID = Mist_GetThreadID();
	sample.eventType = eventType;
	return sample;
}

/* Bigger buffers mean less contention for the list, but also means longer flushes and more memory usage */
#define MIST_BUFFER_SIZE 1024

typedef struct
{
	Mist_ProfileSample samples[MIST_BUFFER_SIZE];
	uint16_t nextSampleWrite;

} Mist_ProfileBuffer;

typedef struct
{
	Mist_ProfileBuffer buffer;
	void* next;

} Mist_ProfileBufferNode;


typedef struct
{
	Mist_ProfileBufferNode* first;
	Mist_ProfileBufferNode* last;
	uint16_t listSize;

	Mist_Lock lock;

} Mist_ProfileBufferList;

Mist_ProfileBufferList mist_ProfileBufferList;
MIST_THREAD_LOCAL Mist_ProfileBuffer mist_ProfileBuffer;

void Mist_ProfileInit( void )
{
	Mist_InitLock(&mist_ProfileBufferList.lock);
}

/* Terminate musst be the last thing called, assure that profiling events will no longer be called once this is called */
void Mist_ProfileTerminate( void )
{
	Mist_ProfileBufferNode* iter;
	Mist_LockSection(&mist_ProfileBufferList.lock);

	iter = mist_ProfileBufferList.first;
	mist_ProfileBufferList.first = NULL;
	mist_ProfileBufferList.last = NULL;
	mist_ProfileBufferList.listSize = 0;

	Mist_UnlockSection(&mist_ProfileBufferList.lock);

	Mist_TerminateLock(&mist_ProfileBufferList.lock);

	while (iter != NULL)
	{
		Mist_ProfileBufferNode* next = (Mist_ProfileBufferNode*)iter->next;
		free(iter);
		iter = next;
	}
}

uint16_t Mist_ProfileListSize()
{
	return mist_ProfileBufferList.listSize;
}

/* Not thread safe */
static void Mist_ProfileAddBufferToList(Mist_ProfileBuffer* buffer)
{
	Mist_ProfileBufferNode* node = (Mist_ProfileBufferNode*)malloc(sizeof(Mist_ProfileBufferNode));
	memcpy(&node->buffer, buffer, sizeof(Mist_ProfileBuffer));
	node->next = NULL;

	if (mist_ProfileBufferList.first == NULL)
	{
		assert(mist_ProfileBufferList.last == NULL);
		mist_ProfileBufferList.first = node;
		mist_ProfileBufferList.last = node;
		mist_ProfileBufferList.listSize = 1;
	}
	else
	{
		assert(mist_ProfileBufferList.listSize != UINT16_MAX);
		assert(mist_ProfileBufferList.last->next == NULL);

		mist_ProfileBufferList.last->next = node;
		mist_ProfileBufferList.last = node;
		mist_ProfileBufferList.listSize++;
	}
}

/* Format: process Id, thread Id,  timestamp, event, category, name */
static const char* const mist_ProfileSample = "{\"pid\":%" PRIu16 ", \"tid\":%" PRIu16 ", \"ts\":%" PRId64 ", \"ph\":\"%c\", \"cat\": \"%s\", \"name\": \"%s\", \"args\":{\"tool\":\"Mist_Profile\"}},";

static size_t Mist_SampleSize(Mist_ProfileSample* sample)
{
	size_t sampleSize = sizeof("{\"pid\":") - 1;
	sampleSize += sample->processorID == 0 ? 1 : (size_t)log10f((float)sample->processorID);
	sampleSize += sizeof(",\"tid\":") - 1;
	sampleSize += sample->threadID == 0 ? 1 : (size_t)log10f((float)sample->threadID);
	sampleSize += sizeof(",\"ts\":") - 1;
	sampleSize += sample->timeStamp == 0 ? 1 : (size_t)log10((double)sample->timeStamp);
	sampleSize += sizeof(",\"ph\":\"") - 1;
	sampleSize += 1; /* sample char */
	sampleSize += sizeof("\",\"cat\":\"") - 1;
	sampleSize += strlen(sample->category);
	sampleSize += sizeof("\", \"name\": \"") - 1;
	sampleSize += strlen(sample->name);
	sampleSize += sizeof("\", \"args\":{\"tool\":\"Mist_Profile\"}},") - 1;
	return sampleSize;
}

static void Mist_Reverse(char* start, char* end)
{
	end -= 1;
	for (; start < end; start++, end--)
	{
		char t = *start;
		*start = *end;
		*end = t;
	}
}

static void Mist_WriteU16(uint16_t val, char* writeBuffer, size_t* writePos)
{
	size_t start = *writePos;
	while (val >= 10)
	{
		// Avoid modulo for debug builds
		uint16_t t = val / 10;
		writeBuffer[(*writePos)++] = '0' + (char)(val - t * 10);
		val = t;
	}
	writeBuffer[(*writePos)++] = '0' + (char)val;

	Mist_Reverse(writeBuffer + start, writeBuffer + *writePos);
}

static void Mist_WriteU64(uint64_t val, char* writeBuffer, size_t* writePos)
{
	size_t start = *writePos;
	while (val >= 10)
	{
		// Avoid modulo for debug builds
		uint64_t t = val / 10;
		writeBuffer[(*writePos)++] = '0' + (char)(val - t * 10);
		val = t;
	}
	writeBuffer[(*writePos)++] = '0' + (char)val;

	Mist_Reverse(writeBuffer + start, writeBuffer + *writePos);
}

#define MIST_MEMCPY_CONST_STR(str, writeBuffer, writePos) \
	{ \
		memcpy(writeBuffer + *writePos, str, sizeof(str) - 1); \
		*writePos += sizeof(str) - 1; \
	}

static void Mist_WriteSample(Mist_ProfileSample* sample, char* writeBuffer, size_t* writePos)
{
	MIST_MEMCPY_CONST_STR("{\"pid\":", writeBuffer, writePos);
	Mist_WriteU16(sample->processorID, writeBuffer, writePos);
	MIST_MEMCPY_CONST_STR(",\"tid\":", writeBuffer, writePos);
	Mist_WriteU16(sample->threadID, writeBuffer, writePos);
	MIST_MEMCPY_CONST_STR(",\"ts\":", writeBuffer, writePos);
	Mist_WriteU64(sample->timeStamp, writeBuffer, writePos);
	MIST_MEMCPY_CONST_STR(",\"ph\":\"", writeBuffer, writePos);
	writeBuffer[(*writePos)++] = sample->eventType;
	MIST_MEMCPY_CONST_STR("\",\"cat\":\"", writeBuffer, writePos);
	size_t strSize = strlen(sample->category);
	memcpy(writeBuffer + (*writePos), sample->category, strSize);
	(*writePos) += strSize;
	MIST_MEMCPY_CONST_STR("\",\"name\":\"", writeBuffer, writePos);
	strSize = strlen(sample->name);
	memcpy(writeBuffer + (*writePos), sample->name, strSize);
	(*writePos) += strSize;
	MIST_MEMCPY_CONST_STR("\",\"args\":{\"tool\":\"Mist_Profile\"}},", writeBuffer, writePos);
}

/* Calculates the size of the samples, allowing the memory to be allocated in one chunk */
/* Thread safe */
size_t Mist_ProfileStringSize(void)
{
	Mist_ProfileBufferNode* start;
	/* We have to keep the lock while calculating the string size. We don't want the list to be stolen or modified while we're working. */
	Mist_LockSection(&mist_ProfileBufferList.lock);

	start = mist_ProfileBufferList.first;

	size_t size = 0;
	while (start != NULL)
	{
		for (uint16_t i = 0; i < start->buffer.nextSampleWrite; i++)
		{
			Mist_ProfileSample* sample = &start->buffer.samples[i];
			size += Mist_SampleSize(sample);
		}

		start = (Mist_ProfileBufferNode*)start->next;
	}

	Mist_UnlockSection(&mist_ProfileBufferList.lock);
	
	return size + 1;
}

/* Returns a string to be printed, this takes time. */
/* Thread safe */
/* bufferSize must be at least >= Mist_SampleStringSize(...) */
void Mist_Flush( char* buffer, size_t* bufferSize )
{
	assert(bufferSize != NULL);

	if (*bufferSize < 4)
	{
		return;
	}

	Mist_ProfileBufferNode* start;
	Mist_LockSection(&mist_ProfileBufferList.lock);

	start = mist_ProfileBufferList.first;
	mist_ProfileBufferList.first = NULL;
	mist_ProfileBufferList.last = NULL;
	mist_ProfileBufferList.listSize = 0;

	Mist_UnlockSection(&mist_ProfileBufferList.lock);

	if (start == NULL)
	{
		buffer[0] = '{';
		buffer[1] = '}';
		buffer[2] = '\0';
	}

	size_t size = 0;
	while (start != NULL)
	{
		for (uint16_t i = 0; i < start->buffer.nextSampleWrite; i++)
		{
			Mist_ProfileSample* sample = &start->buffer.samples[i];
			Mist_WriteSample(sample, buffer, &size);
		}

		Mist_ProfileBufferNode* next = (Mist_ProfileBufferNode*)start->next;
		free(start);
		start = next;
	}

	if (size >= *bufferSize)
	{
		assert(false);
		return;
	}
	buffer[size] = '\0';
	*bufferSize = size + 1;
}

void Mist_FlushAlloc(char** buffer, size_t* bufferSize)
{
	*bufferSize = Mist_ProfileStringSize();
	*buffer = (char*)malloc(*bufferSize);

	Mist_Flush(*buffer, bufferSize);
}

void Mist_Free(char* buffer)
{
	free(buffer);
}

/* Thread safe */
void Mist_WriteProfileSample(Mist_ProfileSample sample)
{
	mist_ProfileBuffer.samples[mist_ProfileBuffer.nextSampleWrite] = sample;

	mist_ProfileBuffer.nextSampleWrite++;
	if (mist_ProfileBuffer.nextSampleWrite == MIST_BUFFER_SIZE)
	{
		Mist_LockSection(&mist_ProfileBufferList.lock);

		Mist_ProfileAddBufferToList(&mist_ProfileBuffer);
		mist_ProfileBuffer.nextSampleWrite = 0;

		Mist_UnlockSection(&mist_ProfileBufferList.lock);
	}
}

/* Thread safe */
void Mist_FlushThreadBuffer( void )
{
	Mist_LockSection(&mist_ProfileBufferList.lock);

	Mist_ProfileAddBufferToList(&mist_ProfileBuffer);
	mist_ProfileBuffer.nextSampleWrite = 0;

	Mist_UnlockSection(&mist_ProfileBufferList.lock);
}

#endif /* MIST_PROFILE_IMPLEMENTATION */

#endif /* __MIST_PROFILER_H */
