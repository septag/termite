#include "termite/core.h"

#include "bx/fpumath.h"
#include "bx/uint32_t.h"

#include "bxx/logger.h"

#include "termite/gfx_driver.h"

#include "imgui_impl.h"

#include "shaders_h/imgui.fso"
#include "shaders_h/imgui.vso"

using namespace termite;

struct ImGuiImpl
{
    bx::AllocatorI* alloc;
    GfxDriverApi* driver;
    ProgramHandle progHandle;
    
    TextureHandle fontTexHandle;
    UniformHandle uniformTexture;

    uint8_t viewId;

    ImGuiImpl()
    {
        viewId = 0;
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
        vdeclBegin(&Decl);
        vdeclAdd(&Decl, VertexAttrib::Position, 2, VertexAttribType::Float);
        vdeclAdd(&Decl, VertexAttrib::TexCoord0, 2, VertexAttribType::Float);
        vdeclAdd(&Decl, VertexAttrib::Color0, 4, VertexAttribType::Uint8, true);
        vdeclEnd(&Decl);
    }
    static VertexDecl Decl;
};
VertexDecl imVertexPosCoordColor::Decl;

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

    GfxDriverApi* driver = g_Im->driver;

    float proj[16];
    float width = ImGui::GetIO().DisplaySize.x;
    float height = ImGui::GetIO().DisplaySize.y;
    uint8_t viewId = g_Im->viewId;
    bx::mtxOrtho(proj, 0.0f, width, height, 0.0f, -1.0f, 1.0f);

    GfxState state = gfxStateBlendAlpha() |
        GfxState::RGBWrite | GfxState::AlphaWrite | GfxState::CullCCW;
    driver->touch(viewId);
    driver->setViewRectRatio(viewId, 0, 0, BackbufferRatio::Equal);
    driver->setViewSeq(viewId, true);
    driver->setViewTransform(viewId, nullptr, proj, GfxViewFlag::Stereo, nullptr);

    for (int n = 0; n < data->CmdListsCount; n++) {
        const ImDrawList* cmdList = data->CmdLists[n];

        TransientVertexBuffer tvb;
        TransientIndexBuffer tib;

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
                TextureHandle* handle = (TextureHandle*)cmd.TextureId;
                if (handle)
                    driver->setTexture(0, g_Im->uniformTexture, *handle, TextureFlag::FromTexture);
                driver->setTransientIndexBufferI(&tib, indexOffset, cmd.ElemCount);
                driver->setTransientVertexBufferI(&tvb, 0, numVertices);
                driver->setState(state, 0);

                driver->submit(viewId, g_Im->progHandle, 0, false);
            }

            indexOffset += cmd.ElemCount;
        }
    }
}

