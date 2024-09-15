/**
 * @brief: concatenate multiple png files into one png file
 * @author: Zhonghao(Leo) Zhang, Computer Engineering student @ University of Waterloo
 * @property: unauthorized use is not permitted
 * @date: 2023/8/23
**/

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h> 

#include "../include/lab_png.h"
#include "../include/zutil.h"
#include "../include/crc.h"

int readpng(char* path, simple_PNG_p png_ind)
{
    FILE* png_f = fopen(path, "rb");
    U8* header = malloc(8* sizeof(U8));
    fread(header, sizeof(U8), 8, png_f);

    if(is_png(header, sizeof(header)) == -1)
    {
        fprintf(stderr, "%s is not a PNG file\n", path);
        return -1;
    }

    png_ind->p_IHDR = malloc(sizeof(struct chunk));
    png_ind->p_IDAT = malloc(sizeof(struct chunk));
    png_ind->p_IEND = malloc(sizeof(struct chunk));

    get_png_data_IHDR(png_ind->p_IHDR, png_f);
    get_png_data_IDAT(png_ind->p_IDAT, png_f);
    get_png_data_IEND(png_ind->p_IEND, png_f);

    fclose(png_f);
    free(header);

    return 0;
}

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        fprintf(stderr, "insufficient arguments");
        exit(1);
    }

    // list of png waiting to be concatenated
    simple_PNG_p* png_ls = malloc((argc - 1)*sizeof(simple_PNG_p));
    data_IHDR_p* IHDR_ls = malloc((argc - 1)*sizeof(data_IHDR_p));

    // size infos
    size_t total_def_len = 0;
    size_t total_inf_len = 0;
    U32 total_height = 0;

    // read each png file into chunk and IHDR
    for(int i = 0; i < argc - 1; ++i)
    {
        png_ls[i] = malloc(sizeof(struct simple_PNG));
        readpng(argv[i + 1], png_ls[i]);

        IHDR_ls[i] = malloc(sizeof(struct data_IHDR));
        copy_IHDR(png_ls[i]->p_IHDR, IHDR_ls[i]);

        // calculate inflated data length
        total_inf_len += ntohl(IHDR_ls[i]->height)*(ntohl(IHDR_ls[i]->width)*4 + 1);
        total_height += ntohl(IHDR_ls[i]->height);
    }

    U8* inf_png = malloc(total_inf_len);
    size_t cur_ind = 0;

    // inflate IDAT data and sum into single pixel array
    for(int i = 0; i < argc - 1; ++i)
    {
        U64 len = 0;
        mem_inf(inf_png + cur_ind, &len, png_ls[i]->p_IDAT->p_data, ntohl(IHDR_ls[i]->height)*(ntohl(IHDR_ls[i]->width)*4 + 1));
        cur_ind += ntohl(IHDR_ls[i]->height)*(ntohl(IHDR_ls[i]->width)*4 + 1);
    }

    // deflate IDAT data
    U64 def_len = 0;
    U8* def_png = malloc(total_inf_len);
    mem_def(def_png, &def_len, inf_png, total_inf_len, Z_DEFAULT_COMPRESSION);

    // create output png file
    FILE *out_png = fopen("all.png", "wb");

    /* 
    IHDR change in Data->height
    IDAT change in Length, Data and CRC
    IEND no change
    */
    
    // write header
    U8 png_header[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(png_header, sizeof(U8), 8, out_png);

    // write IHDR
    
    fwrite(&png_ls[0]->p_IHDR->length, sizeof(U32), 1, out_png);
    fwrite(png_ls[0]->p_IHDR->type, sizeof(U8), 4, out_png);

    total_height = htonl(total_height);
    memcpy(&png_ls[0]->p_IHDR->p_data[4], &total_height, sizeof(U32));
    fwrite(png_ls[0]->p_IHDR->p_data, 13*sizeof(U8), 1, out_png);

    unsigned char* buf = malloc(4 + 13);
    memcpy(buf, png_ls[0]->p_IHDR->type, 4);
    memcpy(buf + 4, png_ls[0]->p_IHDR->p_data, 13);

    U32 crc_val = htonl(crc(buf, 4 + 13));
    fwrite(&crc_val, sizeof(U32), 1, out_png);

    // write IDAT
    U32 IDAT_len = (U32)htonl(def_len);
    fwrite(&IDAT_len, sizeof(U32), 1, out_png);
    fwrite(png_ls[0]->p_IDAT->type, sizeof(U8), 4, out_png);
    fwrite(def_png, sizeof(U8), ntohl(IDAT_len), out_png);

    unsigned char* buf2 = malloc(sizeof(png_ls[0]->p_IDAT->type) + ntohl(IDAT_len));
    memcpy(buf2, png_ls[0]->p_IDAT->type, sizeof(png_ls[0]->p_IDAT->type));
    memcpy(buf2 + sizeof(png_ls[0]->p_IDAT->type), def_png, ntohl(IDAT_len));

    U32 crc_val2 = htonl(crc(buf2, sizeof(png_ls[0]->p_IDAT->type) + ntohl(IDAT_len)));
    fwrite(&crc_val2, sizeof(U32), 1, out_png);

    // write IEND
    fwrite(&png_ls[0]->p_IEND->length, sizeof(U32), 1, out_png);
    fwrite(png_ls[0]->p_IEND->type, sizeof(U8), 4, out_png);
    //data field = 0 bytes in IEND
    fwrite(&png_ls[0]->p_IEND->crc, sizeof(U32), 1, out_png);

    fclose(out_png);

    // cleanup

    free(inf_png);
    free(def_png);
    free(buf);
    free(buf2);

    for(int i = 0; i < argc - 1; ++i)
    {
        free(IHDR_ls[i]);
        free(png_ls[i]->p_IHDR->p_data);
        free(png_ls[i]->p_IDAT->p_data);
        free(png_ls[i]->p_IEND->p_data);
        free(png_ls[i]->p_IHDR);
        free(png_ls[i]->p_IDAT);
        free(png_ls[i]->p_IEND);
        free(png_ls[i]);
    }

    free(IHDR_ls);
    free(png_ls);

    return 0;
}