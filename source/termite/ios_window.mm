#include <UIKit/UIKit.h>
#include <AVFoundation/AVFoundation.h>
#include "tmath.h"
#include <mach/mach_host.h>
#include <SDL_uikitappdelegate.h>

void* iosCreateNativeLayer(void* wnd)
{
    UIWindow* uiwnd = (__bridge UIWindow*)wnd;
    
    // To create a sublayer (without SDL modification)
    CAEAGLLayer* glLayer = [CAEAGLLayer layer];
    CALayer* layer = uiwnd.rootViewController.view.layer;
    [layer addSublayer:glLayer];
    glLayer.contentsScale = [[UIScreen mainScreen] scale];
    
    glLayer.frame = layer.bounds;
    return (__bridge void*)glLayer;
}

tee::vec2_t iosGetScreenSize()
{
    CGRect screenBounds = [[UIScreen mainScreen] bounds];
    CGFloat screenScale = [[UIScreen mainScreen] scale];
    
    return tee::vec2(screenBounds.size.width*screenScale, screenBounds.size.height*screenScale);
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

@implementation SDLUIKitDelegate(MyMethods)
- (BOOL)application:(UIApplication *)application continueUserActivity:(NSUserActivity *)userActivity
                    restorationHandler:(void (^)(NSArray *restorableObjects))restorationHandler
{
    if (userActivity.activityType == NSUserActivityTypeBrowsingWeb) {
        NSURL* url = userActivity.webpageURL;
        NSString* s = [url absoluteString];
        const char* curl = [s UTF8String];
        puts(curl);
    }
    return TRUE;
}
@end