int termite::imguiInit(uint8_t viewId, uint16_t viewWidth, uint16_t viewHeight, GfxDriverApi* driver, const int* keymap)
{
    if (g_Im) {
        assert(false);
        return T_ERR_FAILED;
    }

    BX_BEGINP("Initializing ImGui Integration");

    bx::AllocatorI* alloc = getHeapAlloc();
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
    ShaderHandle fragmentShader = driver->createShader(driver->makeRef(imgui_fso, sizeof(imgui_fso), nullptr, nullptr));
    if (!fragmentShader.isValid()) {
        BX_END_FATAL();
        T_ERROR("Creating fragment-shader failed");
        return T_ERR_FAILED;
    }

    ShaderHandle vertexShader = driver->createShader(driver->makeRef(imgui_vso, sizeof(imgui_vso), nullptr, nullptr));
    if (!vertexShader.isValid()) {
        BX_END_FATAL();
        T_ERROR("Creating vertex-shader failed");
        return T_ERR_FAILED;
    }


    g_Im->progHandle = driver->createProgram(vertexShader, fragmentShader, true);
    if (!g_Im->progHandle.isValid()) {
        BX_END_FATAL();
        T_ERROR("Creating GPU Program failed");
        return T_ERR_FAILED;
    }
    g_Im->uniformTexture = driver->createUniform("u_texture", UniformType::Int1, 1);    

    // Setup ImGui
    ImGuiIO& conf = ImGui::GetIO();
    conf.DisplaySize = ImVec2((float)viewWidth, (float)viewHeight);
    conf.RenderDrawListsFn = imguiDrawLists;
    conf.MemAllocFn = imguiAlloc;
    conf.MemFreeFn = imguiFree;

    if (keymap) {
        conf.KeyMap[ImGuiKey_Tab] = keymap[int(GuiKeyMap::Tab)];
        conf.KeyMap[ImGuiKey_LeftArrow] = keymap[int(GuiKeyMap::LeftArrow)];
        conf.KeyMap[ImGuiKey_RightArrow] = keymap[int(GuiKeyMap::RightArrow)];
        conf.KeyMap[ImGuiKey_UpArrow] = keymap[int(GuiKeyMap::UpArrow)];
        conf.KeyMap[ImGuiKey_DownArrow] = keymap[int(GuiKeyMap::DownArrow)];
        conf.KeyMap[ImGuiKey_Home] = keymap[int(GuiKeyMap::Home)];
        conf.KeyMap[ImGuiKey_End] = keymap[int(GuiKeyMap::End)];
        conf.KeyMap[ImGuiKey_Delete] = keymap[int(GuiKeyMap::Delete)];
        conf.KeyMap[ImGuiKey_Backspace] = keymap[int(GuiKeyMap::Backspace)];
        conf.KeyMap[ImGuiKey_Enter] = keymap[int(GuiKeyMap::Enter)];
        conf.KeyMap[ImGuiKey_Escape] = keymap[int(GuiKeyMap::Escape)];
        conf.KeyMap[ImGuiKey_A] = keymap[int(GuiKeyMap::A)];
        conf.KeyMap[ImGuiKey_C] = keymap[int(GuiKeyMap::C)];
        conf.KeyMap[ImGuiKey_V] = keymap[int(GuiKeyMap::V)];
        conf.KeyMap[ImGuiKey_X] = keymap[int(GuiKeyMap::X)];
        conf.KeyMap[ImGuiKey_Y] = keymap[int(GuiKeyMap::Y)];
        conf.KeyMap[ImGuiKey_Z] = keymap[int(GuiKeyMap::Z)];
        }

    uint8_t* fontData;
    int fontWidth, fontHeight, bpp;
    conf.Fonts->GetTexDataAsAlpha8(&fontData, &fontWidth, &fontHeight, &bpp);

    g_Im->fontTexHandle = driver->createTexture2D((uint16_t)fontWidth, (uint16_t)fontHeight, 1, TextureFormat::A8, 
                                                 TextureFlag::MinPoint|TextureFlag::MagPoint, 
                                                 driver->makeRef(fontData, fontWidth*fontHeight*bpp, nullptr, nullptr));
    if (!g_Im->fontTexHandle.isValid()) {
        BX_END_FATAL();
        T_ERROR("ImGui: Could not create font texture");
        return T_ERR_FAILED;
    }
    conf.Fonts->TexID = (void*)&g_Im->fontTexHandle;

    ImGui::NewFrame();

    BX_END_OK();
    return 0;
}

void termite::imguiShutdown()
{
    //
    if (!g_Im)
        return;

    bx::AllocatorI* alloc = g_Im->alloc;
    GfxDriverApi* driver = g_Im->driver;

    ImGui::Shutdown();

    if (g_Im->uniformTexture.isValid())
        driver->destroyUniform(g_Im->uniformTexture);
    if (g_Im->fontTexHandle.isValid())    
        driver->destroyTexture(g_Im->fontTexHandle);
    if (g_Im->progHandle.isValid())
        driver->destroyProgram(g_Im->progHandle);

    BX_DELETE(alloc, g_Im);
    g_Im = nullptr;
}

