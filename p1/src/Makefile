RM = rm -rf
CC = gcc

all: findpng pnginfo catpng

findpng: findpng.c lab_png.c crc.c 
	$(CC) findpng.c lab_png.c crc.c -o findpng

pnginfo: pnginfo.c crc.c lab_png.c 
	$(CC) pnginfo.c crc.c lab_png.c -o pnginfo

catpng: catpng.c crc.c lab_png.c zutil.c
	$(CC) catpng.c crc.c lab_png.c zutil.c -o catpng -lz

.PHONY: clean
clean:
	$(RM) findpng pnginfo catpng
	$(RM) -rf "all.png"