#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "include/crc.h"
#include "include/png_helper.h"

int is_png(const U8 *buf) {
    // Check if the PNG Header is valid
    if (buf[0] != 0x89 || buf[1] != 0x50 || buf[2] != 0x4E || buf[3] != 0x47 ||
        buf[4] != 0x0D || buf[5] != 0x0A || buf[6] != 0x1A || buf[7] != 0x0A) {
        return 0;
    } else {
        return 1;
    }
}

int is_IHDR(struct chunk *chunk) {
    if (!chunk) {
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
    if (!chunk) {
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
    if (!chunk) {
        fprintf(stderr, "Null pointer found!");
        return 0;
    }
    if (chunk->type[0] == 0X49 && chunk->type[1] == 0X45 &&
        chunk->type[2] == 0X4E && chunk->type[3] == 0X44) {
        return 1;
    }
    return 0;
}

void init_simple_PNG(struct simple_PNG *png) {
    png->p_IHDR = malloc(sizeof(struct chunk));
    init_chunk(png->p_IHDR);
    png->p_IDAT = malloc(sizeof(struct chunk));
    init_chunk(png->p_IDAT);
    png->p_IEND = malloc(sizeof(struct chunk));
    init_chunk(png->p_IEND);
}

void free_simple_PNG(struct simple_PNG *png) {
    if (png->p_IHDR) free_chunk(png->p_IHDR);
    if (png->p_IDAT) free_chunk(png->p_IDAT);
    if (png->p_IEND) free_chunk(png->p_IEND);
}

void init_chunk(struct chunk *chunk) {
    if (chunk) {
        chunk->length = 0;
        memset(chunk->type, 0, CHUNK_TYPE_SIZE);
        chunk->p_data = NULL;
        chunk->crc = 0;
    }
}

void free_chunk(struct chunk *chunk) {
    if (chunk->p_data) free(chunk->p_data);
    chunk->p_data = NULL;
    free(chunk);
    chunk = NULL;
}

void memcpy_chunk(struct chunk *dest, struct chunk *source) {
    dest->length = source->length;
    memcpy(dest->type, source->type, CHUNK_TYPE_SIZE);
    if (dest->p_data) free(dest->p_data);
    dest->p_data = malloc(source->length);
    memcpy(dest->p_data, source->p_data, source->length);
    dest->crc = source->crc;
}

int get_png_data_IHDR(struct data_IHDR *out, struct chunk *chunk_IHDR) {
    memcpy(&out->width, &chunk_IHDR->p_data[0], sizeof(U8) * 4);
    out->width = (U32) ntohl(out->width);
    memcpy(&out->height, &chunk_IHDR->p_data[4], sizeof(U8) * 4);
    out->height = (U32) ntohl(out->height);
    memcpy(&out->bit_depth, &chunk_IHDR->p_data[8], sizeof(U8));
    memcpy(&out->color_type, &chunk_IHDR->p_data[9], sizeof(U8));
    memcpy(&out->compression, &chunk_IHDR->p_data[10], sizeof(U8));
    memcpy(&out->filter, &chunk_IHDR->p_data[11], sizeof(U8));
    memcpy(&out->interlace, &chunk_IHDR->p_data[12], sizeof(U8));

    return 0;
}

int read_png_chunk(struct chunk *out_chunk, U8 *buf, size_t *cur_idx) {
    memcpy(&out_chunk->length, &buf[*cur_idx], CHUNK_LEN_SIZE);
    out_chunk->length = ntohl(out_chunk->length);
    *cur_idx += CHUNK_LEN_SIZE;

    memcpy(out_chunk->type, &buf[*cur_idx], 4 * sizeof(U8));
    *cur_idx += 4 * sizeof(U8);

    out_chunk->p_data = malloc(out_chunk->length * sizeof(U8));
    if (!out_chunk->p_data) {
        perror("malloc");
        free_chunk(out_chunk);
        return -1;
    }

    memcpy(out_chunk->p_data, &buf[*cur_idx], out_chunk->length * sizeof(U8));
    *cur_idx += out_chunk->length * sizeof(U8);

    memcpy(&out_chunk->crc, &buf[*cur_idx], CHUNK_CRC_SIZE);
    *cur_idx += CHUNK_CRC_SIZE;

    return 0;
}

int read_png(struct simple_PNG *out_png, struct data_IHDR *out_ihdr_data, U8 *buf, size_t buf_size) {
    size_t cur_idx = 0;
    while (cur_idx < buf_size) {
        struct chunk *png_chunk = malloc(sizeof(struct chunk));
        if (!png_chunk) {
            free(png_chunk);
            perror("malloc");
            return -1;
        }
        init_chunk(png_chunk);
        if (read_png_chunk(png_chunk, buf, &cur_idx) < 0) {
            free_chunk(png_chunk);
            fprintf(stderr, "Failed reading chunk!");
            return -1;
        }
        if (is_IHDR(png_chunk)) {
            out_png->p_IHDR = png_chunk;
            get_png_data_IHDR(out_ihdr_data, out_png->p_IHDR);
        } else if (is_IDAT(png_chunk)) {
            out_png->p_IDAT = png_chunk;
        } else if (is_IEND(png_chunk)) {
            out_png->p_IEND = png_chunk;
            break;
        } else free_chunk(png_chunk);
    }
    return 0;
}