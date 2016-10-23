# Put this script under packages directory, bgfx directory should be one level up
# Make sure you have built the bgfx with release and exe (x64) configurations
curdir=$(pwd)

android_api=${1}
if [ -z $android_api ]; then
    echo "Provide Android API version as an argument (example: 19)"
    exit
fi

echo "Configuring Bgfx ..."
cd ../bgfx
genie --gcc=android-arm --platform=ARM --with-dynamic-runtime --with-android=${android_api} gmake

echo "Building ArmDebug ..." 
make android-arm-debug

echo "Building ArmRelease ..."
make android-arm-release

echo "Packaging files ..."
cd $curdir

mkdir -p bgfx_android_arm/include
mkdir -p bgfx_android_arm/include/shader
mkdir -p bgfx_android_arm/lib

cp ../bgfx/.build/android-arm/bin/libbgfxRelease.a bgfx_android_arm/lib
cp ../bgfx/.build/android-arm/bin/libbgfxDebug.a bgfx_android_arm/lib

cp -r ../bgfx/include/bgfx bgfx_android_arm/include
cp ../bgfx/src/*.sh bgfx_android_arm/include/shader

tar cf bgfx_android_arm.tar.gz bgfx_android_arm
rm -rf bgfx_android_arm 
echo "Ok"