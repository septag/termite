#include <cstdio>
#include <cstdlib>

#include "assimp/Importer.hpp"
#include "assimp/scene.h"

#include "bx/commandline.h"
#include "bxx/path.h"

#define BX_IMPLEMENT_JSON
#include "bxx/json.h"

#include "../tools_common/log_format_proxy.h"

#include "bx/file.h"

#define LSMODEL_VERSION "0.1"

static bx::DefaultAllocator gAlloc;

using namespace tee;

static void showHelp()
{
    const char* help =
        "ls-model v" LSMODEL_VERSION " - List models inside a file\n"
        "arguments:\n"
        "  -i --input <filepath> Input model file (*.dae, *.fbx, *.obj, etc.)\n"
        "  -j --jsonlog Enable json logging instead of normal text\n";
    puts(help);
}

int main(int argc, char **argv)
{
    bx::CommandLine cmd(argc, argv);
    bx::Path inFilepath(cmd.findOption('i', "input", ""));
    bool jsonlog = cmd.hasArg('j', "jsonlog");
    bool help = cmd.hasArg('h', "help");

    if (help) {
        showHelp();
        return 0;
    }

    LogFormatProxy logger(jsonlog ? LogProxyOptions::Json : LogProxyOptions::Text);

    bx::FileInfo finfo;
    if (!bx::stat(inFilepath.cstr(), finfo) || finfo.m_type != bx::FileInfo::Regular) {
        logger.fatal("Invalid input file '%s'", inFilepath.cstr());
        return -1;
    }

    // Load the file
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(inFilepath.cstr(), 0);
    if (!scene) {
        logger.fatal("Loading '%s' failed: %s", inFilepath.cstr(), importer.GetErrorString());
        return -1;
    }

    // Enumerate models
    aiNode *node = scene->mRootNode;
    if (node == nullptr) {
        logger.warn("Model '%s' Doesn't contain geometry", inFilepath.cstr());
        return 0;
    }

    // Output result
    bx::JsonNodeAllocator alloc(&gAlloc);
    bx::JsonNode* jroot = bx::createJsonNode(&alloc, nullptr, bx::JsonType::Object);
    bx::JsonNode* jchilds;

    if (node->mNumMeshes) {
        jroot->addChild(bx::createJsonNode(&alloc, "mesh")->setString(node->mName.C_Str()));
    }

    if (node->mNumChildren) {
        jchilds = bx::createJsonNode(&alloc, "children", bx::JsonType::Array);
        jroot->addChild(jchilds);

        for (uint32_t i = 0; i < node->mNumChildren; i++) {
            if (node->mChildren[i]->mNumMeshes)
                jchilds->addChild(bx::createJsonNode(&alloc)->setString(node->mChildren[i]->mName.C_Str()));
        }
    }

    char* result = bx::makeJson(jroot, &gAlloc, false);
    jroot->destroy();
    puts(result);
    BX_FREE(&gAlloc, result);
    return 0;
}

