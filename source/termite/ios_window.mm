#include <UIKit/UIKit.h>
#include <AVFoundation/AVFoundation.h>
#include "tmath.h"
#include <mach/mach_host.h>

#include "bxx/path.h"

void* iosCreateNativeLayer(void* wnd)
{
    UIWindow* uiwnd = (__bridge UIWindow*)wnd;
    
    // To create a sublayer (without SDL modification)
    CAMetalLayer* mlayer = [CAMetalLayer layer];
    CALayer* layer = uiwnd.rootViewController.view.layer;
    [layer addSublayer:mlayer];
    mlayer.contentsScale = [[UIScreen mainScreen] scale];
    mlayer.framebufferOnly = true;
    
    mlayer.frame = layer.bounds;
    return (__bridge void*)mlayer;
}

tee::vec2_t iosGetScreenSize()
{
    CGRect nativeBounds = [[UIScreen mainScreen] nativeBounds];
    
    return tee::vec2(nativeBounds.size.width, nativeBounds.size.height);
}

void iosTurnOnAudioSession()
{
    [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayback withOptions:AVAudioSessionCategoryOptionMixWithOthers error:nil];
    [[AVAudioSession sharedInstance] setActive:true error:nil];
}

uint8_t iosGetCoreCount()
{
    return [[NSProcessInfo processInfo] processorCount];
}

void iosGetCacheDir(bx::Path* pPath)
{
    NSString* cachePath = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) lastObject];
    *pPath = [cachePath UTF8String];
}

void iosGetDataDir(bx::Path* pPath)
{
    NSString* cachePath = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) lastObject];
    *pPath = [cachePath UTF8String];
}
