#ifdef termite_SHARED_LIB
#undef termite_SHARED_LIB
#endif
#include "termite/core.h"

#include "bx/fpumath.h"
#include "bx/uint32_t.h"

#include "termite/gfx_driver.h"

#include "imgui_impl.h"

#include T_MAKE_SHADER_PATH(shaders_h, imgui.fso)
#include T_MAKE_SHADER_PATH(shaders_h, imgui.vso)

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

    GfxState::Bits state = gfxStateBlendAlpha() | GfxState::RGBWrite | GfxState::AlphaWrite;
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

        if (!driver->getAvailTransientVertexBuffer(numVertices, imVertexPosCoordColor::Decl) ||
            !driver->getAvailTransientIndexBuffer(numIndices)) 
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

int termite::initImGui(uint8_t viewId, uint16_t viewWidth, uint16_t viewHeight, GfxDriverApi* driver,
					   bx::AllocatorI* alloc, const int* keymap, const char* iniFilename, void* nativeWindowHandle)
{
    if (g_Im) {
        assert(false);
        return T_ERR_FAILED;
    }

    g_Im = BX_NEW(alloc, ImGuiImpl);
    if (!g_Im) {
        return T_ERR_OUTOFMEM;
    }
    g_Im->alloc = alloc;
    g_Im->driver = driver;
    g_Im->viewId = viewId;
    imVertexPosCoordColor::init();

    // Create Graphic objects
    ShaderHandle fragmentShader = driver->createShader(driver->makeRef(imgui_fso, sizeof(imgui_fso), nullptr, nullptr));
    if (!fragmentShader.isValid()) {
        return T_ERR_FAILED;
    }

    ShaderHandle vertexShader = driver->createShader(driver->makeRef(imgui_vso, sizeof(imgui_vso), nullptr, nullptr));
    if (!vertexShader.isValid()) {
        return T_ERR_FAILED;
    }

    g_Im->progHandle = driver->createProgram(vertexShader, fragmentShader, true);
    if (!g_Im->progHandle.isValid()) {
        return T_ERR_FAILED;
    }
    g_Im->uniformTexture = driver->createUniform("u_texture", UniformType::Int1, 1);   

    // Setup ImGui
    ImGuiIO& conf = ImGui::GetIO();
    conf.DisplaySize = ImVec2((float)viewWidth, (float)viewHeight);
    conf.IniFilename = (iniFilename == nullptr || iniFilename[0] == 0) ? "imgui.ini" : iniFilename;
    conf.RenderDrawListsFn = imguiDrawLists;
    conf.MemAllocFn = imguiAlloc;
    conf.MemFreeFn = imguiFree;
    conf.ImeWindowHandle = nativeWindowHandle;

    if (keymap) {
        conf.KeyMap[ImGuiKey_Tab] = keymap[ImGuiKey_Tab];
        conf.KeyMap[ImGuiKey_LeftArrow] = keymap[ImGuiKey_LeftArrow];
        conf.KeyMap[ImGuiKey_RightArrow] = keymap[ImGuiKey_RightArrow];
        conf.KeyMap[ImGuiKey_UpArrow] = keymap[ImGuiKey_UpArrow];
        conf.KeyMap[ImGuiKey_DownArrow] = keymap[ImGuiKey_DownArrow];
        conf.KeyMap[ImGuiKey_Home] = keymap[ImGuiKey_Home];
        conf.KeyMap[ImGuiKey_End] = keymap[ImGuiKey_End];
        conf.KeyMap[ImGuiKey_Delete] = keymap[ImGuiKey_Delete];
        conf.KeyMap[ImGuiKey_Backspace] = keymap[ImGuiKey_Backspace];
        conf.KeyMap[ImGuiKey_Enter] = keymap[ImGuiKey_Enter];
        conf.KeyMap[ImGuiKey_Escape] = keymap[ImGuiKey_Escape];
        conf.KeyMap[ImGuiKey_A] = keymap[ImGuiKey_A];
        conf.KeyMap[ImGuiKey_C] = keymap[ImGuiKey_C];
        conf.KeyMap[ImGuiKey_V] = keymap[ImGuiKey_V];
        conf.KeyMap[ImGuiKey_X] = keymap[ImGuiKey_X];
        conf.KeyMap[ImGuiKey_Y] = keymap[ImGuiKey_Y];
        conf.KeyMap[ImGuiKey_Z] = keymap[ImGuiKey_Z];
    }

    uint8_t* fontData;
    int fontWidth, fontHeight, bpp;
    conf.Fonts->GetTexDataAsRGBA32(&fontData, &fontWidth, &fontHeight, &bpp);
    g_Im->fontTexHandle = driver->createTexture2D((uint16_t)fontWidth, (uint16_t)fontHeight, false, 1, TextureFormat::RGBA8, 
                                                 TextureFlag::MinPoint|TextureFlag::MagPoint, 
                                                 driver->makeRef(fontData, fontWidth*fontHeight*bpp, nullptr, nullptr));
    if (!g_Im->fontTexHandle.isValid()) {
        return T_ERR_FAILED;
    }
    conf.Fonts->TexID = (void*)&g_Im->fontTexHandle;

    ImGui::NewFrame();

    return 0;
}

void termite::shutdownImGui()
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

