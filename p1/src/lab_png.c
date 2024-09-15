#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "../include/lab_png.h"
#include "../include/crc.h"

int is_png(U8 *buf, size_t n)
{
    if(buf[1] == 0x50 && buf[2] == 0x4E && buf[3] == 0x47)
    {
        return 0;
    }
    return -1;
}

U32 get_png_height(struct data_IHDR *buf)
{
    return (U32)ntohl(buf->height);
}

U32 get_png_width(struct data_IHDR *buf)
{
    return (U32)ntohl(buf->width);
}

int is_IHDR(struct chunk *chunk) {
    if (chunk == NULL) {
        fprintf(stderr, "Null pointer found!");
        return 0;
    }

    if (chunk->type[0] == 0X49 && chunk->type[1] == 0X48 &&
        chunk->type[2] == 0X44 && chunk->type[3] == 0X52) {
        return 1;
    }
    return 0;
}

int is_IDAT(struct chunk *chunk) {
    if (chunk == NULL) {
        fprintf(stderr, "Null pointer found!");
        return 0;
    }
    if (chunk->type[0] == 0X49 && chunk->type[1] == 0X44 &&
        chunk->type[2] == 0X41 && chunk->type[3] == 0X54) {
        return 1;
    }
    return 0;
}

int is_IEND(struct chunk *chunk) {
    if (chunk == NULL) {
        fprintf(stderr, "Null pointer found!");
        return 0;
    }
    if (chunk->type[0] == 0X49 && chunk->type[1] == 0X45 &&
        chunk->type[2] == 0X4E && chunk->type[3] == 0X44) {
        return 1;
    }
    return 0;
}

int get_png_data_IHDR(chunk_p out, FILE *fp)
{

    fread(&out->length, sizeof(U32), 1, fp);
    //out->length = ntohl(out->length);
    fread(out->type, sizeof(U8), 4, fp);
    out->p_data = malloc(ntohl(out->length)*sizeof(U8));
    fread(out->p_data, sizeof(U8), 13, fp);
    fread(&out->crc, sizeof(U32), 1, fp);

    return 0;
}

int copy_IHDR(chunk_p out, data_IHDR_p ihdr)
{
    if(is_IHDR(out))
    {
        memcpy(&ihdr->width, &out->p_data[0], sizeof(U32));
        memcpy(&ihdr->height, &out->p_data[4], sizeof(U32));
        memcpy(&ihdr->bit_depth, &out->p_data[8], sizeof(U8));
        memcpy(&ihdr->color_type, &out->p_data[9], sizeof(U8));
        memcpy(&ihdr->compression, &out->p_data[10], sizeof(U8));
        memcpy(&ihdr->filter, &out->p_data[11], sizeof(U8));
        memcpy(&ihdr->interlace, &out->p_data[12], sizeof(U8));
        return 0;
    }
    return -1;
}

int get_png_data_IDAT(chunk_p out, FILE *fp)
{
    // copy chunk to out
    fread(&out->length, sizeof(U32), 1, fp);
    //out->length = ntohl(out->length);
    fread(out->type, sizeof(U8), 4, fp);
    out->p_data = malloc(ntohl(out->length)*sizeof(U8));
    fread(out->p_data, sizeof(U8), ntohl(out->length), fp);
    fread(&out->crc, sizeof(U32), 1, fp);

    if(is_IDAT(out))
    {
        return 0;
    }
    return -1;
}

int get_png_data_IEND(chunk_p out, FILE *fp)
{
    // copy chunk to out

    fread(&out->length, sizeof(U32), 1, fp);
    //out->length = ntohl(out->length);
    fread(out->type, sizeof(U8), 4, fp);
    out->p_data = malloc(ntohl(out->length)*sizeof(U8));
    fread(out->p_data, sizeof(U8), ntohl(out->length), fp);
    fread(&out->crc, sizeof(U32), 1, fp);

    if(is_IEND(out))
    {
        return 0;
    }
    return -1;
}

int crc_check(chunk_p chunk)
{
    unsigned char* buf = malloc(sizeof(chunk->type) + ntohl(chunk->length));
    memcpy(buf, chunk->type, sizeof(chunk->type));
    memcpy(buf + sizeof(chunk->type), chunk->p_data, ntohl(chunk->length));

    unsigned long crc_val = crc(buf, sizeof(chunk->type) + ntohl(chunk->length));

    free(buf);

    if(crc_val != ntohl(chunk->crc))
    {
        if(is_IHDR(chunk))
        {
            printf("IHDR chunk CRC error: computed %lx, expected %x\n", crc_val, ntohl(chunk->crc));
        }
        else if(is_IDAT(chunk))
        {
            printf("IDAT chunk CRC error: computed %lx, expected %x\n", crc_val, ntohl(chunk->crc));
        }
        else if(is_IEND(chunk))
        {
            printf("IEND chunk CRC error: computed %lx, expected %x\n", crc_val, ntohl(chunk->crc));
        }
        return 1;
    }
    return 0;
}