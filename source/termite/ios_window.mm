#include <UIKit/UIKit.h>
#include "vec_math.h"

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

termite::vec2_t iosGetScreenSize()
{
    CGRect screenBounds = [[UIScreen mainScreen] bounds];
    CGFloat screenScale = [[UIScreen mainScreen] scale];
    
    return termite::vec2f(screenBounds.size.width*screenScale, screenBounds.size.height*screenScale);
}
