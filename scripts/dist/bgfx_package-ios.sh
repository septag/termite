# Put this script under packages directory, bgfx directory should be one level up
# Make sure you have built the bgfx with release and exe (x64) configurations
curdir=$(pwd)

ios_ver=${1}
if [ -z $ios_ver ]; then
echo "Provide iOS version as an argument (example: 10.0)"
exit
fi

echo "Configuring Bgfx ..."
cd ../bgfx
genie --with-ios=${ios_ver} --platform=ARM --gcc=ios-arm gmake

echo "Building Debug64 ..." 
make ios-arm-debug

echo "Building Release64 ..."
make ios-arm-release

echo "Packaging files ..."
cd $curdir

mkdir -p bgfx_ios_arm/include
mkdir -p bgfx_ios_arm/include/shader
mkdir -p bgfx_ios_arm/lib

cp ../bgfx/.build/ios-arm/bin/libbgfxRelease.a bgfx_ios_arm/lib
cp ../bgfx/.build/ios-arm/bin/libbgfxDebug.a bgfx_ios_arm/lib

cp -r ../bgfx/include/bgfx bgfx_ios_arm/include
cp ../bgfx/src/*.sh bgfx_ios_arm/include/shader

tar cf bgfx_ios_arm.tar.gz bgfx_ios_arm
rm -rf bgfx_ios_arm

echo "Ok"
