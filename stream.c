#include "lz4.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_SIZE 1024 * 8 // 8KB

size_t write_int(FILE *fp, int i) {
    return fwrite(&i, sizeof(i), 1, fp);
}

size_t write_bin(FILE *fp, const void *array, size_t arrayBytes) {
    return fwrite(array, 1, arrayBytes, fp);
}

size_t read_int(FILE *fp, int *i) {
    return fread(i, sizeof(*i), 1, fp);
}

size_t read_bin(FILE *fp, void *array, size_t arrayBytes) {
    return fread(array, 1, arrayBytes, fp);
}

void test_compress(FILE *outFp, FILE *inpFp) {
    assert(outFp != NULL);
    assert(inpFp != NULL);

    LZ4_stream_t lz4Stream_body;
    LZ4_stream_t *lz4Stream = &lz4Stream_body;

    char inpBuf[2][CHUNK_SIZE];
    int inpBufIndex = 0;

    LZ4_initStream(lz4Stream, sizeof(*lz4Stream));

    for (;;) {
        char *const inpPtr = inpBuf[inpBufIndex];
        const int inpBytes = (int)read_bin(inpFp, inpPtr, CHUNK_SIZE);
        if (0 == inpBytes) {
            break;
        }

        {
            char cmpBuf[LZ4_COMPRESSBOUND(CHUNK_SIZE)];
            const int cmpBytes = LZ4_compress_fast_continue(lz4Stream, inpPtr, cmpBuf, inpBytes, sizeof(cmpBuf), 1);
            if (cmpBytes <= 0) {
                break;
            }
            write_int(outFp, cmpBytes);
            write_bin(outFp, cmpBuf, (size_t)cmpBytes);
        }

        inpBufIndex = (inpBufIndex + 1) % 2;
    }

    write_int(outFp, 0);
}

void test_decompress(FILE *outFp, FILE *inpFp) {
    assert(outFp != NULL);
    assert(inpFp != NULL);

    LZ4_streamDecode_t lz4StreamDecode_body;
    LZ4_streamDecode_t *lz4StreamDecode = &lz4StreamDecode_body;

    char decBuf[2][CHUNK_SIZE];
    int decBufIndex = 0;

    LZ4_setStreamDecode(lz4StreamDecode, NULL, 0);

    for (;;) {
        char cmpBuf[LZ4_COMPRESSBOUND(CHUNK_SIZE)];
        int cmpBytes = 0;

        {
            const size_t readCount0 = read_int(inpFp, &cmpBytes);
            if (readCount0 != 1 || cmpBytes <= 0) {
                break;
            }

            const size_t readCount1 = read_bin(inpFp, cmpBuf, (size_t)cmpBytes);
            if (readCount1 != (size_t)cmpBytes) {
                break;
            }
        }

        {
            char *const decPtr = decBuf[decBufIndex];
            const int decBytes = LZ4_decompress_safe_continue(lz4StreamDecode, cmpBuf, decPtr, cmpBytes, CHUNK_SIZE);
            if (decBytes <= 0) {
                break;
            }
            write_bin(outFp, decPtr, (size_t)decBytes);
        }

        decBufIndex = (decBufIndex + 1) % 2;
    }
}

int compare(FILE *fp0, FILE *fp1) {
    int result = 0;

    assert(fp0 != NULL);
    assert(fp1 != NULL);

    while (0 == result) {
        char b0[65536];
        char b1[65536];
        const size_t r0 = read_bin(fp0, b0, sizeof(b0));
        const size_t r1 = read_bin(fp1, b1, sizeof(b1));

        result = (int)r0 - (int)r1;

        if (0 == r0 || 0 == r1) {
            break;
        }
        if (0 == result) {
            result = memcmp(b0, b1, r0);
        }
    }

    return result;
}

int main(int argc, char *argv[]) {
    char inpFilename[128] = {0};
    char lz4Filename[256] = {0};
    char decFilename[256] = {0};

    snprintf(inpFilename, 256, "%s", "fontawesome-webfont.bin");
    snprintf(lz4Filename, 256, "lz4s-%d.%s", CHUNK_SIZE, inpFilename);
    snprintf(decFilename, 256, "lz4s-%d.dec.%s", CHUNK_SIZE, inpFilename);

    // compress
    {
        FILE *inpFp = fopen(inpFilename, "rb");
        FILE *outFp = fopen(lz4Filename, "wb");

        printf("compress : %s -> %s\n", inpFilename, lz4Filename);
        test_compress(outFp, inpFp);
        printf("compress : done\n");

        fclose(outFp);
        fclose(inpFp);
    }

    // decompress
    {
        FILE *inpFp = fopen(lz4Filename, "rb");
        FILE *outFp = fopen(decFilename, "wb");

        printf("decompress : %s -> %s\n", lz4Filename, decFilename);
        test_decompress(outFp, inpFp);
        printf("decompress : done\n");

        fclose(outFp);
        fclose(inpFp);
    }

    // verify
    {
        FILE *inpFp = fopen(inpFilename, "rb");
        FILE *decFp = fopen(decFilename, "rb");

        printf("verify : %s <-> %s\n", inpFilename, decFilename);
        const int cmp = compare(inpFp, decFp);
        if (0 == cmp) {
            printf("verify : OK\n");
        } else {
            printf("verify : NG\n");
        }

        fclose(decFp);
        fclose(inpFp);
    }

    return 0;
}