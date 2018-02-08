#ifndef __MIST_PROFILER_H
#define __MIST_PROFILER_H

// -Platform Macros-

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__)
	#define MIST_WIN 1
#elif defined(macintosh) || defined(Macintosh) || defined(__APPLE__) && defined(__MACH__)
	#define MIST_MAC
#elif defined(__unix__) || defined(__unix)
	#define MIST_UNIX
#endif

#include <stdint.h>
#include <assert.h>

#if MIST_WIN
#include <Windows.h>
#endif

#define MIST_UNUSED(a) (void)a

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
	#error "OS timer has not been implemented!"
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
	sample.processorID = 0;
	sample.threadID = 0;
	sample.eventType = eventType;
	return sample;
}

#define MIST_BUFFER_SIZE 1024

typedef struct
{
	Mist_ProfileSample samples[MIST_BUFFER_SIZE];
	uint16_t nextSampleWrite;

} Mist_ProfileBuffer;

static void Mist_WriteProfileSample(Mist_ProfileBuffer* buffer, Mist_ProfileSample sample)
{
	MIST_UNUSED(buffer);
	MIST_UNUSED(sample);
	/* TOTO */
}

typedef struct
{
	Mist_ProfileBuffer activeBuffer;


} Mist_ProfileBufferList;



/* -API- */

#define MIST_PROFILE_TYPE_BEGIN "B"
#define MIST_PROFILE_TYPE_END "E"
#define MIST_PROFILE_TYPE_INSTANT "I"

#define MIST_BEGIN_PROFILE(cat, name) Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_BEGIN);
#define MIST_END_PROFILE(cat, name) Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_END);
#define MIST_PROFILE_EVENT(cat, name) Mist_CreateProfileSample(cat, name, Mist_TimeStamp(), MIST_PROFILE_TYPE_INSTANT);

#endif /* __MIST_PROFILER_H */
