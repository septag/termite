# Termite: A Lightweight multiplatform game framework
v0.4

_termite_ is a collection of multiplatform tools and libraries that can be used to make games and realtime applications. Currently it can not be defined as _engine_, It's more of a framework for making simple games from scratch. It's rather low-level, fast and memory efficient and runs in different platforms.  
My strategy is to provide various low-level building blocks that is used to build higher level games and tools.  

## Features and Components
- A Fast Multiplatform Base library (forked from _bx_), various memory allocators, containers, hash tables, threading, vector math, SIMD math, sockets, logger, json parsing, etc..
- 2D Camera with zoom/pan. Simple First Person 3D Camera
- Command System for making Undo/Redo system in tools pipeline
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
- Simple 2D sound (wav/ogg) support through SDL_mixer
- ImGui wrapper/renderer for tools development
- General purpose Scene-Management state machine for load/unload/transitioning between multiple scenes.
- Remotery profiler integration
- Box2D physics wrapper

Here is a shot from my own game _fisherboy_ editor running on _termite_:  
![Fisherboy](https://raw.githubusercontent.com/septag/termite/master/wiki/img/fisherboy.jpg)


## Supported Platforms
Note that the engine is in heavy development, and some of them may be broken on latest commits. Currently I'm developing my game on Windows/android/iOS, so these platforms will likely build successfully.  

- Windows: Tested on Windows10 + Visual Studio 2015
- Linux: Tested on Ubuntu
- MacOS: Tested on MacOS Sierra
- Android: Tested on KitKat 4.4
- iOS: Tested on iOS 10.0

## Build
_Termite_ uses [cmake](https://cmake.org) build system, so you have to install cmake on your host computer in order be able to build. 
I've also prebuilt some dependencies like _SDL2_ and _libcurl_ for some platforms that doesn't have them as a package.

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
Recommended solution is to use Android studio (with cmake support) to build your project.

### iOS
iOS build uses a custom [https://github.com/leetal/ios-cmake](toolchain file) to make iOS project files. 

```
mkdir build-ios
cd build-ios
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake -DIOS_PLATFORM=OS -DIOS_DEPLOYMENT_TARGET=9.0 -G Xcode
```

- *IOS_DEPLOYMENT_TARGET*: minimum SDK version that you wish to target for. For now you can't set it less than 9.0, because the curl library is built on that requirement (some build fixes needed to build for 8.0)

Check out ```cmake/ios.toolchain.cmake``` to see more iOS specific options.

## Test
Currently there are two test apps and sources are under ```tests``` directory:

- *test_init*: The most basic test, tests initialization and shutdown and briefly tests task manager.
- *test_sdl*: Test a basic graphics/window initialization and some basic debug drawing. In order to run this test without errors, you should run it under _termite_ root folder as working directory. Because it searches for fonts under ```termite/assets``` directory and may throw error if not found.

## Open Source Libraries
- [__Bgfx__](https://github.com/bkaradzic/bgfx) Low-level graphics.
- [__SDL2__](https://www.libsdl.org) Window and input
- [__SDL_mixer__](https://www.libsdl.org/projects/SDL_mixer/) 2D sound library
- [__NanoVg__](https://github.com/memononen/nanovg) Vector based debug drawing
- [__ImGui__](https://github.com/ocornut/imgui) Debugging and Tools GUI
- [__Assimp__](http://www.assimp.org/) 3D model importing
- [__bx__](https://github.com/bkaradzic/bx) A modified version of Branimir Karadžić's base library
- [__Box2D__](http://box2d.org) 2D Physics
- [__OpenAL Soft__](http://kcat.strangesoft.net/openal.html) Audio
- [__stb__](https://github.com/nothings/stb) Sean Barrett's single header libraries 
- [__deboost.context__](https://github.com/septag/deboost.context) Boost's _deboostified_ fiber library (without boost dependency of course)
- [__remotery__](https://github.com/Celtoys/Remotery) Remotery remote profiler
- [__Box2D__](http://box2d.org/) 2D Physics library

## License
[BSD 2-clause](https://github.com/septag/termite/blob/master/LICENSE)
