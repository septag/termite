## deboost.context
"Deboostified" version of boost.context (coroutines), Plain and simple C API for context switching. Easy build on multiple platforms.  

### Build
#### Currently supported platforms 
- Windows (x86_64, Win32)
- Linux (x86_64/x86)
- OSX (x86_64/x86)
- Android (ARM/x86/ARM64/x86_64)
- iOS (Arm64, Arm7, x86_64, i386)

#### iOS
I've made an extra xcode project files for iOS ```projects/xcode/fcontext``` because I didn't know how to set different ASM files for each ARM architecture in cmake. So If you know how to do it, I'd be happy if you tell me.  
So, you can use the included toolchain file or use your own, just define _IOS_ to include the xcode project instead of generating it with cmake.
```
cd deboost.context
mkdir .build
cd .build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake -G Xcode
```


### Usage
Link your program with fcontext.lib/libfcontext.a and include the file _fcontext.h_.  
See _include/fcontext/fcontext.h_ for API usage.  
More info is available at: [boost.context](http://www.boost.org/doc/libs/1_60_0/libs/context/doc/html/index.html)

### Credits
- Boost.context: This library uses the code from boost.context [github](https://github.com/boostorg/context)

### Thanks
- Ali Salehi [github](https://github.com/lordhippo)

