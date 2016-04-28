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
    gfxDriver* driver;
    gfxProgramHandle progHandle;
    gfxDynamicVertexBufferHandle vertexBuffer;
    gfxDynamicIndexBufferHandle indexBuffer;
    
    gfxTextureHandle fontTexHandle;
    gfxUniformHandle uniformTexture;

    ImGuiImpl()
    {
        fontTexHandle = T_INVALID_HANDLE;
        progHandle = T_INVALID_HANDLE;
        vertexBuffer = T_INVALID_HANDLE;
        uniformTexture = T_INVALID_HANDLE;
        alloc = nullptr;
    }
};

struct VertexPosCoordColor
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
gfxVertexDecl VertexPosCoordColor::Decl;

static ImGuiImpl* gIm = nullptr;

static void* imguiAlloc(size_t size)
{
    return BX_ALLOC(gIm->alloc, size);
}

static void imguiFree(void* ptr)
{
    if (gIm)
        BX_FREE(gIm->alloc, ptr);
}

static void imguiDrawLists(ImDrawData* data)
{
    assert(gIm);

    gfxDriver* driver = gIm->driver;

    float proj[16];
    bx::mtxOrtho(proj, 0.0f, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, 0.0f, -1.0f, 1.0f);
    driver->setViewTransform(0, nullptr, proj);

    gfxState state = gfxStateBlendAlpha() |
        gfxState::RGBWrite | gfxState::AlphaWrite | gfxState::DepthTestAlways | gfxState::CullCCW;

    for (int n = 0; n < data->CmdListsCount; n++) {
        const ImDrawList* cmdList = data->CmdLists[n];

        gfxTransientVertexBuffer tvb;
        gfxTransientIndexBuffer tib;

        uint32_t numVertices = (uint32_t)cmdList->VtxBuffer.size();
        uint32_t numIndices = (uint32_t)cmdList->IdxBuffer.size();

        if (!driver->checkAvailTransientVertexBuffer(numVertices, VertexPosCoordColor::Decl) ||
            !driver->checkAvailTransientIndexBuffer(numIndices)) 
        {
            break;
        }

        driver->allocTransientVertexBuffer(&tvb, numVertices, VertexPosCoordColor::Decl);
        driver->allocTransientIndexBuffer(&tib, numIndices);

        // Fill Vertex/Index data
        VertexPosCoordColor* verts = (VertexPosCoordColor*)tvb.data;
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
                    driver->setTexture(0, gIm->uniformTexture, *handle);
                driver->setState(state);
                driver->setIndexBuffer(&tib, indexOffset, cmd.ElemCount);
                driver->setVertexBuffer(&tvb, 0, numVertices);
                driver->submit(0, gIm->progHandle);
            }

            indexOffset += cmd.ElemCount;
        }
    }
}

int termite::imguiInit(uint16_t viewWidth, uint16_t viewHeight, gfxDriver* driver, const int* keymap)
{
    if (gIm) {
        assert(false);
        return T_ERR_FAILED;
    }

    BX_BEGINP("Initializing ImGui Integration");

    bx::AllocatorI* alloc = coreGetAlloc();
    gIm = BX_NEW(alloc, ImGuiImpl);
    if (!gIm) {
        BX_END_FATAL();
        return T_ERR_OUTOFMEM;
    }
    gIm->alloc = alloc;
    gIm->driver = driver;
    VertexPosCoordColor::init();

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


    gIm->progHandle = driver->createProgram(vertexShader, fragmentShader, true);
    if (!T_ISVALID(gIm->progHandle)) {
        BX_END_FATAL();
        T_ERROR("Creating GPU Program failed");
        return T_ERR_FAILED;
    }
    gIm->uniformTexture = driver->createUniform("u_texture", gfxUniformType::Int1);    

    gIm->vertexBuffer = driver->createDynamicVertexBuffer(MAX_VERTICES, VertexPosCoordColor::Decl);
    gIm->indexBuffer = driver->createDynamicIndexBuffer(MAX_INDICES);
    if (!T_ISVALID(gIm->vertexBuffer) || !T_ISVALID(gIm->indexBuffer)) {
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

    gIm->fontTexHandle = driver->createTexture2D((uint16_t)fontWidth, (uint16_t)fontHeight, 1, gfxTextureFormat::A8, 
                                                 gfxTextureFlag::MinPoint|gfxTextureFlag::MagPoint, 
                                                 driver->makeRef(fontData, fontWidth*fontHeight*bpp));
    if (!T_ISVALID(gIm->fontTexHandle)) {
        BX_END_FATAL();
        T_ERROR("ImGui: Could not create font texture");
        return T_ERR_FAILED;
    }
    conf.Fonts->TexID = (void*)&gIm->fontTexHandle;

    ImGui::NewFrame();

    BX_END_OK();
    return T_OK;
}

void termite::imguiShutdown()
{
    if (!gIm)
        return;

    bx::AllocatorI* alloc = gIm->alloc;
    gfxDriver* driver = gIm->driver;

    ImGui::Shutdown();

    if (T_ISVALID(gIm->uniformTexture))
        driver->destroyUniform(gIm->uniformTexture);
    if (T_ISVALID(gIm->indexBuffer))
        driver->destroyDynamicIndexBuffer(gIm->indexBuffer);
    if (T_ISVALID(gIm->vertexBuffer))
        driver->destroyDynamicVertexBuffer(gIm->vertexBuffer);
    if (T_ISVALID(gIm->fontTexHandle))    
        driver->destroyTexture(gIm->fontTexHandle);
    if (T_ISVALID(gIm->progHandle))
        driver->destroyProgram(gIm->progHandle);

    BX_DELETE(alloc, gIm);
    gIm = nullptr;
}

void termite::imguiRender()
{
    ImGui::Render();
}

void termite::imguiNewFrame()
{
    ImGui::NewFrame();
    ImGui::ShowTestWindow();
    //ImGui::ShowStyleEditor();
}
