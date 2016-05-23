#include "termite/core.h"

#include "bx/fpumath.h"
#include "bx/uint32_t.h"

#include "bxx/logger.h"

#include "termite/gfx_driver.h"

#include "imgui_impl.h"

#include "shaders_h/imgui.fso"
#include "shaders_h/imgui.vso"

#define MAX_VERTICES 5000
#define MAX_INDICES 10000

using namespace termite;

struct ImGuiImpl
{
    bx::AllocatorI* alloc;
    gfxDriverI* driver;
    gfxProgramHandle progHandle;
    gfxDynamicVertexBufferHandle vertexBuffer;
    gfxDynamicIndexBufferHandle indexBuffer;
    
    gfxTextureHandle fontTexHandle;
    gfxUniformHandle uniformTexture;

    uint8_t viewId;

    ImGuiImpl()
    {
        viewId = 0;
        fontTexHandle = T_INVALID_HANDLE;
        progHandle = T_INVALID_HANDLE;
        vertexBuffer = T_INVALID_HANDLE;
        indexBuffer = T_INVALID_HANDLE;
        uniformTexture = T_INVALID_HANDLE;
        driver = nullptr;
        alloc = nullptr;
    }
};

struct imVertexPosCoordColor
{
    float x;
    float y;
    float tx;
    float ty;
    uint32_t color;
    
    static void init()
    {
        Decl.begin()
            .add(gfxAttrib::Position, 2, gfxAttribType::Float)
            .add(gfxAttrib::TexCoord0, 2, gfxAttribType::Float)
            .add(gfxAttrib::Color0, 4, gfxAttribType::Uint8, true)
            .end();
    }
    static gfxVertexDecl Decl;
};
gfxVertexDecl imVertexPosCoordColor::Decl;

static ImGuiImpl* g_Im = nullptr;

static void* imguiAlloc(size_t size)
{
    return BX_ALLOC(g_Im->alloc, size);
}

static void imguiFree(void* ptr)
{
    if (g_Im)
        BX_FREE(g_Im->alloc, ptr);
}

static void imguiDrawLists(ImDrawData* data)
{
    assert(g_Im);

    gfxDriverI* driver = g_Im->driver;

    float proj[16];
    float width = ImGui::GetIO().DisplaySize.x;
    float height = ImGui::GetIO().DisplaySize.y;
    uint8_t viewId = g_Im->viewId;
    bx::mtxOrtho(proj, 0.0f, width, height, 0.0f, -1.0f, 1.0f);

    gfxState state = gfxStateBlendAlpha() |
        gfxState::RGBWrite | gfxState::AlphaWrite | gfxState::DepthTestAlways | gfxState::CullCCW;
    driver->touch(viewId);
    driver->setViewRect(viewId, 0, 0, gfxBackbufferRatio::Equal);
    driver->setViewSeq(viewId, true);
    driver->setViewTransform(viewId, nullptr, proj);

    for (int n = 0; n < data->CmdListsCount; n++) {
        const ImDrawList* cmdList = data->CmdLists[n];

        gfxTransientVertexBuffer tvb;
        gfxTransientIndexBuffer tib;

        uint32_t numVertices = (uint32_t)cmdList->VtxBuffer.size();
        uint32_t numIndices = (uint32_t)cmdList->IdxBuffer.size();

        if (!driver->checkAvailTransientVertexBuffer(numVertices, imVertexPosCoordColor::Decl) ||
            !driver->checkAvailTransientIndexBuffer(numIndices)) 
        {
            break;
        }

        driver->allocTransientVertexBuffer(&tvb, numVertices, imVertexPosCoordColor::Decl);
        driver->allocTransientIndexBuffer(&tib, numIndices);

        // Fill Vertex/Index data
        imVertexPosCoordColor* verts = (imVertexPosCoordColor*)tvb.data;
        uint16_t* indices = (uint16_t*)tib.data;
        const ImDrawVert* vertexSrc = &cmdList->VtxBuffer[0];

        for (int i = 0; i < cmdList->VtxBuffer.size(); i++) {
            verts->x = vertexSrc->pos.x;
            verts->y = vertexSrc->pos.y;
            verts->tx = vertexSrc->uv.x;
            verts->ty = vertexSrc->uv.y;
            verts->color = vertexSrc->col;

            verts++;
            vertexSrc++;
        }

        memcpy(indices, &cmdList->IdxBuffer[0], cmdList->IdxBuffer.size()*sizeof(ImDrawIdx));

        // Draw Command lists
        uint32_t indexOffset = 0;
        for (int i = 0; i < cmdList->CmdBuffer.size(); i++) {
            const ImDrawCmd& cmd = cmdList->CmdBuffer[i];
            if (cmd.UserCallback) {
                cmd.UserCallback(cmdList, &cmd);
            } else {
                driver->setScissor(uint16_t(cmd.ClipRect.x),
                                   uint16_t(cmd.ClipRect.y),
                                   uint16_t(cmd.ClipRect.z - cmd.ClipRect.x),
                                   uint16_t(cmd.ClipRect.w - cmd.ClipRect.y));
                gfxTextureHandle* handle = (gfxTextureHandle*)cmd.TextureId;
                if (handle)
                    driver->setTexture(0, g_Im->uniformTexture, *handle);
                driver->setIndexBuffer(&tib, indexOffset, cmd.ElemCount);
                driver->setVertexBuffer(&tvb, 0, numVertices);
                driver->setState(state);

                driver->submit(viewId, g_Im->progHandle);
            }

            indexOffset += cmd.ElemCount;
        }
    }
}

