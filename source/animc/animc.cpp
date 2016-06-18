#include <cstdio>
#include <cstdlib>

#include "bx/allocator.h"
#include "bx/commandline.h"
#include "bx/crtimpl.h"
#include "bx/fpumath.h"
#include "bx/string.h"
#include "bx/uint32_t.h"
#include "bxx/array.h"
#include "bxx/path.h"

#define BX_IMPLEMENT_LOGGER
#include "bxx/logger.h"

#define BX_IMPLEMENT_JSON
#include "bxx/json.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "../include_common/tanim_format.h"
#include "../include_common/coord_convert.h"
#include "../tools_common/log_format_proxy.h"

#define ANIMC_VERSION "0.1"

using namespace termite;

static bx::CrtAllocator g_alloc;
static LogFormatProxy* g_logger = nullptr;

struct Args
{
    bx::Path inFilepath;
    bx::Path outFilepath;
    bool verbose;
    ZAxis zaxis;
    int fps;

    Args()
    {
        verbose = false;
        zaxis = ZAxis::Unknown;
        fps = 30;
    }
};

struct AnimData
{
    struct Channel
    {
        taChannel c;
        float* poss;
        float* rots;
    };

    int fps;
    bool hasScale;
    int numFrames;
    int numChannels;
    Channel* channels;
};


static AnimData* importAnim(const Args& args)
{
    Assimp::Importer importer;
    uint32_t flags = 0;

    if (args.zaxis == ZAxis::Unknown)
        flags |= aiProcess_MakeLeftHanded;

    // Load scene
    const aiScene* scene = importer.ReadFile(args.inFilepath.cstr(), flags);
    if (scene == nullptr) {
        g_logger->fatal("Loading '%s' failed: %s", args.inFilepath.cstr(), importer.GetErrorString());
        return nullptr;
    }

    if (scene->mNumAnimations == 0) {
        g_logger->warn("No animations in file '%s'", args.inFilepath.cstr());
        return nullptr;
    }

    int fps = args.fps;
    int numChannels = 0;
    int numFrames = 0;
    bool hasScale = false;

    // Channel count is the sum of all animation channels
    // numFrames is the maximum of all animation channel key counts
    for (uint32_t i = 0; i < scene->mNumAnimations; i++) {
        const aiAnimation* anim = scene->mAnimations[i];
        if (anim->mTicksPerSecond > 0.0)
            fps = (int)anim->mTicksPerSecond;

        numChannels += anim->mNumChannels;
        for (uint32_t k = 0; k < anim->mNumChannels; k++) {
            const aiNodeAnim* channel = anim->mChannels[k];
            numFrames = bx::uint32_max(numFrames,
                                       bx::uint32_max(channel->mNumPositionKeys,
                                       bx::uint32_max(channel->mNumRotationKeys, channel->mNumScalingKeys)));
        }
    }

    if (numChannels == 0) {
        g_logger->warn("No animations in file '%s'", args.inFilepath.cstr());
        return nullptr;
    }

    AnimData::Channel* channels = (AnimData::Channel*)BX_ALLOC(&g_alloc, sizeof(AnimData::Channel)*numChannels);
    if (!channels)
        return nullptr;
    memset(channels, 0x00, sizeof(AnimData::Channel)*numChannels);

    int channelOffset = 0;
    for (uint32_t i = 0; i < scene->mNumAnimations; i++) {
        const aiAnimation* aanim = scene->mAnimations[i];
        for (uint32_t k = 0; k < aanim->mNumChannels; k++) {
            const aiNodeAnim* achannel = aanim->mChannels[k];
            AnimData::Channel* channel = &channels[k + channelOffset];

            bx::strlcpy(channel->c.bindto, achannel->mNodeName.data, sizeof(channel->c.bindto));
            channel->poss = (float*)BX_ALLOC(&g_alloc, sizeof(float) * 4 * numFrames);
            channel->rots = (float*)BX_ALLOC(&g_alloc, sizeof(float) * 4 * numFrames);
            if (!channel->poss || !channel->rots)
                return nullptr;

            vec3_t pos;
            quat_t quat;
            for (int f = 0; f < numFrames; f++) {
                float scale = 1.0f;
                if (f < (int)achannel->mNumPositionKeys) {
                    pos = convertVec3(achannel->mPositionKeys[f].mValue, args.zaxis);
                }

                if (f < (int)achannel->mNumRotationKeys) {
                    quat = convertQuat(achannel->mRotationKeys[f].mValue, args.zaxis);
                }

                if (f < (int)achannel->mNumScalingKeys) {
                    scale = (achannel->mScalingKeys[f].mValue.x +
                             achannel->mScalingKeys[f].mValue.y +
                             achannel->mScalingKeys[f].mValue.z) / 3.0f;
                    hasScale = !bx::fequal(scale, 1.0f, 0.00001f);
                }

                float* p = &channel->poss[f * 4];
                p[0] = pos.x;       p[1] = pos.y;       p[2] = pos.z;       p[3] = scale;
                float* r = &channel->rots[f * 4];
                r[0] = quat.x;      r[1] = quat.y;      r[2] = quat.z;      r[3] = quat.w;
            }
        }

        channelOffset += aanim->mNumChannels;
    }

    // Write
    AnimData* anim = (AnimData*)BX_ALLOC(&g_alloc, sizeof(AnimData));
    if (!anim)
        return nullptr;
    anim->channels = channels;
    anim->fps = fps;
    anim->hasScale = hasScale;
    anim->numChannels = numChannels;
    anim->numFrames = numFrames;
    return anim;
}

