#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>

#include "queue.h"


#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
    /* <0 indicates an invalid seq number */
} RECV_BUF;

htmlDocPtr mem_getdoc(char *buf, int size, const char *url);

xmlXPathObjectPtr getnodeset(xmlDocPtr doc, xmlChar *xpath);

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);

int recv_buf_init(RECV_BUF *ptr, size_t max_size);

int recv_buf_cleanup(RECV_BUF *ptr);

int write_file(const char *path, const void *in, size_t len);

void curl_easy_setopt_all(CURL *curl, RECV_BUF *ptr, const char *url);