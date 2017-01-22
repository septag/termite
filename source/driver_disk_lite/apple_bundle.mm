#import <Foundation/Foundation.h>
#include "bx/bx.h"
#include "bxx/path.h"

NSMutableArray* g_bundles = nil;

int iosAddBundle(const char* bundleName)
{
    NSBundle *bundle = [NSBundle bundleWithPath:[[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:bundleName] ofType:@"bundle"]];
    if (!bundle)
        return 0;
    
    if (!g_bundles) {
        g_bundles = [NSMutableArray arrayWithObject:bundle];
    }   else    {
        [g_bundles addObject:bundle];
    }
    
    return (int)g_bundles.count - 1;
}

bx::Path iosResolveBundlePath(int bundleId, const char* filepath)
{
    if (g_bundles)  {
        bx::Path pfilepath(filepath);
        bx::Path filename = pfilepath.getFilename();
        bx::Path fileext = pfilepath.getFileExt();

        assert(bundleId >= 0 && bundleId < g_bundles.count);
        NSUInteger index = (NSUInteger)bundleId ;
        NSBundle* bundle = [g_bundles objectAtIndex:index];
        
        NSString* path = [bundle pathForResource:[NSString stringWithUTF8String:filename.cstr()] ofType:[NSString stringWithUTF8String:fileext.cstr()]];
        if (path)
            return bx::Path([path UTF8String]);
        else
            return bx::Path();
    }   else    {
        return bx::Path();
    }
}

