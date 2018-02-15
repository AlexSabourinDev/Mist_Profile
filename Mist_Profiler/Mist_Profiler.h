#ifndef __MIST_PROFILER_H
#define __MIST_PROFILER_H

/*
Mist_Profiler Usage, License: MIT

About:
The Mist_Profiler is a single header utility that generates chrome:\\tracing json that can be then imported and viewed using chrome's
tracing utility. Mist_Profiler is completely thread safe and attempts to minimize contention between thread.

Usage:
Using Mist_Profiler is simple,

Before any use of the profiler
add MIST_PROFILE_DEFINE_GLOBALS to the top of a cpp file and call
{
	Mist_ProfilerInit();
}

And at the end of your program execution, call
{
	Mist_ProfilerTerminate();
}

Warning: No other calls to the profiler should be made after terminate has been invoked!

To gather samples for the profiler, simply call

{
	MIST_BEGIN_PROFILE("Category Of Sample", "Sample Name");

	// ...

	MIST_END_PROFILE("Category Of Sample", "Sample Name");
}

Chrome://tracing matches these calls by name and category so determining a unique name for these samples and categories is important
to generate informative profiling data.

Warning: Category and Name are not stored, their lifetime must exist either until program termination or until the next call to Mist_FlushThreadBuffer and Mist_Flush.

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

	char* print = Mist_Flush();

	// Note the comma here, this is to assure that the current set of samples are appended to the previous set. This comma is required in the final JSON document.
	fprintf(fileHandle, ",%s", print);

	free(print);
}

Finally, when printing out the buffer, the set of samples must include 
the preface and postface which complete the profiling json format
{
	fprintf(fileHandle, "%s", mist_ProfilePreface);
	fprintf(fileHandle, ",%s", flushedSamples);
	fprintf(fileHandle, ",%s", moreFlushedSamples);
	fprintf(fileHandle, "%s", mist_ProfilePostface);
}

Threading:

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
	Call Mist_Flush() to flush the remaining buffers

	Print to a file
	Add mist_ProfilePostface to the file

	Terminate the profiler
}

*/

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
#ifdef __cplusplus
	#define MIST_INLINE inline
#else
	#define MIST_INLINE static
#endif

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

MIST_INLINE void Mist_InitLock(Mist_Lock* lock)
{
#if MIST_WIN
	InitializeCriticalSection(lock);
#else
	#error "Mist_CreateLock not implemented!"
#endif
}

MIST_INLINE void Mist_TerminateLock(Mist_Lock* lock)
{
#if MIST_WIN
	DeleteCriticalSection(lock);
#else
	#error "Mist_DestroyLock not implemented!"
#endif
}

MIST_INLINE void Mist_LockSection(Mist_Lock* lock)
{
#if MIST_WIN
	EnterCriticalSection(lock);
#else
	#error "Mist_LockMutex not implemented!"
#endif
}

MIST_INLINE void Mist_UnlockSection(Mist_Lock* lock)
{
#if MIST_WIN
	LeaveCriticalSection(lock);
#else
	#error "Mist_UnlockMutex not implemented!"
#endif
}

MIST_INLINE uint16_t Mist_GetThreadID( void )
{
#if MIST_WIN
	return (uint16_t)GetCurrentThreadId();
#else
	#error "Mist_GetThreadId not implemented!"
#endif
}

MIST_INLINE uint16_t Mist_GetProcessID( void )
{
#if MIST_WIN
	return (uint16_t)GetProcessId(GetCurrentProcess());
#else
	#error "Mist_GetProcessID not implemented!"
#endif
}

/* -Timer- */

MIST_INLINE int64_t Mist_TimeStamp( void )
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

MIST_INLINE Mist_ProfileSample Mist_CreateProfileSample(const char* category, const char* name, int64_t timeStamp, char eventType)
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
extern MIST_THREAD_LOCAL Mist_ProfileBuffer mist_ProfileBuffer;


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
extern Mist_ProfileBufferList mist_ProfileBufferList;

MIST_INLINE void Mist_ProfileInit( void )
{
	Mist_InitLock(&mist_ProfileBufferList.lock);
}

/* Terminate musst be the last thing called, assure that profiling events will no longer be called once this is called */
MIST_INLINE void Mist_ProfileTerminate( void )
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

MIST_INLINE uint16_t Mist_ProfileListSize()
{
	return mist_ProfileBufferList.listSize;
}

/* Not thread safe */
MIST_INLINE void Mist_ProfileAddBufferToList(Mist_ProfileBuffer* buffer)
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
		assert(mist_ProfileBufferList.listSize != (~(uint16_t)0));
		assert(mist_ProfileBufferList.last->next == NULL);

		mist_ProfileBufferList.last->next = node;
		mist_ProfileBufferList.last = node;
		mist_ProfileBufferList.listSize++;
	}
}

/* Returns a string to be printed, this takes time. */
/* Thread safe */
/* The returned string must be freed. */
MIST_INLINE char* Mist_Flush( void )
{
	Mist_ProfileBufferNode* start;
	Mist_LockSection(&mist_ProfileBufferList.lock);

	start = mist_ProfileBufferList.first;
	mist_ProfileBufferList.first = NULL;
	mist_ProfileBufferList.last = NULL;
	mist_ProfileBufferList.listSize = 0;

	Mist_UnlockSection(&mist_ProfileBufferList.lock);


	char* print = NULL;
	if (start == NULL)
	{
		/* Give an empty string */
		print = (char*)malloc(3);
		print[0] = '{';
		print[1] = '}';
		print[2] = 0;
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
	print[size] = '\0';

	return print;
}
static const char* mist_ProfilePreface = "{\"traceEvents\":[{},";
static const char* mist_ProfilePostface = "{}]}";

/* Thread safe */
MIST_INLINE void Mist_WriteProfileSample(Mist_ProfileSample sample)
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
MIST_INLINE void Mist_FlushThreadBuffer( void )
{
	Mist_LockSection(&mist_ProfileBufferList.lock);

	Mist_ProfileAddBufferToList(&mist_ProfileBuffer);
	mist_ProfileBuffer.nextSampleWrite = 0;

	Mist_UnlockSection(&mist_ProfileBufferList.lock);
}


/* -API- */

#define MIST_PROFILE_TYPE_BEGIN 'B'
#define MIST_PROFILE_TYPE_END 'E'
#define MIST_PROFILE_TYPE_INSTANT 'I'

#define MIST_BEGIN_PROFILE(cat, name) Mist_WriteProfileSample(Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_BEGIN));
#define MIST_END_PROFILE(cat, name) Mist_WriteProfileSample(Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_END));
#define MIST_PROFILE_EVENT(cat, name) Mist_WriteProfileSample(Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_INSTANT));

#define MIST_PROFILE_DEFINE_GLOBALS \
	Mist_ProfileBufferList mist_ProfileBufferList; \
	MIST_THREAD_LOCAL Mist_ProfileBuffer mist_ProfileBuffer;

#endif /* __MIST_PROFILER_H */
