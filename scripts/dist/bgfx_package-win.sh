# Put this script under packages directory, bgfx directory should be one level up
# Make sure you have built the bgfx with release and exe (x64) configurations
mkdir -p bgfx/bin
mkdir -p bgfx/include
mkdir -p bgfx/include/shader
mkdir -p bgfx/lib

cp ../bgfx/.build/win64_vs2015/bin/texturecRelease.exe bgfx/bin/texturec.exe
cp ../bgfx/.build/win64_vs2015/bin/shadercRelease.exe bgfx/bin/shaderc.exe
cp ../bgfx/.build/win64_vs2015/bin/texturevRelease.exe bgfx/bin/texturev.exe

cp ../bgfx/.build/win64_vs2015/bin/bgfxRelease.lib bgfx/lib
cp ../bgfx/.build/win64_vs2015/bin/bgfxDebug.lib bgfx/lib
cp ../bgfx/.build/win64_vs2015/bin/bgfxDebug.pdb bgfx/lib

cp -r ../bgfx/include/bgfx bgfx/include
cp ../bgfx/src/*.sh bgfx/include/shader

tar cf bgfx_vc140.tar.gz bgfx