#include "bx/bx.h"
#include "bx/commandline.h"
#include "bx/allocator.h"
#include "bx/file.h"
#include "bxx/path.h"

#include "termite/types.h"

#include "tiny-AES128-C/aes.h"
#include "lz4/lz4.h"

#include <stdio.h>

#include <cassert>

// default AES encryption keys
static const uint8_t kAESKey[] = {0x32, 0xBF, 0xE7, 0x76, 0x41, 0x21, 0xF6, 0xA5, 0xEE, 0x70, 0xDC, 0xC8, 0x73, 0xBC, 0x9E, 0x37};
static const uint8_t kAESIv[] = {0x0A, 0x2D, 0x76, 0x63, 0x9F, 0x28, 0x10, 0xCD, 0x24, 0x22, 0x26, 0x68, 0xC1, 0x5A, 0x82, 0x5A};

#define T_ENC_SIGN 0x54454e43        // "TENC"
#define T_ENC_VERSION TEE_MAKE_VERSION(1, 0)       

// Encode file header
// Same as termite version (core.cpp)
#pragma pack(push, 1)
struct EncodeHeader
{
    uint32_t sign;
    uint32_t version;
    int decodeSize;
    int uncompSize;
};
#pragma pack(pop)


#ifdef _DEBUG
#define STB_LEAKCHECK_IMPLEMENTATION
#include "bxx/leakcheck_allocator.h"
static bx::LeakCheckAllocator gAllocStub;
#else
static bx::DefaultAllocator gAllocStub;
#endif
static bx::AllocatorI* gAlloc = &gAllocStub;

static int char2int(char input)
{
    if (input >= '0' && input <= '9')
        return input - '0';
    if (input >= 'A' && input <= 'F')
        return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
        return input - 'a' + 10;
    else
        return 0;
}

// This function assumes src to be a zero terminated sanitized string with
// an even number of [0-9a-f] characters, and target to be sufficiently large
static void hex2bin(const char* src, char* target)
{
    while (*src && src[1]) {
        *(target++) = char2int(*src)*16 + char2int(src[1]);
        src += 2;
    }
}

int main(int argc, char* argv[])
{
    bx::CommandLine cmdline(argc, argv);
    const char* filepath = cmdline.findOption('f', "file");
    const char* outputFilepath = cmdline.findOption('o', "out");
    if (!filepath || !outputFilepath) {
        printf("-f and -o Parameters must be set\n");
        return -1;
    }
    const char* skey = cmdline.findOption('k', "key");
    const char* siv = cmdline.findOption('i', "iv");

    uint8_t key[16];
    uint8_t iv[16];
    if (!skey)  memcpy(key, kAESKey, sizeof(key));
    else        hex2bin(skey, (char*)key);

    if (!siv)   memcpy(iv, kAESIv, sizeof(iv));
    else        hex2bin(siv, (char*)iv);

    bx::Path path(filepath);
    path.normalizeSelf();

    bx::FileInfo finfo;
    if (!bx::stat(path.cstr(), finfo) || finfo.m_type != bx::FileInfo::Regular) {
        printf("'%s' is an invalid file path\n", path.cstr());
        return -1;
    }

    // Open file for read
    bx::FileReader file;
    bx::Error err;
    if (!file.open(path.cstr(), &err)) {
        printf("Could not open '%s'\n", path.cstr());
        return -1;
    }

    int32_t size = int32_t(finfo.m_size);
    void* mem = BX_ALLOC(gAlloc, size);
    if (!mem) {
        printf("Out of memory, requested size: %ull\n", size);
        file.close();
        return -1;
    }
    file.read(mem, int32_t(size), &err);
    file.close();

    // Encrypt and LZ4
    EncodeHeader header;
    header.sign = T_ENC_SIGN;
    header.version = T_ENC_VERSION;

    // Compress LZ4
    int maxSize = BX_ALIGN_16(LZ4_compressBound(size));
    void* compressedBuff = BX_ALLOC(gAlloc, maxSize);
    if (!compressedBuff) {
        printf("Out of memory, requested size: %ull\n", maxSize);
        BX_FREE(gAlloc, mem);
        return -1;
    }
    int compressSize = LZ4_compress_default((const char*)mem, (char*)compressedBuff, size, maxSize);
    BX_FREE(gAlloc, mem);
    int alignedCompressSize = BX_ALIGN_16(compressSize);
    assert(alignedCompressSize <= maxSize);
    bx::memSet((uint8_t*)compressedBuff + compressSize, 0x00, alignedCompressSize - compressSize);

    // AES Encode
    int encSize = int(alignedCompressSize + sizeof(header));
    void* encMem = BX_ALLOC(gAlloc, encSize);
    if (encMem) {
        AES_CBC_encrypt_buffer((uint8_t*)encMem + sizeof(header), (uint8_t*)compressedBuff, alignedCompressSize, key, iv);

        // finalize header and slap it at the begining of the output buffer
        header.decodeSize = compressSize;
        header.uncompSize = size;
        *((EncodeHeader*)encMem) = header;
    }

    BX_FREE(gAlloc, compressedBuff);
    
    // Write encMem to output file
    bx::FileWriter outFile;
    if (!outFile.open(outputFilepath, false, &err)) {
        printf("Could not write to file '%s'\n", outputFilepath);
    }

    outFile.write(encMem, encSize, &err);
    outFile.close();
    BX_FREE(gAlloc, encMem);

    printf("Encrypted file written to: %s (%.1fkb -> %.1fkb)\n", outputFilepath, float(size)/1024.0f, float(encSize)/1024.0f);

#if _DEBUG
    stb_leakcheck_dumpmem();
#endif

    return 0;
}



