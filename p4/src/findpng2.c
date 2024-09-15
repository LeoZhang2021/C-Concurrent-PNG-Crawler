#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <search.h>
#include <pthread.h>

#include "include/png_helper.h"
#include "include/curl_helper.h"

// Define Macro
#define PNG_URL_FILE "png_urls.txt"

// Function Declarations
int crawl(char *url);

void *thread_process_url(void *arg);

int find_http(char *buf, int size, int follow_relative_links, const char *base_url);

int process_url(char *url);

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf);

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf);

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf);

int write_url(const char *path, char *in);

int read_args(int argc, char *argv[]);

void start_timing();

void end_timing();

// Gloabl Variables
Queue *url_frontier;
char *visted_url[1000];
unsigned long num_threads, num_pngs, num_working;
char *log_file;
char *seed_url;
int png_count, visted_url_count;
pthread_mutex_t png_count_mutex, url_frontier_mutex, png_url_file_mutex, visted_url_mutex, xml_mutex;
pthread_cond_t url_frontier_cond, png_count_cond;
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
    num_working = num_threads;

    url_frontier = malloc(sizeof(Queue));
    init_queue(url_frontier);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    pthread_mutex_init(&png_count_mutex, NULL);
    pthread_mutex_init(&url_frontier_mutex, NULL);
    pthread_mutex_init(&png_url_file_mutex, NULL);
    pthread_mutex_init(&visted_url_mutex, NULL);
    pthread_mutex_init(&xml_mutex, NULL);

    pthread_cond_init(&url_frontier_cond, NULL);
    pthread_cond_init(&png_count_cond, NULL);

    // Execution Time Starts
    start_timing();

    // Crawl Seed Url
    process_url(seed_url);
    crawl(seed_url);

    // Create threads
    pthread_t tid[num_threads];
    long t[num_threads];
    for (int i = 0; i < num_threads; i++) {
        t[i] = i;
        pthread_create(&tid[i], NULL, thread_process_url, &t[i]);
    }

    // Wait for all threads to exit
    for (int i = 0; i < num_threads; i++) {
        void *t_ret;
        pthread_join(tid[i], &t_ret);
        if (t_ret == (void *) -1) {
            return -1;
        }
    }
    pthread_mutex_destroy(&png_count_mutex);
    pthread_mutex_destroy(&url_frontier_mutex);
    pthread_mutex_destroy(&png_url_file_mutex);
    pthread_mutex_destroy(&visted_url_mutex);
    pthread_mutex_destroy(&xml_mutex);
    pthread_cond_destroy(&url_frontier_cond);
    pthread_cond_destroy(&png_count_cond);
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
    free(url_frontier);
    free(seed_url);
    if (log_file) free(log_file);

    // Destory Hash Table
    hdestroy();

    return 0;
}

int crawl(char *url) {
    RECV_BUF recv_buf;
    CURL *curl_handle = curl_easy_init();

    recv_buf_init(&recv_buf, BUF_SIZE);
    curl_easy_setopt_all(curl_handle, &recv_buf, (char *) url);
    if (curl_easy_perform(curl_handle) == CURLE_OK) {
        process_data(curl_handle, &recv_buf);
    }
    curl_easy_cleanup(curl_handle);
    recv_buf_cleanup(&recv_buf);
    return 0;
}

void *thread_process_url(void *arg) {
    while (1) {
        pthread_mutex_lock(&url_frontier_mutex);
        if (is_empty(url_frontier)) {
            num_working--;
            if (num_working == 0) {
                pthread_mutex_unlock(&url_frontier_mutex);
                pthread_cond_broadcast(&url_frontier_cond);
                break;
            }
            pthread_cond_wait(&url_frontier_cond, &url_frontier_mutex);

            if (num_working == 0) {
                pthread_mutex_unlock(&url_frontier_mutex);
                break;
            }
            num_working++;
        }
        pthread_mutex_unlock(&url_frontier_mutex);

        pthread_mutex_lock(&png_count_mutex);
        if (png_count >= num_pngs) {
            pthread_mutex_unlock(&png_count_mutex);
            break;
        }
        pthread_mutex_unlock(&png_count_mutex);

        // Get url from urls frontier
        pthread_mutex_lock(&url_frontier_mutex);
        char url[256];
        if (dequeue(url_frontier, url) != 0) {
            pthread_mutex_unlock(&url_frontier_mutex);
            continue;
        }
        pthread_mutex_unlock(&url_frontier_mutex);

        if (process_url(url) != 0) {
            continue;
        }

        crawl(url);
    }
    return (void *) 0;
}

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf) {
    CURLcode res;
    long response_code;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code >= 400 || response_code == 0) {
        return 1;
    }

    // Check if redirection occurred
    long redirect_count;
    curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_COUNT, &redirect_count);

    if (redirect_count > 0) {
        char *redirect_url = NULL;
        res = curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &redirect_url);
        if (res == CURLE_OK && redirect_url != NULL) {
            process_url(redirect_url);
        }
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if (res != CURLE_OK || ct == NULL) {
        return 2;
    }

    if (strstr(ct, CT_HTML)) {
        return process_html(curl_handle, p_recv_buf);
    } else if (strstr(ct, CT_PNG)) {
        return process_png(curl_handle, p_recv_buf);
    } else {
        return 1;
    }
}

int process_url(char *url) {
    ENTRY url_entry;

    url_entry.data = NULL;
    url_entry.key = malloc(256);
    strcpy(url_entry.key, url);

    pthread_mutex_lock(&visted_url_mutex);
    if (hsearch(url_entry, FIND) != NULL) { // if already visited
        free(url_entry.key);
        pthread_mutex_unlock(&visted_url_mutex);
        return 1;
    }
    hsearch(url_entry, ENTER);
    visted_url[visted_url_count] = url_entry.key;
    visted_url_count++;
    pthread_mutex_unlock(&visted_url_mutex);

    return 0;
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf) {
    char *eurl = NULL;
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);

    if (eurl != NULL && is_png((U8 *) p_recv_buf->buf) && png_count < num_pngs) {
        pthread_mutex_lock(&png_url_file_mutex);
        write_url(PNG_URL_FILE, eurl);
        pthread_mutex_unlock(&png_url_file_mutex);

        pthread_mutex_lock(&png_count_mutex);
        png_count++;
        pthread_mutex_unlock(&png_count_mutex);
    }

    return -1;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf) {
    int follow_relative_link = 1;
    char *url = NULL;

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    pthread_mutex_lock(&xml_mutex);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url);
    pthread_mutex_unlock(&xml_mutex);
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
                pthread_mutex_lock(&url_frontier_mutex);
                enqueue(url_frontier, (char *) href);
                pthread_cond_signal(&url_frontier_cond);
                pthread_mutex_unlock(&url_frontier_mutex);
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
    num_threads = 1;
    num_pngs = 50;
    log_file = NULL;

    while ((c = getopt(argc, argv, "t:m:v:")) != -1) {
        switch (c) {
            case 't':
                num_threads = strtoul(optarg, NULL, 10);
                if (num_threads <= 0) {
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
    printf("findpng2 execution time: %.6lf seconds\n", times[1] - times[0]);
}