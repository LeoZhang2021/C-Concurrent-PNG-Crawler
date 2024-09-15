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

void init_queue(Queue *queue);

void destroy_queue(Queue *queue);

int is_empty(Queue *queue);

void enqueue(Queue *queue, char *data);

int dequeue(Queue *queue, char *out_data);
