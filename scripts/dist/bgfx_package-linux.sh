# Put this script under packages directory, bgfx directory should be one level up
# Make sure you have built the bgfx with release and exe (x64) configurations
curdir=$(pwd)

echo "Configuring Bgfx ..."
cd ../bgfx
genie --with-tools --with-dynamic-runtime --with-shared-lib --gcc=linux-gcc gmake

echo "Building Debug64 ..." 
make linux-debug64

echo "Building Release64 ..."
make linux-release64

echo "Packaging files ..."
cd $curdir

mkdir -p bgfx/bin
mkdir -p bgfx/include
mkdir -p bgfx/include/shader
mkdir -p bgfx/lib

cp ../bgfx/.build/linux64_gcc/bin/texturecRelease bgfx/bin/texturec
cp ../bgfx/.build/linux64_gcc/bin/shadercRelease bgfx/bin/shaderc
cp ../bgfx/.build/linux64_gcc/bin/texturevRelease bgfx/bin/texturev

cp ../bgfx/.build/linux64_gcc/bin/libbgfx-shared-libRelease.so bgfx/lib/libbgfxRelease.so
cp ../bgfx/.build/linux64_gcc/bin/libbgfx-shared-libDebug.so bgfx/lib/libbgfxDebug.so

cp -r ../bgfx/include/bgfx bgfx/include
cp ../bgfx/src/*.sh bgfx/include/shader

tar cf bgfx_linux64.tar.gz bgfx
rm -rf bgfx

echo "Ok"
