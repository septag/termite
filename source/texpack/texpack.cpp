#include "bx/bx.h"
#include "bx/commandline.h"
#include "bx/allocator.h"
#include "bx/file.h"
#include "bxx/path.h"
#include "bxx/array.h"
#include "bxx/logger.h"

#include "termite/types.h"
#include "termite/tmath.h"

#include "lz4/lz4.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include <stdio.h>

using namespace tee;

#ifdef _DEBUG
#define STB_LEAKCHECK_IMPLEMENTATION
#include "bxx/leakcheck_allocator.h"
static bx::LeakCheckAllocator gAllocStub;
#else
static bx::DefaultAllocator gAllocStub;
#endif
static bx::AllocatorI* gAlloc = &gAllocStub;

struct PackMode {
    enum Enum {
        XY_NORMAL_Z_HUE = 0,      // x,y=normal vector ; z=packed RG values, a=unchanged
        None
    };
};

struct ImageData
{
    bx::Path path;
    int w;
    int h;
    int comp;   // number of pixel components
    stbi_uc* srcPixels;
};

static PackMode::Enum getPackMode(const char* smode) 
{
    if (bx::strCmpI(smode, "XY_NORMAL_Z_HUE") == 0) {
        return PackMode::XY_NORMAL_Z_HUE;
    }

    return PackMode::None;
}

static const char* getPackModeStr(PackMode::Enum mode)
{
    switch (mode) {
    case PackMode::XY_NORMAL_Z_HUE:
        return "XY_NORMAL_Z_HUE";

    default:
        return "None";
    }
}

// Image #1) Normal map: extracts XY from normal map
// Image #2) Low quality color map: Extracts RG from color map and pack them into one channel with much lower precision
//           Alpha is also extracted from this image with original precision (unchanged)
static void packXyNormZRg(const ImageData* images, int numImages, const char* outputFilepath)
{
    if (numImages < 2) {
        BX_WARN("Must specify two images for this packing mode");
        return;
    }

    const ImageData& normImg = images[0];
    const ImageData& colorImg = images[1];

    if (normImg.w != colorImg.w || normImg.h != colorImg.h) {
        BX_WARN("Input images '%s' and '%s' must have identical dimensions", normImg.path.cstr(), colorImg.path.cstr());
        return;
    }

    BX_TRACE("Packing '%s' and '%s' -> '%s'", images[0].path.cstr(), images[1].path.cstr(), outputFilepath);

    int w = normImg.w;
    int h = normImg.h;
    uint8_t* normBuff = normImg.srcPixels;
    uint8_t* colorBuff = colorImg.srcPixels;
    uint8_t* destBuff = (uint8_t*)BX_ALLOC(gAlloc, w*h*4);
    if (!destBuff) {
        BX_WARN("Out of memory");
        return;
    }
    uint8_t* dest = destBuff;
    float hsv[3];

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            ucolor_t n = ucolor(*((uint32_t*)normBuff));
            ucolor_t c = ucolor(*((uint32_t*)colorBuff));

            // In colors, the only thing that is important to us is the HUE value (with S=1, V=1)
            // So extract the hue and put it in a single BLUE channel
            vec4_t colorf = tmath::ucolorToVec4(c);
            bx::rgbToHsv(hsv, colorf.f);
            float hh = hsv[0];

            ucolor_t o = ucolor(n.r, n.g, uint8_t(hh*255.0f), c.a);

            // decode
            /*
            const float px = bx::fabs(bx::ffract(hh + 1.0f) * 6.0f - 3.0f);
            const float py = bx::fabs(bx::ffract(hh + 2.0f/3.0f) * 6.0f - 3.0f);
            ucolor_t o = ucolorf(bx::clamp(px - 1.0f, 0.0f, 1.0f), bx::clamp(py - 1.0f, 0.0f, 1.0f), 0, color.w);
            */

            *((uint32_t*)dest) = o.n;

            normBuff += 4;
            colorBuff += 4;
            dest += 4;
        }
    }

    // Check output extension
    bx::Path opath(outputFilepath);
    bx::Path ext = opath.getFileExt();
    if (ext.isEqualNoCase("png")) {
        if (!stbi_write_png(opath.cstr(), w, h, 4, destBuff, 4*w)) {
            BX_FATAL("Writing '%s' failed", opath.cstr());
        }
    } else if (ext.isEqualNoCase("bmp")) {
        if (!stbi_write_bmp(opath.cstr(), w, h, 4, destBuff)) {
            BX_FATAL("Writing '%s' failed", opath.cstr());
        }
    } else if (ext.isEqualNoCase("tga")) {
        if (!stbi_write_tga(opath.cstr(), w, h, 4, destBuff)) {
            BX_FATAL("Writing '%s' failed", opath.cstr());
        }
    } else {
        if (!stbi_write_png(opath.cstr(), w, h, 4, destBuff, 4)) {
            BX_FATAL("Writing '%s' failed", opath.cstr());
        }
    }

    BX_FREE(gAlloc, destBuff);
}

