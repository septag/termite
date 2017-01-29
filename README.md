# Termite: A Lightweight multiplatform game library
v0.3

_termite_ is a collection of multiplatform tools and libraries that can be used to make games and realtime applications. Currently it can not be defined as _engine_, It's more of a framework for making simple games. It's rather low-level, fast, memory and cache efficient and runs in different platforms.  

## Features and Components
- A Fast Multiplatform Base library (mostly from _bx_), various memory allocators, containers, hash tables, threading, vector math, SIMD math, sockets, logger, json parsing, etc..
- 2D Camera with zoom/pan. Simple First Person 3D Camera
- Command System for making Undo/Redo system in editors
- Error Reporting
- Event Dispatcher (loading, timers, ...)
- Debug Drawing 2D/3D
- Plugin System
- Bitmap Fonts
- 3D Model Import with animations and skinning
- Resource manager with Async read and Hot-Loading support
- Fiber based multi-threaded task manager (Naghty Dog GDC 2015)
- Tag based Memory pools (Naghty Dog GDC 2015)
- Progressive Scene Loader
- Sprites and Texture animations with support for [TexturePacker](https://www.codeandweb.com/texturepacker) spritesheets


## Supported Platforms
- Windows: Tested on Windows10 + Visual Studio 2015
- Linux: Tested on Ubuntu
- MacOS: Tested on MacOS Sierra
- Android: Tested on KitKat 4.4
- OSX: WIP

## Build
_Termite_ uses [cmake](https://cmake.org) build system, so you have to install cmake on your host computer in order be able to build. 
I've also prebuilt some dependencies like _bgfx_ for most platforms, and some more 3rdparties for windows platform, And they are downloaded from my server upon cmake build. But I have built the 64bit binaries for PC and ARM binaries for Android/iOS. If you want other architectures on a specific platforms you can always download the code and build them yourself.

### Windows/Linux/MacOS
Use cmake and a configuration name, note that configuration name is mandatory, there are four available configurations:

- _Debug_ Enable debugging that implies development features too. (defines *termite_DEV*)
- _Release_ Fastest build with no Developer features
- _Development_: Defines *termite_DEV=1*  in all sources and enables developer features like hot-loading and editor mode. This build listens on default port 6006 to pass debugging information to tools.
- _Profile_: Enables profiling features, uses release compile options but with profile sampling injected into code. This build also listens on default port 6006 to pass profile information to tools.

On different platforms you'll need to install some 3rdparty libraries:

- **Windows**: All needed pre-built libraries will be downloaded automcatically from my server for Visual Studio 2015 (x64 binaries only) on build. If you have a different VC version you have to download or build them yourself. See _Open Source Libraries_ section at the bottom of this document. And install *bgfx*, *assimp*, *libuv*, *SDL2* under ```deps``` directory.
- **Ubuntu**: Install the following packages: ```sudo apt install libassimp-dev libuv1-dev libsdl2-dev```. Bgfx will be downloaded for 64bit linux machines automatically.
- **MacOS**: Install the following packages using brew: ```brew install assimp libuv sdl2```. Bgfx will be downloaded for 64bit linux machines automatically.

For example to build Debug build, run following commands within _termite_ root project directory (linux/gnu make): 

```
mkdir .build  
cd .build  
cmake .. -DCMAKE_BUILD_TYPE=Debug  
make  
```

For _Visual Studio_ under windows you can use ```cmake .. -DCMAKE_BUILD_TYPE=Debug -G "Visual Studio 14 2015 Win64"``` and open the generated solution file.

### Android
Android uses a different toolchain that is also included with the project, so you have to specify it for cmake.  
Before that download and install _Android NDK_ and _Android SDK_. Define Environment variable *ANDROID_NDK* path to your NDK directory root, and run the following commands within _termite_ root project directory:  

```
mkdir .build  
cd .build  
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/android.toolchain.cmake -DANDROID_ABI="armeabi-v7a with NEON" -DANDROID_NATIVE_API_LEVEL=android-19 -DANDROID_STL=gnustl_static -DANDROID_STL_FORCE_FEATURES=0 -DCMAKE_BUILD_TYPE=Release -GNinja
```

In the example above, I built for _armv7a_ architecture in ```-DANDROID_ABI="armeabi-v7a with NEON"```.  

```-DANDROID_NATIVE_API_LEVEL=android-19``` defines the API version number, which in here I used kitkat 4.4 (19) API.  
```-DCMAKE_BUILD_TYPE=Release``` defines that we want to build with Release compile flags. See Above for other configuration names.  
```-GNinja``` Defines that we want to use _Ninja_ build system. which the executable also resides in ```deps/bx/tools/bin```. You can use different build systems.  
For more info on android build, read [this](https://github.com/taka-no-me/android-cmake).

## Test
Currently there are two test apps and sources are under ```tests``` directory:

- *test_init*: The most basic test, tests initialization and shutdown and briefly tests task manager.
- *test_sdl*: Test a basic graphics/window initialization and some basic debug drawing. In order to run this test without errors, you should run it under _termite_ root folder as working directory. Because it searches for fonts under ```termite/assets``` directory and may throw error if not found.

## Open Source Libraries
- [__Bgfx__](https://github.com/bkaradzic/bgfx) Low-level graphics.
- [__libuv__](https://github.com/libuv/libuv) AsyncIO
- [__SDL2__](https://www.libsdl.org) Window and input
- [__NanoVg__](https://github.com/memononen/nanovg) Vector based debug drawing
- [__ImGui__](https://github.com/ocornut/imgui) Debugging and tools GUI
- [__Assimp__](http://www.assimp.org/) 3D model importing
- [__bx__](https://github.com/bkaradzic/bx) A modified version of Branimir Karadžić's base library used as base
- [__Box2D__](http://box2d.org) 2D Physics
- [__OpenAL Soft__](http://kcat.strangesoft.net/openal.html) Audio
- [__stb__](https://github.com/nothings/stb) Sean Barrett's single header libraries 
- [__deboost.context__](https://github.com/septag/deboost.context) Boost's _deboostified_ fiber library (without boost dependency of course)
