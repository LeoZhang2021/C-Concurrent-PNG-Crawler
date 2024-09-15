/**
 * @brief:  prints all png files in a given directory recursively
 * @author: Zhonghao(Leo) Zhang, Computer Engineering student @ University of Waterloo
 * @property: unauthorized use is not permitted
 * @date: 2023/8/20
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

int cnt = 0;

int ftype(char *path)
{
    FILE* png_f = fopen(path, "rb");
    U8* header = malloc(8* sizeof(U8));
    fread(header, sizeof(U8), 8, png_f);

    if(is_png(header, sizeof(header)) == 0)
    {
        return 1;
    } 
    return 0;
}

void findpng(char *dir_name) 
{
    DIR *p_dir;
    struct dirent *p_dirent;

    if ((p_dir = opendir(dir_name)) == NULL) {
        // perror("directory cannot be opened");
        return;
    }

    while ((p_dirent = readdir(p_dir)) != NULL) 
    {
        char *str_path = malloc(sizeof(dir_name) + 1 + sizeof(p_dirent->d_name));

        if (strcmp(p_dirent->d_name, ".") == 0 || strcmp(p_dirent->d_name, "..") == 0)
        {
            continue;
        } 

        sprintf(str_path, "%s%s%s", dir_name, "/", p_dirent->d_name);

        if (str_path == NULL) {
            return;
        } else {
            if(ftype(str_path))
            {
                ++cnt;
                printf("%s\n", str_path);
            }
            findpng(str_path);
        }
    }

    if ( closedir(p_dir) != 0 ) {
        perror("closedir");
        return;
    }

    return;
}

int main(int argc, char *argv[]) 
{
    if (argc == 1) {
        fprintf(stderr, "Usage: %s <directory name>\n", argv[0]);
        exit(1);
    }

    findpng(argv[1]);
    if(cnt == 0)
    {
        printf("findpng: No PNG file found\n");
    }

    return 0;
}