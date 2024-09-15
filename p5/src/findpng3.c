#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <search.h>

#include "include/png_helper.h"
#include "include/curl_helper.h"

// Define Macro
#define PNG_URL_FILE "png_urls.txt"
#define MAX_WAIT_MSECS 30000

// Function Declarations
int find_http(char *buf, int size, int follow_relative_links, const char *base_url);

int process_url(char *url);

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf);

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf);

int write_url(const char *path, char *in);

int read_args(int argc, char *argv[]);

void start_timing();

void end_timing();

// Gloabl Variables
Queue *url_frontier;
char *visted_url[1000];
CURLM *multi_handler;
unsigned long num_conn, num_pngs;
char *log_file;
char *seed_url;
int png_count, visted_url_count, still_running, msgs_left;
double times[2];
struct timeval tv;

int main(int argc, char *argv[]) {
    // Read Arguments
    if (read_args(argc, argv) != 0) return -1;

    // Create Empty png url file & log file
    write_file(PNG_URL_FILE, "", 0);
    if (log_file) {
        write_file(log_file, "", 0);
    }

    // Create Hash Table
    hcreate(10000);

    // Initialize Global Variables
    png_count = 0;
    visted_url_count = 0;
    still_running = 0;
    msgs_left = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    multi_handler = curl_multi_init();

    url_frontier = malloc(sizeof(Queue));
    init_queue(url_frontier);
    enqueue(url_frontier, (char *) seed_url);

    VQueue *curl_recv_buf_queue = malloc(sizeof(VQueue));
    CURL_RECV_BUF *curl_recv_buf_list[num_conn];
    init_vqueue(curl_recv_buf_queue);
    for (int i = 0; i < num_conn; i++) {
        curl_recv_buf_list[i] = malloc(sizeof(CURL_RECV_BUF));
        curl_recv_buf_init(curl_recv_buf_list[i]);
        venqueue(curl_recv_buf_queue, curl_recv_buf_list[i]);
    }

    // Execution Time Starts
    start_timing();

    while (1) {
        if (png_count >= num_pngs) {
            break;
        }

        while (!is_empty(url_frontier) && !is_vempty(curl_recv_buf_queue)) {
            // Add to Multi Handler
            char url[256];
            dequeue(url_frontier, url);
            if (process_url(url) != 0) {
                continue;
            }

            CURL_RECV_BUF *curl_recv_buf = vdequeue(curl_recv_buf_queue);
            curl_recv_buf->in_use = 1;
            recv_buf_init(curl_recv_buf->recv_buf, BUF_SIZE);
            curl_easy_setopt_all(curl_recv_buf, url);
            curl_multi_add_handle(multi_handler, curl_recv_buf->curl);
            curl_multi_perform(multi_handler, &still_running);
        }

        if (is_empty(url_frontier) && curl_recv_buf_queue->size == num_conn) {
            break;
        }

        int numfds = 0;
        if (curl_multi_wait(multi_handler, NULL, 0, MAX_WAIT_MSECS, &numfds) != CURLM_OK) {
            fprintf(stderr, "error: curl_multi_wait()\n");
            return EXIT_FAILURE;
        }

        curl_multi_perform(multi_handler, &still_running);

        CURLMsg *msg = NULL;
        while ((msg = curl_multi_info_read(multi_handler, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                CURL_RECV_BUF *curl_recv_buf = NULL;
                CURLcode return_code;
                CURL *curl_handle = msg->easy_handle;
                return_code = curl_easy_getinfo(curl_handle, CURLINFO_PRIVATE, &curl_recv_buf);

                long response_code;
                return_code = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

                if (response_code >= 400 || response_code == 0) {
                } else {
                    // Check if redirection occurred
                    long redirect_count = 0;
                    curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_COUNT, &redirect_count);

                    if (redirect_count > 0) {
                        char *redirect_url = NULL;
                        return_code = curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &redirect_url);
                        if (return_code == CURLE_OK && redirect_url != NULL) {
                            process_url(redirect_url);
                        }
                    }

                    char *ct = NULL;
                    return_code = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);

                    if (strstr(ct, CT_HTML)) {
                        process_html(curl_handle, curl_recv_buf->recv_buf);
                    } else if (strstr(ct, CT_PNG)) {
                        process_png(curl_handle, curl_recv_buf->recv_buf);
                    }
                }
                curl_multi_remove_handle(multi_handler, curl_handle);
                recv_buf_cleanup(curl_recv_buf->recv_buf);
                curl_recv_buf->in_use = 0;
                venqueue(curl_recv_buf_queue, curl_recv_buf);
            }
        }
    }

    while (!is_vempty(curl_recv_buf_queue)) {
        vdequeue(curl_recv_buf_queue);
    }

    for (int i = 0; i < num_conn; i++) {
        if (curl_recv_buf_list[i]->in_use) {
            curl_multi_remove_handle(multi_handler, curl_recv_buf_list[i]->curl);
            recv_buf_cleanup(curl_recv_buf_list[i]->recv_buf);
        }
        curl_recv_buf_cleanup(curl_recv_buf_list[i]);
        free(curl_recv_buf_list[i]);
    }
    curl_multi_cleanup(multi_handler);
    curl_global_cleanup();

    // Print Execution Time
    end_timing();

    // Free hsearch entries & log urls
    if (log_file) {
        FILE *fp = fopen(log_file, "a");
        if (fp == NULL) {
            perror("fopen");
            return -2;
        }
        for (int i = 0; i < visted_url_count; i++) {
            fprintf(fp, "%s\n", visted_url[i]);
            free(visted_url[i]);
        }
        fclose(fp);
    } else {
        for (int i = 0; i < visted_url_count; i++) {
            free(visted_url[i]);
        }
    }

    // Free all the allocated memories
    destroy_queue(url_frontier);
    free(curl_recv_buf_queue);
    free(url_frontier);
    free(seed_url);
    if (log_file) free(log_file);

    // Destory Hash Table
    hdestroy();

    return EXIT_SUCCESS;
}

