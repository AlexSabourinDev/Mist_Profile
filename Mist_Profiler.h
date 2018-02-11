#ifndef __MIST_PROFILER_H
#define __MIST_PROFILER_H

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

#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

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
	typedef HANDLE Mist_Mutex;
#else
	#error "Mist Mutex not implemented!"
#endif

static Mist_Mutex Mist_CreateMutex( void )
{
#if MIST_WIN
	return CreateMutex(NULL, FALSE, NULL);
#else
	#error "Mist_CreateMutex not implemented!"
#endif
}

static void Mist_DestroyMutex(Mist_Mutex mutex)
{
#if MIST_WIN
	BOOL result = CloseHandle(mutex);
	MIST_UNUSED(result);
	assert(result == TRUE);
#else
	#error "Mist_DestroyMutex not implemented!"
#endif
}

static void Mist_LockMutex(Mist_Mutex mutex)
{
#if MIST_WIN
	DWORD waitResult = WaitForSingleObject(mutex, INFINITE);
	MIST_UNUSED(waitResult);
	assert(waitResult == WAIT_OBJECT_0);
#else
	#error "Mist_LockMutex not implemented!"
#endif
}

static void Mist_UnlockMutex(Mist_Mutex mutex)
{
#if MIST_WIN
	BOOL result = ReleaseMutex(mutex);
	MIST_UNUSED(result);
	assert(result == TRUE);
#else
	#error "Mist_UnlockMutex not implemented!"
#endif
}

static uint16_t Mist_GetThreadID( void )
{
#if MIST_WIN
	return (uint16_t)GetCurrentThreadId();
#else
	#error "Mist_GetThreadId not implemented!"
#endif
}

static uint16_t Mist_GetProcessID( void )
{
#if MIST_WIN
	return (uint16_t)GetProcessId(GetCurrentProcess());
#else
	#error "Mist_GetProcessID not implemented!"
#endif
}

/* -Timer- */

static int64_t Mist_TimeStamp( void )
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

	int64_t microSeconds = (int64_t)time.QuadPart * 1000000;
	return microSeconds / (int64_t)frequency.QuadPart;
#else
	#error "Mist_TimeStamp not implemented!"
#endif
}

/* -Profiler- */

typedef struct
{
	int64_t timeStamp;

	const char* category;
	const char* name;

	uint16_t processorID;
	uint16_t threadID;

	char eventType;

}  Mist_ProfileSample;

static Mist_ProfileSample Mist_CreateProfileSample(const char* category, const char* name, int64_t timeStamp, char eventType)
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

#define MIST_BUFFER_SIZE 1024

typedef struct
{
	Mist_ProfileSample samples[MIST_BUFFER_SIZE];
	uint16_t nextSampleWrite;

} Mist_ProfileBuffer;
MIST_THREAD_LOCAL Mist_ProfileBuffer mist_ProfileBuffer;


typedef struct
{
	Mist_ProfileBuffer buffer;
	void* next;

} Mist_ProfileBufferNode;


typedef struct
{
	Mist_ProfileBufferNode* first;
	Mist_ProfileBufferNode* last;

	Mist_Mutex lock;

} Mist_ProfileBufferList;
Mist_ProfileBufferList mist_ProfileBufferList;

static void Mist_ProfileInit( void )
{
	mist_ProfileBufferList.lock = Mist_CreateMutex();
}

static void Mist_ProfileTerminate( void )
{
	Mist_DestroyMutex(mist_ProfileBufferList.lock);
}

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
	}
	else
	{
		assert(mist_ProfileBufferList.last->next == NULL);
		mist_ProfileBufferList.last->next = node;
		mist_ProfileBufferList.last = node;
	}
}

/* Returns a string to be printed, this takes time. */
/* The returned string must be freed. */
static char* Mist_Flush( void )
{
	Mist_ProfileBufferNode* start;
	Mist_LockMutex(mist_ProfileBufferList.lock);

	start = mist_ProfileBufferList.first;
	mist_ProfileBufferList.first = NULL;
	mist_ProfileBufferList.last = NULL;

	Mist_UnlockMutex(mist_ProfileBufferList.lock);


	char* print = NULL;
	if (start == NULL)
	{
		/* Give an empty string */
		print = (char*)malloc(1);
		*print = 0;
		return print;
	}

	size_t size = 0;
	while (start != NULL)
	{
		/* Format: process Id, thread Id,  timestamp, event, category, name */
		const char* const profileSample = "{\"pid\":%" PRIu16 ", \"tid\":%" PRIu16 ", \"ts\":%" PRId64 ", \"ph\":\"%c\", \"cat\": \"%s\", \"name\": \"%s\", \"args\":{\"tool\":\"Mist_Profile\"}},";

		for (uint16_t i = 0; i < start->buffer.nextSampleWrite; i++)
		{
			Mist_ProfileSample* sample = &start->buffer.samples[i];
			int sampleSize = snprintf(NULL, 0, profileSample, sample->processorID, sample->threadID, sample->timeStamp, sample->eventType, sample->category, sample->name);
			if (sampleSize <= 0)
			{
				assert(false);
				continue;
			}
			
			size_t previousSize = size;
			size += sampleSize;
			char* appended = (char*)realloc(print, size + 1 /* Add space for the null terminator*/);
			if (appended == NULL)
			{
				/* Failed to allocate the buffer! Give up here and return what we have */
				assert(false);
				return print;
			}
			print = appended;

			int printedSize = snprintf(print + previousSize, sampleSize + 1, profileSample, sample->processorID, sample->threadID, sample->timeStamp, sample->eventType, sample->category, sample->name);
			if (printedSize != sampleSize)
			{
				assert(false);
				continue;
			}
		}

		Mist_ProfileBufferNode* next = (Mist_ProfileBufferNode*)start->next;
		free(start);
		start = next;
	}
	/* Erase the last comma*/
	print[size - 1] = '\0';

	return print;
}
const char* mist_PrintPreface = "{\"traceEvents\":[";
const char* mist_PrintPostface = "]}";


static void Mist_WriteProfileSample(Mist_ProfileSample sample)
{
	mist_ProfileBuffer.samples[mist_ProfileBuffer.nextSampleWrite] = sample;

	mist_ProfileBuffer.nextSampleWrite++;
	if (mist_ProfileBuffer.nextSampleWrite == MIST_BUFFER_SIZE)
	{
		Mist_LockMutex(mist_ProfileBufferList.lock);

		Mist_ProfileAddBufferToList(&mist_ProfileBuffer);
		mist_ProfileBuffer.nextSampleWrite = 0;

		Mist_UnlockMutex(mist_ProfileBufferList.lock);
	}
}

static void Mist_FlushActiveBuffer( void )
{
	Mist_LockMutex(mist_ProfileBufferList.lock);

	Mist_ProfileAddBufferToList(&mist_ProfileBuffer);
	mist_ProfileBuffer.nextSampleWrite = 0;

	Mist_UnlockMutex(mist_ProfileBufferList.lock);
}


/* -API- */

#define MIST_PROFILE_TYPE_BEGIN 'B'
#define MIST_PROFILE_TYPE_END 'E'
#define MIST_PROFILE_TYPE_INSTANT 'I'

#define MIST_BEGIN_PROFILE(cat, name) Mist_WriteProfileSample(Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_BEGIN));
#define MIST_END_PROFILE(cat, name) Mist_WriteProfileSample(Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_END));
#define MIST_PROFILE_EVENT(cat, name) Mist_WriteProfileSample(Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_INSTANT));

#endif /* __MIST_PROFILER_H */