static void showHelp() 
{
    BX_TRACE("");
}

int main(int argc, char* argv[])
{
    bx::CommandLine cmdline(argc, argv);
    const char* filepaths = cmdline.findOption('f', "file");
    const char* outputFilepath = cmdline.findOption('o', "out");
    const char* spackmode = cmdline.findOption('m', "mode");

    bx::enableLogToFileHandle(stdout, stderr);

    if (!filepaths || !outputFilepath || !spackmode) {
        BX_FATAL("-f, -o, -m Parameters must be set");
        showHelp();
        return -1;
    }

    // check valid packing modes
    PackMode::Enum packMode = getPackMode(spackmode);
    if (packMode == PackMode::None) {
        BX_FATAL("Invalid packing mode, Valid values are:\n");
        for (int i = 0; i < PackMode::None; i++) {
            BX_VERBOSE("\t%s\n", getPackModeStr((PackMode::Enum)i));
        }
        showHelp();
        return -1;
    }
    
   
    // tokenize input paths by ';'
    size_t filepathsSz = strlen(filepaths) + 1;
    char* tmpfilepaths = (char*)alloca(filepathsSz);
    BX_ASSERT(tmpfilepaths, "input paths too big");
    bx::memCopy(tmpfilepaths, filepaths, filepathsSz);

    bx::Array<ImageData> images;
    images.create(4, 16, gAlloc);
    {
        char* tok = strtok(tmpfilepaths, ";");
        while (tok) {
            bx::FileInfo finfo;
            if (bx::stat(tok, finfo) && finfo.m_type == bx::FileInfo::Regular) {
                ImageData* idata = images.push();
                bx::memSet(idata, 0x0, sizeof(ImageData));
                idata->path = tok;
            }
            tok = strtok(nullptr, ";");
        }
    }

    if (images.getCount() == 0) {
        BX_FATAL("No valid input file path found");
        BX_TRACE("");
        images.destroy();
        return -1;
    }        

    for (int i = 0; i < images.getCount(); i++) {
        // Open image 
        BX_VERBOSE("Loading: %s", images[i].path.cstr());
        images[i].srcPixels = stbi_load(images[i].path.cstr(), &images[i].w, &images[i].h, &images[i].comp, 4);
        if (!images[i].srcPixels) {
            BX_WARN("Could not load image '%s'", images[i].path.cstr());
        }
    }

    switch (packMode) {
    case PackMode::XY_NORMAL_Z_HUE:
        packXyNormZRg(images.getBuffer(), images.getCount(), outputFilepath);
        break;
    default:
        break;
    }

    // cleanup
    for (int i = 0; i < images.getCount(); i++) {
        if (images[i].srcPixels)
            stbi_image_free(images[i].srcPixels);
    }
    images.destroy();

    BX_TRACE("Done");

#if _DEBUG
    stb_leakcheck_dumpmem();
#endif

    return 0;
}



