# ReadFileAccel

Read File Acceleration is a hobby project to measure and experiment with new methods to read data into memory from a disk faster.

The experiments are performed on an existing program that has to read a lot of data. A good example program is a game. A modern game needs to read gigabytes of data in a very short amount of time; like, interactive rates are between 16 milliseconds and 33 milliseconds per-frame, 60 and 30 frames per-second respectively.

This work is a bottom-up approach where a system function (ReadFile) in another process is replaced with a hook function. The hook functions written so far include:

* A function (ReadFileTrace) to measure how long in microseconds a read took to complete,
* A function (ReadFileCache) to write frequently read data into an in-memory cache,
* A function (ReadFileDirectStorage) to batch reads into a single request, and
* A function (ReadFileDecompress) to read compressed data.

## Analysis
There is a python script to analyze read file performance. The script can plot graphs and calculate simple metrics.

## Log File Format
A log file (xxxyyyzzz.txt) is written next to the Injector exe file. The log messages are written to a log file in this format:

`"ThreadId", "Time", "Event", "Path", "Position (bytes)", "Size (bytes)", "Elapsed Time (microseconds)", ...`

## Todo
A list of items to work on next:

* Investigate quartiles to more easily compare distribution plots
* Investigate creating a D3D12 device in Injector and passing to hook library
* Alt. Investigate hooking into the function to create a D3D device
* Investigate replacing hook library with Microsoft’s Detours
* Create a —no-input mode and a —process-id argument for Injector
* Create a script to start a target process ,get its process id and then run Injector.
* Create a crash hook function to test writing a crash log.
* Creat a prediction hook function to predict reads into the future and have data loaded ahead of time.
* Create a hook router to route to a different hook function by file path rules.
* Create a program to replay read file events.
* Create event structure and function a log event function.
* Create a web server for real-time analysis

# DLL Injector

Use this code to inject a DLL into a process.

Enter process-id (pid) to the program and the DLL will be injected!

Set the DLL path by changing the the define `DLL_PATH` in `main.c` in the injector folder.

A simple DLL that opens a `MessageBox` is also attached to illustrate the injection.

Read more at [Wikipedia](https://en.wikipedia.org/wiki/DLL_injection).

#### Example

```
$ Injector.exe
Enter pid: 24516
Got LoadLibraryA's address: 00007FF9522A04F0
Wrote path "C://Projects/C/DLLInjector/MyDLL/MyDLL.dll" successfully to address 000001F61B8E0000
Created thread with handle 0000000000000088
Successfuly injected the DLL into the process!
```





