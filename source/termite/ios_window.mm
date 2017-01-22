#include <UIKit/UIKit.h>

void* iosGetNativeLayer(void* wnd)
{
    UIWindow* uiwnd = (__bridge_transfer UIWindow*)wnd;
    //CALayer layer =uiwnd.rootViewController.view.layer;
    
    return (__bridge void*)uiwnd.rootViewController.view.layer;
}
