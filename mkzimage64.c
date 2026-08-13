#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <zlib.h>

enum { BUF_SIZE = 65536 };

static int do_compression(FILE* outf, int infd, uint64_t* compressed_size, uint64_t* uncompressed_size)
{
    *uncompressed_size = 0;
    void* mapping = 0;
    size_t mapping_size = 0;
    z_stream strm = {};
    deflateInit(&strm, 9);
    {
        off_t sz = lseek(infd, 0, SEEK_END);
        if(sz > 0 && sz == (size_t)sz && (mapping = mmap(0, sz, PROT_READ, MAP_PRIVATE, infd, 0)) != MAP_FAILED)
        {
            mapping_size = sz;
            strm.next_in = mapping;
            strm.avail_in = mapping_size;
            if(sz >= 16)
            {
                size_t chk = sz - 16;
                if(chk > 8)
                    chk = 8;
                memcpy((char*)uncompressed_size, (char*)mapping + 16, chk);
            }
        }
        else if(sz > 0)
            lseek(infd, 0, SEEK_SET);
    }
    char inbuf[BUF_SIZE] = {};
    char outbuf[BUF_SIZE] = {};
    int eof = 0;
    do
    {
        if(!strm.avail_in && !eof)
        {
            ssize_t chk = read(infd, inbuf, BUF_SIZE);
            if(chk < 0)
            {
                perror("read");
                deflateEnd(&strm);
                if(mapping_size)
                    munmap(mapping, mapping_size);
                return -1;
            }
            if(chk == 0)
                eof = 1;
            else
            {
                strm.next_in = inbuf;
                strm.avail_in = chk;
                if(strm.total_in < 24 && strm.total_in + chk > 16)
                {
                    size_t skip = strm.total_in >= 16 ? 0 : (16 - strm.total_in);
                    char* inp = inbuf + skip;
                    char* outp = (char*)uncompressed_size + (strm.total_in + skip - 16);
                    size_t chk = strm.total_in + chk - (strm.total_in + skip);
                    if(chk >= 24 - (strm.total_in + skip))
                        chk = 24 - (strm.total_in + skip);
                    memcpy(outp, inp, chk);
                }
            }
        }
        if(!strm.avail_out)
        {
            if((char*)strm.next_out == outbuf + BUF_SIZE && fwrite(outbuf, BUF_SIZE, 1, outf) != 1)
            {
                perror("fwrite");
                deflateEnd(&strm);
                if(mapping_size)
                    munmap(mapping, mapping_size);
                return -1;
            }
            strm.next_out = outbuf;
            strm.avail_out = BUF_SIZE;
        }
    }
    while(deflate(&strm, eof ? Z_FINISH : 0) != Z_STREAM_END);
    if(fwrite(outbuf, (char*)strm.next_out - outbuf, 1, outf) != 1)
    {
        perror("fwrite");
        deflateEnd(&strm);
        if(mapping_size)
            munmap(mapping, mapping_size);
        return -1;
    }
    if(strm.total_in > *uncompressed_size)
        *uncompressed_size = strm.total_in;
    *compressed_size = strm.total_out;
    deflateEnd(&strm);
    if(mapping_size)
        munmap(mapping, mapping_size);
    return 0;
}

#if __STDC_VERSION__ >= 202300

static const char payload[] = {
    #embed "payload.bin"
};
#define payload_size sizeof(payload)

#else

asm("payload:\n.incbin \"payload.bin\"\npayload_end:");
extern const char payload[];
extern const char payload_end[];
#define payload_size ((uintptr_t)payload_end - (uintptr_t)payload)

#endif

