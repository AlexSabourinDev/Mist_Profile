# Mist_Profiler
### About:
The Mist_Profiler is a single header utility that generates [chrome://tracing](chrome://tracing) json that can be then imported and viewed using chrome's tracing utility. 
Mist_Profiler is completely thread safe and attempts to minimize contention between thread.

### Usage:
Using `Mist_Profiler.h` is simple,
Include in one of your source files with define:
```C
	#define MIST_PROFILE_IMPLEMENTATION
	#include <Mist_Profiler.h>
```

Before profiling call:
```C
	Mist_ProfileInit();
```

And at the end of your program execution call
```C
	Mist_ProfilerTerminate();
```
**Warning:** No other calls to the profiler should be made after terminate has been invoked!

To gather samples for the profiler, simply call

```C
	MIST_PROFILE_BEGIN("Category Of Sample", "Sample Name");

	// ...

	MIST_PROFILE_END("Category Of Sample", "Sample Name");
```

[chrome://tracing](chrome://tracing) matches these calls by name and category. Determining a unique name for these is important.

**Warning:** `Category` and `Name` are not stored, their lifetime must exist either until program termination or until the next call to `Mist_FlushThreadBuffer` and `Mist_Flush`.

Once a significant amount of samples have been gathered, samples have to be flushed.
A simple way to do this is shown below
```C
	// Buffers are only added to the list once they are full.
	// This minimizes contention and allows simple modification of the buffers.
	// The buffers are also stored as thread local globals, and must be flushed from their threads.
	if(Mist_ProfileListSize() == 0)
	{
		// Adds the current buffer to the list of buffers even if it hasn't been filled up yet.
		Mist_FlushThreadBuffer();
	}

	char* print = Mist_FlushAlloc(); // Edit: Changed from Mist_Flush!! Mist_Flush now requires a buffer to be passed to it.
	fprintf(fileHandle, "%s", print);
	free(print);
```
**Warning:** Flushing many samples can be slow, the size of the buffer can either be minimized or
flushing on a seperate thread can be used to minimize the latency.

Finally, when printing out the buffer, the set of samples must include the preface and postface.
```C
	fprintf(fileHandle, "%s", mist_ProfilePreface);
	fprintf(fileHandle, "%s", flushedSamples);
	fprintf(fileHandle, "%s", moreFlushedSamples);
	fprintf(fileHandle, "%s", mist_ProfilePostface);
```

### Threading:

It is important to call `Mist_FlushThreadBuffer()` before shutting down a thread and 
before the last flush as the remaining buffers might have some samples left in them.

A multithreaded program could have the format:
```
	#define MIST_PROFILE_IMPLEMENTATION
	#include <Mist_Profiler.h>
	
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
```

##### Tested Compilers
- MSVC 2017

##### Supported Platforms
- Windows

##### Planned Support
- Unix
- Mac OS
- GCC
- Clang

#### Sample Program
```C
#include <stdlib.h>
#include <stdio.h>

#define MIST_PROFILE_IMPLEMENTATION
#include <Mist_Profiler.h>

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	Mist_ProfileInit();

	FILE* file;
	fopen_s(&file, "Profiling.txt", "w");

	MIST_PROFILE_BEGIN("Full Profile", "Profiling...");
	for (int i = 0; i < 1024; i++)
	{
		MIST_PROFILE_BEGIN("Profiling!", "Profiling!");
		MIST_PROFILE_END("Profiling!", "Profiling!");
		MIST_PROFILE_EVENT("Profiling!", "Event!");
	}

	MIST_PROFILE_BEGIN("Profiling!", "Flushing!");
	Mist_FlushThreadBuffer();
	char* buffer = Mist_FlushAlloc();
	MIST_PROFILE_END("Profiling!", "Flushing!");

	MIST_PROFILE_BEGIN("Profiling!", "Writing To File!");
	fprintf(file, "%s", mist_ProfilePreface);
	fprintf(file, "%s", buffer);
	free(buffer);
	MIST_PROFILE_END("Profiling!", "Writing To File!");

	MIST_PROFILE_END("Full Profile", "Profiling...");

	Mist_FlushThreadBuffer();
	buffer = Mist_FlushAlloc();

	fprintf(file, "%s", buffer);
	fprintf(file, "%s", mist_ProfilePostface);
	free(buffer);

	fclose(file);
	Mist_ProfileTerminate();
	return 0;
}
```

##### Visualized Capture:
![alt text](https://github.com/AlexSabourinDev/Mist_Profile/blob/master/Example.PNG "Example Profile")

##### Thanks
Big thanks to
https://www.gamasutra.com/view/news/176420/Indepth_Using_Chrometracing_to_view_your_inline_profiling_data.php
https://aras-p.info/blog/2017/01/23/Chrome-Tracing-as-Profiler-Frontend/
and the team working on chrome://tracing or providing the tools and information needed to implement this library.

#### Change Log
- 2019-01-13: Changed Mist_Flush to Mist_FlushAlloc. Mist_Flush now requires an explicit buffer. This will break previous usage of the profiler.
- 2019-01-13: Changed MIST_BEGIN/END_PROFILE to MIST_PROFILE_BEGIN/END. Will break previous calls to old macros.
- 2018-02-14: Removed the need for a comma to append the buffers. This will break previous usage of the profiler but since it isn't a week old, I believe this is fine.
