#include "core.h"
#include "bx/hash.h"

#include "gfx_driver.h"

using namespace st;

static const uint8_t gAttribTypeSizeDx9[(int)gfxAttribType::Count][4] =
{
    { 4,  4,  4,  4 }, // Uint8
    { 4,  4,  4,  4 }, // Uint10
    { 4,  4,  8,  8 }, // Int16
    { 4,  4,  8,  8 }, // Half
    { 4,  8, 12, 16 }, // Float
};

static const uint8_t gAttribTypeSizeDx1x[(int)gfxAttribType::Count][4] =
{
    { 1,  2,  4,  4 }, // Uint8
    { 4,  4,  4,  4 }, // Uint10
    { 2,  4,  8,  8 }, // Int16
    { 2,  4,  8,  8 }, // Half
    { 4,  8, 12, 16 }, // Float
};

static const uint8_t gAttribTypeSizeGl[(int)gfxAttribType::Count][4] =
{
    { 1,  2,  4,  4 }, // Uint8
    { 4,  4,  4,  4 }, // Uint10
    { 2,  4,  6,  8 }, // Int16
    { 2,  4,  6,  8 }, // Half
    { 4,  8, 12, 16 }, // Float
};

static const uint8_t(*gAttribTypeSize[])[(int)gfxAttribType::Count][4] =
{
    &gAttribTypeSizeDx9,  // Null
    &gAttribTypeSizeDx9,  // Direct3D9
    &gAttribTypeSizeDx1x, // Direct3D11
    &gAttribTypeSizeDx1x, // Direct3D12
    &gAttribTypeSizeGl,   // Metal
    &gAttribTypeSizeGl,   // OpenGLES
    &gAttribTypeSizeGl,   // OpenGL
    &gAttribTypeSizeGl,   // Vulkan
    &gAttribTypeSizeDx9,  // Count
};
BX_STATIC_ASSERT(BX_COUNTOF(gAttribTypeSize) == (int)gfxRendererType::Count + 1);

st::gfxVertexDecl::gfxVertexDecl()
{

}

gfxVertexDecl& st::gfxVertexDecl::begin(gfxRendererType _type)
{
    hash = (uint32_t)_type;
    stride = 0;
    memset(attribs, 0xff, sizeof(attribs));
    memset(offset, 0x00, sizeof(offset));
    return *this;
}

void st::gfxVertexDecl::end()
{
    bx::HashMurmur2A murmur;
    murmur.begin();
    murmur.add(attribs, sizeof(attribs));
    murmur.add(offset, sizeof(offset));
    hash = murmur.end();
}

gfxVertexDecl& st::gfxVertexDecl::add(gfxAttrib _attrib, uint8_t _num, gfxAttribType _type, bool _normalized, bool _asInt /*= false*/)
{
    int aidx = (int)_attrib;
    const uint16_t encodedNorm = (_normalized & 1) << 7;
    const uint16_t encodedType = ((uint16_t)_type & 7) << 3;
    const uint16_t encodedNum = (_num - 1) & 3;
    const uint16_t encodeAsInt = (_asInt&(!!"\x1\x1\x1\x0\x0"[(int)_type])) << 8;
    attribs[aidx] = encodedNorm | encodedType | encodedNum | encodeAsInt;

    offset[aidx] = stride;
    stride += (*gAttribTypeSize[hash])[(int)_type][_num - 1];

    return *this;

}

gfxVertexDecl& st::gfxVertexDecl::skip(uint8_t _numBytes)
{
    stride += _numBytes;
    return *this;
}

void st::gfxVertexDecl::decode(gfxAttrib _attrib, uint8_t* _num, gfxAttribType* _type, bool* _normalized, bool* _asInt) const
{
    uint16_t val = attribs[(int)_attrib];
    *_num = (val & 3) + 1;
    *_type = gfxAttribType((val >> 3) & 7);
    *_normalized = !!(val&(1 << 7));
    *_asInt = !!(val&(1 << 8));
}