static int write_payloads(FILE* fout, int argc, const char** argv, uint64_t* payloads_size)
{
    static char zeroes[4096];
    *payloads_size = 8;
    for(int i = 1; i < argc; i++)
    {
        if(!strcmp(argv[i], "--payload") && i + 1 < argc)
        {
            const char* payload_path = argv[++i];
            int fdin = open(payload_path, O_RDONLY);
            if(fdin < 0)
            {
                perror(payload_path);
                return 1;
            }
            off_t sz = lseek(fdin, 0, SEEK_END);
            char* mapping;
            uint64_t chunk_size = 0;
            if(sz >= 0)
                chunk_size = ((sz + 4103) & -4096) - 8;
            if(sz > 0 && sz == (size_t)sz && (mapping = mmap(0, sz, PROT_READ, MAP_PRIVATE, fdin, 0)) != MAP_FAILED)
            {
                if(fwrite(&chunk_size, sizeof(chunk_size), 1, fout) != 1
                || fwrite(mapping, sz, 1, fout) != 1
                || fwrite(zeroes, chunk_size - (size_t)sz, 1, fout) != 1)
                {
                    perror("fwrite");
                    munmap(mapping, sz);
                    close(fdin);
                    return -1;
                }
                munmap(mapping, sz);
                close(fdin);
                *payloads_size += chunk_size + 8;
                continue;
            }
            lseek(fdin, 0, SEEK_END);
            off_t head = ftell(fout);
            uint64_t actual_chunk_size = 0;
            if(fwrite(&chunk_size, sizeof(chunk_size), 1, fout) != 1)
            {
                perror("fwrite");
                close(fdin);
                return -1;
            }
            char buf[4096];
            ssize_t chk;
            while((chk = read(fdin, buf, sizeof(buf))) > 0)
            {
                if(fwrite(buf, chk, 1, fout) != 1)
                {
                    perror("fwrite");
                    close(fdin);
                    return -1;
                }
                actual_chunk_size += chk;
            }
            if(chk < 0)
            {
                perror("read");
                close(fdin);
                return -1;
            }
            close(fdin);
            uint64_t full_size = ((actual_chunk_size + 4103) & -4096) - 8;
            if(fwrite(zeroes, full_size - actual_chunk_size, 1, fout) != 1)
            {
                perror("fwrite");
                return -1;
            }
            if(full_size != chunk_size)
            {
                off_t tail = ftell(fout);
                if(fseek(fout, head, SEEK_SET))
                {
                    perror("fseek");
                    return -1;
                }
                if(fwrite(&full_size, sizeof(full_size), 1, fout) != 1)
                {
                    perror("fwrite");
                    return -1;
                }
                if(fseek(fout, tail, SEEK_SET))
                {
                    perror("fseek");
                    return -1;
                }
            }
            *payloads_size += full_size + 8;
        }
    }
    if(fwrite(zeroes, 8, 1, fout) != 1)
    {
        perror("fwrite");
        return -1;
    }
    return 0;
}

static int append_dtb(FILE* fout, const char* dtb_path, uint64_t* compressed_size, uint64_t* dtb_offset, uint64_t payloads_size)
{
    char buf[16] = {};
    if((*compressed_size) % 16 && fwrite(buf, (-*compressed_size) % 16, 1, fout) != 1)
    {
        perror("fwrite");
        return -1;
    }
    uint64_t orig_sz = *compressed_size + (-*compressed_size % 16);
    if(dtb_path)
    {
        int fd = open(dtb_path, O_RDONLY);
        if(fd < 0)
        {
            perror("open");
            return -1;
        }
        char buf[BUF_SIZE];
        for(;;)
        {
            ssize_t chk = read(fd, buf, BUF_SIZE);
            if(chk < 0)
            {
                perror("read");
                close(fd);
                return -1;
            }
            if(chk == 0)
                break;
            if(fwrite(buf, chk, 1, fout) != 1)
            {
                perror("fwrite");
                close(fd);
                return -1;
            }
        }
        close(fd);
    }
    *dtb_offset = (payload_size + payloads_size + orig_sz) << 32;
    return 0;
}

static uint64_t get_image_size(uint64_t compressed_size, uint64_t uncompressed_size, uint64_t payloads_size)
{
    return payload_size + payloads_size + compressed_size + uncompressed_size + 0x1fffff;
}