int process_url(char *url) {
    ENTRY url_entry;

    url_entry.data = NULL;
    url_entry.key = malloc(256);
    strcpy(url_entry.key, url);

    if (hsearch(url_entry, FIND) != NULL) { // if already visited
        free(url_entry.key);
        return 1;
    }
    hsearch(url_entry, ENTER);
    visted_url[visted_url_count] = url_entry.key;
    visted_url_count++;

    return 0;
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf) {
    char *eurl = NULL;
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);

    if (eurl != NULL && is_png((U8 *) p_recv_buf->buf) && png_count < num_pngs) {
        write_url(PNG_URL_FILE, eurl);
        png_count++;
    }

    return -1;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf) {
    int follow_relative_link = 1;
    char *eurl = NULL;
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, eurl);
    return 0;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url) {
    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar *) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset(doc, xpath);
    if (result) {
        nodeset = result->nodesetval;

        for (i = 0; i < nodeset->nodeNr; i++) {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);

            if (follow_relative_links) {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }

            if (href != NULL && !strncmp((const char *) href, "http", 4)) {
                enqueue(url_frontier, (char *) href);
            }
            xmlFree(href);
        }

        xmlXPathFreeObject(result);
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}

int write_url(const char *path, char *in) {
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "a");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }
    fprintf(fp, "%s\n", in);

    return fclose(fp);
}

int read_args(int argc, char *argv[]) {
    int c;
    char *str = "option requires an argument";

    if (argc < 2) {
        fprintf(stderr, "Minimum number of arguments required: 1\n");
        return -1;
    }

    // Initialize global variables before reading arguments
    num_conn = 1;
    num_pngs = 50;
    log_file = NULL;

    while ((c = getopt(argc, argv, "t:m:v:")) != -1) {
        switch (c) {
            case 't':
                num_conn = strtoul(optarg, NULL, 10);
                if (num_conn <= 0) {
                    fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                    return -1;
                }
                break;
            case 'm':
                num_pngs = strtoul(optarg, NULL, 10);
                if (num_pngs < 0) {
                    fprintf(stderr, "%s: %s >= 0 -- 'm'\n", argv[0], str);
                    return -1;
                }
                break;
            case 'v':
                log_file = malloc(256);
                strcpy(log_file, optarg);
                break;
            default:
                return -1;
        }
    }
    seed_url = malloc(256);
    strcpy(seed_url, argv[argc - 1]);

    return 0;
}

void start_timing() {
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec / 1000000.;
}

void end_timing() {
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec / 1000000.;
    printf("findpng3 execution time: %.6lf seconds\n", times[1] - times[0]);
}