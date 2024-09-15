#include <stdio.h>
#include <stdlib.h>

typedef struct node {
    char data[256];
    struct node *next;
} Node;

typedef struct queue {
    int size;
    Node *front;
    Node *rear;
} Queue;

typedef struct void_node {
    void *data;
    struct void_node *next;
} VNode;

typedef struct void_queue {
    int size;
    VNode *front;
    VNode *rear;
} VQueue;

void init_queue(Queue *queue);

void destroy_queue(Queue *queue);

int is_empty(Queue *queue);

void enqueue(Queue *queue, char *data);

int dequeue(Queue *queue, char *out_data);

void init_vqueue(VQueue *queue);

int is_vempty(VQueue *queue);

void venqueue(VQueue *queue, void *data);

void* vdequeue(VQueue *queue);