static void usage(const char* argv0)
{
    fprintf(stderr, R"(Usage: %s [<infile> <outfile> [dtb]] [--payload <payload>...]

Compresses the aarch64 kernel image at <infile> into the self-extracting kernel image at <outfile>.
If a devicetree is specified in [dtb], it is appended after the compressed data.
If no arguments are specified, stdin/stdout are used.
One or more ARM payloads might be specified. If so, the corresponding ARM code will be run before the kernel.
)", argv0);
    exit(1);
}

static void parse_argv(int argc, const char** argv, const char** input, const char** output, const char** dtb)
{
    const char** outs[3] = {input, output, dtb};
    size_t idx = 0;
    for(size_t i = 1; i < argc; i++)
    {
        if(argv[i][0] != '-')
        {
            if(idx == 3)
                usage(argv[0]);
            *outs[idx++] = argv[i];
        }
        else if(!strcmp(argv[i], "--payload") && i + 1 < argc && argv[i+1][0] != '-')
            i++;
        else
            usage(argv[0]);
    }
    if(idx == 1)
        usage(argv[0]);
    while(idx < 3)
        *outs[idx++] = 0;
}

int main(int argc, const char** argv)
{
    const char* input_filename;
    const char* output_filename;
    const char* dtb_filename;
    parse_argv(argc, argv, &input_filename, &output_filename, &dtb_filename);
    int fdin;
    FILE* fout;
    if(!input_filename)
    {
        fdin = 0;
        fout = fdopen(1, "wb");
        if(!fout)
        {
            perror("/dev/stdout");
            return 1;
        }
    }
    else
    {
        fdin = open(input_filename, O_RDONLY);
        if(fdin < 0)
        {
            perror(input_filename);
            return 1;
        }
        fout = fopen(output_filename, "wb");
        if(!fout)
        {
            perror(output_filename);
            close(fdin);
            return 1;
        }
    }
    FILE* fout_seekable = fout;
    off_t orig_pos = ftell(fout);
    char* memstream;
    size_t memstream_size;
    if(orig_pos < 0)
    {
        fout_seekable = open_memstream(&memstream, &memstream_size);
        if(!fout_seekable)
        {
            perror("open_memstream");
            fclose(fout);
            close(fdin);
            return 1;
        }
        orig_pos = 0;
    }
    uint64_t payloads_size;
    uint64_t header[10];
    memcpy(header, payload, sizeof(header));
    if((fwrite(payload, payload_size, 1, fout_seekable) != 1 && (perror("fwrite"), 1))
    || (write_payloads(fout_seekable, argc, argv, &payloads_size) && (perror("write_payloads"), 1))
    || (do_compression(fout_seekable, fdin, header+8, header+9) && (perror("do_compression"), 1))
    || (header[2] = get_image_size(header[8], header[9], payloads_size), 0)
    || (append_dtb(fout_seekable, dtb_filename, header+8, header+5, payloads_size) && (perror("append_dtb"), 1)) //see comment in crt.S
    || (fseek(fout_seekable, orig_pos, SEEK_SET) && (perror("fseek"), 1))
    || (fwrite(header, sizeof(header), 1, fout_seekable) != 1 && (perror("fwrite"), 1)))
    {
        if(fout_seekable != fout)
            fclose(fout_seekable);
        fclose(fout);
        close(fdin);
        return 1;
    }
    close(fdin);
    if(fout_seekable != fout)
    {
        fflush(fout_seekable);
        if(fwrite(memstream, memstream_size, 1, fout) != 1)
        {
            perror("fwrite");
            fclose(fout_seekable);
            fclose(fout);
            return 1;
        }
        fclose(fout_seekable);
    }
    if(fflush(fout))
    {
        perror("fflush");
        fclose(fout);
        return 1;
    }
    fclose(fout);
    return 0;
}