static bool exportAnimFile(const char* animFilepath, const AnimData& anim)
{
    // Write to file
    taHeader header;
    header.sign = TANIM_SIGN;
    header.version = TANIM_VERSION;
    header.fps = anim.fps;
    header.hasScale = anim.hasScale ? 1 : 0;
    header.numFrames = anim.numFrames;
    header.numChannels = anim.numChannels;
    header.metaOffset = -1;

    bx::CrtFileWriter file;
    bx::Error err;
    if (!file.open(animFilepath, false, &err)) {
        g_logger->fatal("Could not open file '%s' for writing", animFilepath);
        return false;
    }

    file.write(&header, sizeof(header), &err);

    for (int i = 0; i < anim.numChannels; i++) {
        file.write(&anim.channels[i].c, sizeof(taChannel), &err);
        file.write(&anim.channels[i].poss, sizeof(float) * 4 * header.numFrames, &err);
        file.write(&anim.channels[i].rots, sizeof(float) * 4 * header.numFrames, &err);
    }

    file.close();
    return true;
}

static void showHelp()
{
    const char* help =
        "animc v" ANIMC_VERSION " - Animation importer for termite engine\n"
        "Arguments:\n"
        "  -i --input <filepath> Input animation file\n"
        "  -o --output <filepath> Output tanim file\n"
        "  -v --verbose Verbose mode\n"
        "  -z --zaxis <zaxis> Set Z-Axis, choises are ['UP', 'GL']\n"
        "  -j --jsonlog Enable json logging instead of normal text\n"
        "  -f --fps <fps> default number of frames-per-second\n";
    puts(help);
}

int main(int argc, char** argv)
{
    // Read arguments
    Args args;
    bx::CommandLine cmd(argc, argv);
    args.verbose = cmd.hasArg('v', "verbose");
   
    const char* zaxis = cmd.findOption('z', "zaxis", "");
    if (bx::stricmp(zaxis, "UP") == 0)
        args.zaxis = ZAxis::Up;
    else if (bx::stricmp(zaxis, "GL") == 0)
        args.zaxis = ZAxis::GL;
    else
        args.zaxis = ZAxis::Unknown;
    args.fps = bx::toInt(cmd.findOption('f', "fps", "30"));
    args.inFilepath = cmd.findOption('i', "input", "");
    args.outFilepath = cmd.findOption('o', "output", "");
    bool jsonLog = cmd.hasArg('j', "jsonlog");

    bool help = cmd.hasArg('h', "help");
    if (help) {
        showHelp();
        return 0;
    }

    // Logger
    bx::enableLogToFileHandle(stdout);
    LogFormatProxy logger(jsonLog ? LogProxyOptions::Json : LogProxyOptions::Text);
    g_logger = &logger;

    // Argument check
    if (args.inFilepath.isEmpty() || args.outFilepath.isEmpty()) {
        g_logger->fatal("Invalid arguments");
        return -1;
    }

    if (args.inFilepath.getType() != bx::PathType::File) {
        g_logger->fatal("File '%s' is invalid", args.inFilepath.cstr());
        return -1;
    }

    AnimData* anim = importAnim(args);
    if (!anim)
        return -1;
    int ret = exportAnimFile(args.outFilepath.cstr(), *anim) ? 0 : -1;

    // cleanup
    for (int i = 0; i < anim->numChannels; i++) {
        if (anim->channels[i].poss)
            BX_FREE(&g_alloc, anim->channels[i].poss);
        if (anim->channels[i].rots)
            BX_FREE(&g_alloc, anim->channels[i].rots);
    }
    BX_FREE(&g_alloc, anim->channels);
    BX_FREE(&g_alloc, anim);

    return ret;
}