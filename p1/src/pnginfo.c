/**
 * @brief:  checks validity of a png file
            outputs dimensions of a png file
            checks is png file is corrupted
 * @author: Zhonghao(Leo) Zhang, Computer Engineering student @ University of Waterloo
 * @property: unauthorized use is not permitted
 * @date: 2023/8/20
**/

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "../include/lab_png.h"
#include "../include/crc.h"

int main(int argc, char** argv)
{
    if( argc < 2 ) {
        perror("insufficient argument");
        return 0;
    }

    printf("%s: ", argv[1]);

    FILE* png_f = fopen(argv[1], "rb");
    U8* header = malloc(8* sizeof(U8));
    fread(header, sizeof(U8), 8, png_f);

    if(is_png(header, sizeof(header)) == -1)
    {
        printf("Not a PNG file\n");
        return 0;
    }

    struct data_IHDR* IHDR = malloc(13*sizeof(char));
    chunk_p ihdr_chunk = malloc(sizeof(struct chunk));
    chunk_p idat_chunk = malloc(sizeof(struct chunk));
    chunk_p iend_chunk = malloc(sizeof(struct chunk));

    get_png_data_IHDR(ihdr_chunk, png_f);
    get_png_data_IDAT(idat_chunk, png_f);
    get_png_data_IEND(iend_chunk, png_f);

    copy_IHDR(ihdr_chunk, IHDR);

    int h = get_png_height(IHDR);
    int w = get_png_width(IHDR);
    printf("%d x %d\n", w, h);

    crc_check(ihdr_chunk);
    crc_check(idat_chunk);
    crc_check(iend_chunk);

    fclose(png_f);

    FILE* p = fopen(argv[1], "rb");
    U32 s;
    fseek(p, 8*sizeof(U8), SEEK_SET);
    fread(&s, sizeof(U32), 1, p);

    fclose(p);

    return 0;
}