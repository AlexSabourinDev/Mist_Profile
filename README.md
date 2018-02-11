# Mist_Profiler
### About:
The Mist_Profiler is a single header utility that generates [chrome://tracing](chrome://tracing) json that can be then imported and viewed using chrome's tracing utility. 
Mist_Profiler is completely thread safe and attempts to minimize contention between thread.

### Usage:
Using `Mist_Profiler.h` is simple,
Before any use of the profiler add `MIST_PROFILE_DEFINE_GLOBALS`
to the top of a cpp file and call

```C
	Mist_ProfilerInit();
```

And at the end of your program execution call
```C
	Mist_ProfilerTerminate();
```
**Warning:** No other calls to the profiler should be made after terminate has been invoked!

To gather samples for the profiler, simply call

```C
	MIST_BEGIN_PROFILE("Category Of Sample", "Sample Name");

	// ...

	MIST_END_PROFILE("Category Of Sample", "Sample Name");
```

[chrome://tracing](chrome://tracing) matches these calls by name and category so determining a unique name for these samples and categories is important
to generate informative profiling data.

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

	char* print = Mist_Flush();

	// Note the comma here, 
	// this is to assure that the current set of samples are appended to the previous set. 
	// This comma is required in the final JSON document.
	fprintf(fileHandle, ",%s", print);

	free(print);
```
**Warning:** Flushing many samples can be quite slow, the size of the buffer can either be minimized or
flushing on a seperate thread can be used to minimize the latency.

Finally, when printing out the buffer, the set of samples must include the preface and postface.
```C
	fprintf(fileHandle, "%s", mist_ProfilePreface);
	fprintf(fileHandle, ",%s", flushedSamples);
	fprintf(fileHandle, ",%s", moreFlushedSamples);
	fprintf(fileHandle, "%s", mist_ProfilePostface);
```

### Threading:

It is important to call `Mist_FlushThreadBuffer()` before shutting down a thread and 
before the last flush as the remaining buffers might have some samples left in them.

A multithreaded program could have the format:
```
	Define Globals
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
```

##### Tested Compilers
MSVC 2017

##### Supported Platforms
Windows

##### Planned Support
Unix,
Mac OS,
GCC,
Clang

#### Sample Program
```C
#include <stdlib.h>
#include <stdio.h>

MIST_PROFILE_DEFINE_GLOBALS
int main(int argc, char** argv)
{
	MIST_UNUSED(argc);
	MIST_UNUSED(argv);

	Mist_ProfileInit();

	FILE* file;
	fopen_s(&file, "Profiling.txt", "w");

	MIST_BEGIN_PROFILE("Full Profile", "Profiling...");
	for (int i = 0; i < 1024; i++)
	{
		MIST_BEGIN_PROFILE("Profiling!", "Profiling!");
		MIST_END_PROFILE("Profiling!", "Profiling!");
		MIST_PROFILE_EVENT("Profiling!", "Event!");
	}

	MIST_BEGIN_PROFILE("Profiling!", "Flushing!");
	Mist_FlushThreadBuffer();
	char* buffer = Mist_Flush();
	MIST_END_PROFILE("Profiling!", "Flushing!");

	MIST_BEGIN_PROFILE("Profiling!", "Writing To File!");
	fprintf(file, "%s", mist_ProfilePreface);
	fprintf(file, ",%s", buffer);
	free(buffer);
	MIST_END_PROFILE("Profiling!", "Writing To File!");

	MIST_END_PROFILE("Full Profile", "Profiling...");

	Mist_FlushThreadBuffer();
	buffer = Mist_Flush();

	fprintf(file, ",%s", buffer);
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
https://www.gamasutra.com/view/news/176420/Indepth_Using_Chrometracing_to_view_your_inline_profiling_data.php and
https://aras-p.info/blog/2017/01/23/Chrome-Tracing-as-Profiler-Frontend/
For providing the information needed to implement this library.