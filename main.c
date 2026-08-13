#include "uzlib/src/uzlib.h"
#include "dt.h"
#include "mmu.h"

static void unpack_kernel(const void* src, size_t src_sz, void* dst, size_t dst_sz)
{
    uzlib_init();
    struct uzlib_uncomp uncomp = {
        .source = src,
        .source_limit = (uint8_t*)src + src_sz,
        .dest_start = dst,
        .dest = dst,
        .dest_limit = (uint8_t*)dst + dst_sz,
    };
    uzlib_uncompress_init(&uncomp, 0, 0);
    uzlib_zlib_parse_header(&uncomp);
    uzlib_uncompress_chksum(&uncomp);
}

static void* the_dt;

static unsigned __int128 resolve_dt_node(const char* path)
{
    size_t size = 0;
    void* data = resolve_property(the_dt, path, &size);
    return ((unsigned __int128)size << 64) | (uint64_t)data;
}

static uintptr_t call_payloads(uintptr_t p)
{
    size_t sz;
    while((sz = *(uintptr_t*)p))
    {
        p += 8;
        ((void(*)(unsigned __int128(*)(const char*)))p)(resolve_dt_node);
        p += sz;
    }
    return p + 8;
}

extern const uint64_t compressed_size;
extern const uint64_t uncompressed_size;
extern const uint64_t image_size;
extern const char _start[];
extern const char _end[];

unsigned __int128 main(void* dt)
{
    the_dt = dt;
    maybe_replace_cmdline(dt);
    uintptr_t src = call_payloads((uintptr_t)_end);
    enable_mmu((uintptr_t)_start, (uintptr_t)_start + image_size);
    uint64_t kernel_header[8];
    unpack_kernel((void*)src, compressed_size, kernel_header, sizeof(kernel_header));
    uintptr_t dst = (uintptr_t)(src + compressed_size);
    dst -= kernel_header[1];
    dst = (dst + 0x1fffff) & -0x200000;
    dst += kernel_header[1];
    unpack_kernel((void*)src, compressed_size, (void*)dst, uncompressed_size);
    disable_mmu((uintptr_t)_start, (uintptr_t)_start + image_size);
    return ((unsigned __int128)dst << 64) | (uint64_t)dt;
}
