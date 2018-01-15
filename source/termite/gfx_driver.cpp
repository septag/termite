#include "pch.h"
#include "bx/hash.h"
#include "gfx_driver.h"

namespace tee {
    static const uint8_t gAttribTypeSizeDx9[VertexAttribType::Count][4] =
    {
        {4,  4,  4,  4}, // Uint8
        {4,  4,  4,  4}, // Uint10
        {4,  4,  8,  8}, // Int16
        {4,  4,  8,  8}, // Half
        {4,  8, 12, 16}, // Float
    };

    static const uint8_t gAttribTypeSizeDx1x[VertexAttribType::Count][4] =
    {
        {1,  2,  4,  4}, // Uint8
        {4,  4,  4,  4}, // Uint10
        {2,  4,  8,  8}, // Int16
        {2,  4,  8,  8}, // Half
        {4,  8, 12, 16}, // Float
    };

    static const uint8_t gAttribTypeSizeGl[VertexAttribType::Count][4] =
    {
        {1,  2,  4,  4}, // Uint8
        {4,  4,  4,  4}, // Uint10
        {2,  4,  6,  8}, // Int16
        {2,  4,  6,  8}, // Half
        {4,  8, 12, 16}, // Float
    };

    static const uint8_t(*gAttribTypeSize[])[VertexAttribType::Count][4] =
    {
        &gAttribTypeSizeDx9,  // Null
        &gAttribTypeSizeDx9,  // Direct3D9
        &gAttribTypeSizeDx1x, // Direct3D11
        &gAttribTypeSizeDx1x, // Direct3D12
        &gAttribTypeSizeGl,   // Gnm
        &gAttribTypeSizeGl,   // Metal
        &gAttribTypeSizeGl,   // OpenGLES
        &gAttribTypeSizeGl,   // OpenGL
        &gAttribTypeSizeGl,   // Vulkan
        &gAttribTypeSizeDx9,  // Count
    };
    BX_STATIC_ASSERT(BX_COUNTOF(gAttribTypeSize) == RendererType::Count + 1);

    VertexDecl* gfx::beginDecl(VertexDecl* vdecl, RendererType::Enum _type /*= RendererType::Noop*/)
    {
        vdecl->hash = (uint32_t)_type;
        vdecl->stride = 0;
        bx::memSet(vdecl->offset, 0x00, sizeof(vdecl->offset));
        bx::memSet(vdecl->attribs, 0xff, sizeof(vdecl->attribs));
        return vdecl;
    }

    void gfx::endDecl(VertexDecl* vdecl)
    {
        bx::HashMurmur2A murmur;
        murmur.begin();
        murmur.add(vdecl->attribs, sizeof(vdecl->attribs));
        murmur.add(vdecl->offset, sizeof(vdecl->offset));
        vdecl->hash = murmur.end();
    }

    VertexDecl* gfx::addAttrib(VertexDecl* vdecl, VertexAttrib::Enum _attrib, uint8_t _num, VertexAttribType::Enum _type,
                              bool _normalized /*= false*/, bool _asInt /*= false*/)
    {
        int aidx = (int)_attrib;
        const uint16_t encodedNorm = (_normalized & 1) << 7;
        const uint16_t encodedType = ((uint16_t)_type & 7) << 3;
        const uint16_t encodedNum = (_num - 1) & 3;
        const uint16_t encodeAsInt = (_asInt&(!!"\x1\x1\x1\x0\x0"[(int)_type])) << 8;
        vdecl->attribs[aidx] = encodedNorm | encodedType | encodedNum | encodeAsInt;
        vdecl->offset[aidx] = vdecl->stride;
        vdecl->stride += (*gAttribTypeSize[vdecl->hash])[(int)_type][_num - 1];
        return vdecl;
    }

    VertexDecl* gfx::skipAttrib(VertexDecl* vdecl, uint8_t _numBytes)
    {
        vdecl->stride += _numBytes;
        return vdecl;
    }

    void gfx::decodeAttrib(VertexDecl* vdecl, VertexAttrib::Enum _attrib, uint8_t* _num, VertexAttribType::Enum* _type,
                          bool* _normalized, bool* _asInt)
    {
        uint16_t val = vdecl->attribs[_attrib];
        *_num = (val & 3) + 1;
        *_type = VertexAttribType::Enum((val >> 3) & 7);
        *_normalized = !!(val&(1 << 7));
        *_asInt = !!(val&(1 << 8));
    }

    bool gfx::hasAttrib(VertexDecl* vdecl, VertexAttrib::Enum _attrib)
    {
        return vdecl->attribs[(int)_attrib] != UINT16_MAX;
    }

    uint32_t gfx::getDeclSize(const VertexDecl* vdecl, uint32_t _num)
    {
        return _num*vdecl->stride;
    }
}