int termite::imguiInit(uint8_t viewId, uint16_t viewWidth, uint16_t viewHeight, gfxDriverI* driver, const int* keymap)
{
    if (g_Im) {
        assert(false);
        return T_ERR_FAILED;
    }

    BX_BEGINP("Initializing ImGui Integration");

    bx::AllocatorI* alloc = coreGetAlloc();
    g_Im = BX_NEW(alloc, ImGuiImpl);
    if (!g_Im) {
        BX_END_FATAL();
        return T_ERR_OUTOFMEM;
    }
    g_Im->alloc = alloc;
    g_Im->driver = driver;
    g_Im->viewId = viewId;
    imVertexPosCoordColor::init();

    // Create Graphic objects
    gfxShaderHandle fragmentShader = driver->createShader(driver->makeRef(imgui_fso, sizeof(imgui_fso)));
    if (!T_ISVALID(fragmentShader)) {
        BX_END_FATAL();
        T_ERROR("Creating fragment-shader failed");
        return T_ERR_FAILED;
    }

    gfxShaderHandle vertexShader = driver->createShader(driver->makeRef(imgui_vso, sizeof(imgui_vso)));
    if (!T_ISVALID(vertexShader)) {
        BX_END_FATAL();
        T_ERROR("Creating vertex-shader failed");
        return T_ERR_FAILED;
    }


    g_Im->progHandle = driver->createProgram(vertexShader, fragmentShader, true);
    if (!T_ISVALID(g_Im->progHandle)) {
        BX_END_FATAL();
        T_ERROR("Creating GPU Program failed");
        return T_ERR_FAILED;
    }
    g_Im->uniformTexture = driver->createUniform("u_texture", gfxUniformType::Int1);    

    g_Im->vertexBuffer = driver->createDynamicVertexBuffer(MAX_VERTICES, imVertexPosCoordColor::Decl);
    g_Im->indexBuffer = driver->createDynamicIndexBuffer(MAX_INDICES);
    if (!T_ISVALID(g_Im->vertexBuffer) || !T_ISVALID(g_Im->indexBuffer)) {
        BX_END_FATAL();
        T_ERROR("Creating GPU VertexBuffer failed");
        return T_ERR_FAILED;
    }

    // Setup ImGui
    ImGuiIO& conf = ImGui::GetIO();
    conf.DisplaySize = ImVec2((float)viewWidth, (float)viewHeight);
    conf.RenderDrawListsFn = imguiDrawLists;
    conf.MemAllocFn = imguiAlloc;
    conf.MemFreeFn = imguiFree;

    if (keymap) {
        conf.KeyMap[ImGuiKey_Tab] = keymap[int(gfxGuiKeyMap::Tab)];
        conf.KeyMap[ImGuiKey_LeftArrow] = keymap[int(gfxGuiKeyMap::LeftArrow)];
        conf.KeyMap[ImGuiKey_RightArrow] = keymap[int(gfxGuiKeyMap::RightArrow)];
        conf.KeyMap[ImGuiKey_UpArrow] = keymap[int(gfxGuiKeyMap::UpArrow)];
        conf.KeyMap[ImGuiKey_DownArrow] = keymap[int(gfxGuiKeyMap::DownArrow)];
        conf.KeyMap[ImGuiKey_Home] = keymap[int(gfxGuiKeyMap::Home)];
        conf.KeyMap[ImGuiKey_End] = keymap[int(gfxGuiKeyMap::End)];
        conf.KeyMap[ImGuiKey_Delete] = keymap[int(gfxGuiKeyMap::Delete)];
        conf.KeyMap[ImGuiKey_Backspace] = keymap[int(gfxGuiKeyMap::Backspace)];
        conf.KeyMap[ImGuiKey_Enter] = keymap[int(gfxGuiKeyMap::Enter)];
        conf.KeyMap[ImGuiKey_Escape] = keymap[int(gfxGuiKeyMap::Escape)];
        conf.KeyMap[ImGuiKey_A] = keymap[int(gfxGuiKeyMap::A)];
        conf.KeyMap[ImGuiKey_C] = keymap[int(gfxGuiKeyMap::C)];
        conf.KeyMap[ImGuiKey_V] = keymap[int(gfxGuiKeyMap::V)];
        conf.KeyMap[ImGuiKey_X] = keymap[int(gfxGuiKeyMap::X)];
        conf.KeyMap[ImGuiKey_Y] = keymap[int(gfxGuiKeyMap::Y)];
        conf.KeyMap[ImGuiKey_Z] = keymap[int(gfxGuiKeyMap::Z)];
        }

    uint8_t* fontData;
    int fontWidth, fontHeight, bpp;
    conf.Fonts->GetTexDataAsAlpha8(&fontData, &fontWidth, &fontHeight, &bpp);

    g_Im->fontTexHandle = driver->createTexture2D((uint16_t)fontWidth, (uint16_t)fontHeight, 1, gfxTextureFormat::A8, 
                                                 gfxTextureFlag::MinPoint|gfxTextureFlag::MagPoint, 
                                                 driver->makeRef(fontData, fontWidth*fontHeight*bpp));
    if (!T_ISVALID(g_Im->fontTexHandle)) {
        BX_END_FATAL();
        T_ERROR("ImGui: Could not create font texture");
        return T_ERR_FAILED;
    }
    conf.Fonts->TexID = (void*)&g_Im->fontTexHandle;

    ImGui::NewFrame();

    BX_END_OK();
    return T_OK;
}

void termite::imguiShutdown()
{
    //
    if (!g_Im)
        return;

    bx::AllocatorI* alloc = g_Im->alloc;
    gfxDriverI* driver = g_Im->driver;

    ImGui::Shutdown();

    if (T_ISVALID(g_Im->uniformTexture))
        driver->destroyUniform(g_Im->uniformTexture);
    if (T_ISVALID(g_Im->indexBuffer))
        driver->destroyDynamicIndexBuffer(g_Im->indexBuffer);
    if (T_ISVALID(g_Im->vertexBuffer))
        driver->destroyDynamicVertexBuffer(g_Im->vertexBuffer);
    if (T_ISVALID(g_Im->fontTexHandle))    
        driver->destroyTexture(g_Im->fontTexHandle);
    if (T_ISVALID(g_Im->progHandle))
        driver->destroyProgram(g_Im->progHandle);

    BX_DELETE(alloc, g_Im);
    g_Im = nullptr;
}

void termite::imguiRender()
{
    ImGui::Render();
}

void termite::imguiNewFrame()
{
    ImGui::NewFrame();
    //ImGui::ShowTestWindow();
    // ImGui::ShowStyleEditor();
}
