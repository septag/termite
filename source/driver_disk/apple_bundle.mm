#import <Foundation/Foundation.h>
#include "bx/bx.h"
#include "bxx/path.h"

NSMutableArray* gBundles = nil;

int iosAddBundle(const char* bundleName)
{
    NSBundle *bundle = [NSBundle bundleWithPath:[[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:bundleName] ofType:nil]];
    if (!bundle)
        return 0;
    
    if (!gBundles) {
        gBundles = [NSMutableArray arrayWithObject:bundle];
    }   else    {
        [gBundles addObject:bundle];
    }
    
#if 0
    // TEMP: For debugging
    NSError * error;
    NSString * resourcePath = [bundle resourcePath];
    //NSString * documentsPath = [resourcePath stringByAppendingPathComponent:@""];
    NSArray * directoryContents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:resourcePath error:&error];
#endif
    
    return (int)gBundles.count - 1;
}

bx::Path iosResolveBundlePath(int bundleId, const char* filepath)
{
    if (gBundles)  {
        bx::Path pfilepath(filepath);
        bx::Path filename = pfilepath.getFilename();
        bx::Path fileext = pfilepath.getFileExt();
        bx::Path bundlePath = pfilepath.getDirectory();
        bundlePath.join(filename.cstr());

        assert(bundleId >= 0 && bundleId < gBundles.count);
        NSUInteger index = (NSUInteger)bundleId ;
        NSBundle* bundle = [gBundles objectAtIndex:index];
        
        NSString* path = [bundle pathForResource:[NSString stringWithUTF8String:bundlePath.cstr()] ofType:[NSString stringWithUTF8String:fileext.cstr()]];
        if (path)
            return bx::Path([path UTF8String]);
        else
            return bx::Path();
    }   else    {
        return bx::Path();
    }
}

