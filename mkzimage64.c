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

static int append_dtb(FILE* fout, const char* dtb_path, uint64_t* compressed_size, uint64_t* dtb_offset)
{
    char buf[16] = {};
    if((*compressed_size) % 16 && fwrite(buf, (-*compressed_size) % 16, 1, fout) != 1)
    {
        perror("fwrite");
        return -1;
    }
    *compressed_size += (-*compressed_size) % 16;
    uint64_t orig_sz = *compressed_size;
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
            *compressed_size += chk;
            if(fwrite(buf, chk, 1, fout) != 1)
            {
                perror("fwrite");
                close(fd);
                return -1;
            }
        }
        close(fd);
    }
    *dtb_offset = (payload_size + orig_sz) << 32;
    return 0;
}

uint64_t get_image_size(uint64_t compressed_size, uint64_t uncompressed_size)
{
    return payload_size + compressed_size + uncompressed_size + 0x1fffff;
}

int main(int argc, const char** argv)
{
    int fdin;
    FILE* fout;
    if(argc <= 1)
    {
        fdin = 0;
        fout = fdopen(1, "wb");
        if(!fout)
        {
            perror("/dev/stdout");
            return 1;
        }
    }
    else if((argc == 3 || argc == 4) && argv[1][0] != '-' && argv[2][0] != '-' && (argc == 3 || argv[3][0] != '-'))
    {
        fdin = open(argv[1], O_RDONLY);
        if(fdin < 0)
        {
            perror(argv[1]);
            return 1;
        }
        fout = fopen(argv[2], "wb");
        if(!fout)
        {
            perror(argv[2]);
            close(fdin);
            return 1;
        }
    }
    else
    {
        fprintf(stderr, R"(Usage: %s [<infile> <outfile> [dtb]]

Compresses the aarch64 kernel image at <infile> into the self-extracting kernel image at <outfile>.
If a devicetree is specified in [dtb], it is appended after the compressed data.
If no arguments are specified, stdin/stdout are used.
)", argv[0]);
        return 1;
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
    uint64_t header[10];
    memcpy(header, payload, sizeof(header));
    if((fwrite(payload, payload_size, 1, fout_seekable) != 1 && (perror("fwrite"), 1))
    || (do_compression(fout_seekable, fdin, header+8, header+9) && (perror("do_compression"), 1))
    || (header[2] = get_image_size(header[8], header[9]), 0)
    || append_dtb(fout_seekable, argc >= 4 ? argv[3] : 0, header+8, header+5) //see comment in crt.S
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
