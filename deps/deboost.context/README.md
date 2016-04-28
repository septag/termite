## deboost.context
"Deboostified" version of boost.context (coroutines), Plain and simple C API for context switching. Easy build on multiple platforms.  

### Build
#### Currently supported platforms 
- Windows (x86_64, Win32)
- Linux (x86_64/x86)
- More will be added soon ...

#### Linux/Unix
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

### Usage
See _include/fcontext/fcontext.h_ for API usage.  
More info is available at: [boost.context](http://www.boost.org/doc/libs/1_60_0/libs/context/doc/html/index.html)

### Credits
- Boost.context: This library uses the code from boost.context [github](https://github.com/boostorg/context)

### Acknowledgements
- Ali Salehi (nysalehi@gmail.com) 

