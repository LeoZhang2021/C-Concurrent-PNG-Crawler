#include <stdlib.h>
#include <string.h>

#include "include/queue.h"

void init_queue(Queue *queue) {
    queue->size = 0;
    queue->front = NULL;
    queue->rear = NULL;
}

void destroy_queue(Queue *queue) {
    char dummy[256];
    while (!is_empty(queue)) {
        dequeue(queue, dummy);
    }
}

int is_empty(Queue *queue) {
    return queue->size == 0;
}

void enqueue(Queue *queue, char *data) {
    Node *new_node = malloc(sizeof(Node));
    strcpy(new_node->data, data);
    new_node->next = NULL;

    if (is_empty(queue)) {
        queue->front = new_node;
        queue->rear = new_node;
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    queue->size++;
}

int dequeue(Queue *queue, char *out_data) {
    if (is_empty(queue)) {
        return 1;
    }

    Node *temp = queue->front;
    strcpy(out_data, temp->data);
    if (queue->front == queue->rear) {
        queue->front = NULL;
        queue->rear = NULL;
    } else {
        queue->front = queue->front->next;
    }

    queue->size--;
    free(temp);

    return 0;
}

void init_vqueue(VQueue *queue) {
    queue->size = 0;
    queue->front = NULL;
    queue->rear = NULL;
}

int is_vempty(VQueue *queue) {
    return queue->size == 0;
}

void venqueue(VQueue *queue, void *data) {
    VNode *new_node = malloc(sizeof(VNode));
    new_node->data = data;
    new_node->next = NULL;

    if (is_vempty(queue)) {
        queue->front = new_node;
        queue->rear = new_node;
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    queue->size++;
}

void *vdequeue(VQueue *queue) {
    if (is_vempty(queue)) {
        return NULL;
    }

    VNode *temp = queue->front;
    void *out_data = temp->data;
    if (queue->front == queue->rear) {
        queue->front = NULL;
        queue->rear = NULL;
    } else {
        queue->front = queue->front->next;
    }

    queue->size--;
    free(temp);

    return out_data;
}
