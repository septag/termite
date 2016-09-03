## deboost.context
"Deboostified" version of boost.context (coroutines), Plain and simple C API for context switching. Easy build on multiple platforms.  

### Build
#### Currently supported platforms 
- Windows (x86_64, Win32)
- Linux (x86_64/x86)
- OSX (x86_64/x86)
- Android (ARM/x86/ARM64/x86_64)
- More will be added soon ...

#### Linux/Unix/Mac
```
cd deboost.context
mkdir .build
cd .build
cmake ..
make
make install
```

#### Windows
Assuming you have visual studio installed on your system
```
cd deboost.context
mkdir .build
cd .build
cmake .. -G "Visual Studio 14 2015 Win64"
```
Now open the generated solution file and build

#### Android
Uses [android-cmake](https://github.com/taka-no-me/android-cmake) for Android builds. visit the page for help on command line options.  
It requres android NDK to build. extract the android NDK and set _ANDROID_NDK_ environment variable to the NDK root path.  
As an example if you want to build for Android ARMv7(Neon) 
```
cd deboost.context
mkdir .build
cd .build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/android.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI="armeabi-v7a with NEON" -DANDROID_STL="stlport_static" -DANDROID_NATIVE_API_LEVEL=android-19
```
On Windows, This command tries to make Visual Studio sln files. Which requires you to install [Nsight Visual Studio Edition](https://developer.nvidia.com/nsight-visual-studio-edition-downloads)  
If you wish to install without visual studio and Nsight on windows. You have to install [Ninja](https://ninja-build.org/) and Add it to your PATH. Then add ```-GNinja``` to your cmake command line.


### Usage
Link your program with fcontext.lib/libfcontext.a and include the file _fcontext.h_.  
See _include/fcontext/fcontext.h_ for API usage.  
More info is available at: [boost.context](http://www.boost.org/doc/libs/1_60_0/libs/context/doc/html/index.html)

### Credits
- Boost.context: This library uses the code from boost.context [github](https://github.com/boostorg/context)

### Acknowledgements
- Ali Salehi (nysalehi@gmail.com) 

