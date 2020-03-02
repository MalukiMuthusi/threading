

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include "../utility/utils.h"
#include "../sem/sem.h"

#define NUM_CHILDREN 2
#define NUM_ITEMS 127

// QUEUE

typedef struct {
    int *array; // array of integers
    int length; // length of queue
    int next_in; // first item
    int next_out; // last item
    Semaphore *mutex;
    Semaphore *items; // number of items in the queue = number of queue_pop per time
    Semaphore *spaces; // number of empty spaces in Queue = number of queue_push per time
} Queue;

// function to make the Queue
Queue *make_queue(int length) {
    Queue *queue = (Queue *) malloc(sizeof(Queue));
    queue->length = length;
    queue->array = (int *) malloc(length * sizeof(int));
    queue->next_in = 0;
    queue->next_out = 0;
    queue->mutex = make_semaphore(1); // it is unlocked
    queue->items = make_semaphore(0); // locked.
    queue->spaces = make_semaphore(length - 1);
    return queue;
}

int queue_incr(Queue *queue, int i) {
    return (i + 1) % queue->length;
}

int queue_empty(Queue *queue) {
    // queue is empty if next_in and next_out are the same
    int res = (queue->next_in == queue->next_out);
    return res;
}

int queue_full(Queue *queue) {
    // queue is full if incrementing next_in lands on next_out
    int res = (queue_incr(queue, queue->next_in) == queue->next_out);
    return res;
}

void queue_push(Queue *queue, int item) {
    semaphore_wait(queue->spaces);
    semaphore_wait(queue->mutex);

    queue->array[queue->next_in] = item;
    queue->next_in = queue_incr(queue, queue->next_in);

    semaphore_signal(queue->mutex);
    semaphore_signal(queue->items);
}

int queue_pop(Queue *queue) {
    semaphore_wait(queue->items);
    semaphore_wait(queue->mutex);

    int item = queue->array[queue->next_out];
    queue->next_out = queue_incr(queue, queue->next_out);

    semaphore_signal(queue->mutex);
    semaphore_signal(queue->spaces);

    return item;
}

// SHARED

typedef struct {
    Queue *queue;
} Shared;

Shared *make_shared() {
    Shared *shared = check_malloc(sizeof(Shared));
    shared->queue = make_queue(128); // queue of size 128
    return shared;
}

// THREAD

pthread_t make_thread(void *(*entry)(void *), Shared *shared) {
    int ret;
    pthread_t thread;

    ret = pthread_create(&thread, NULL, entry, (void *) shared);
    if (ret != 0) {
        perror_exit("pthread_create failed");
    }
    return thread;
}

void join_thread(pthread_t thread) {
    int ret = pthread_join(thread, NULL);
    if (ret == -1) {
        perror_exit("pthread_join failed");
    }
}

// PRODUCER-CONSUMER

void *producer_entry(void *arg) {
    int i;
    Shared *shared = (Shared *) arg;
    for (i = 0; i < NUM_ITEMS; i++) {
        printf("adding item %d\n", i);
        queue_push(shared->queue, i);
    }
    pthread_exit(NULL);
}

void *consumer_entry(void *arg) {
    int i;
    int item;
    Shared *shared = (Shared *) arg;

    for (i = 0; i < NUM_ITEMS; i++) {
        item = queue_pop(shared->queue);
        printf("consuming item %d\n", item);
    }
    pthread_exit(NULL);
}

// TEST CODE

void queue_test() {
    int i;
    int item;
    int length = 128;

    Queue *queue = make_queue(length);
    assert(queue_empty(queue));
    for (i = 0; i < length - 1; i++) {
        queue_push(queue, i);
    }
    assert(queue_full(queue));
    for (i = 0; i < 10; i++) {
        item = queue_pop(queue);
        assert(i == item);
    }
    assert(!queue_empty(queue));
    assert(!queue_full(queue));
    for (i = 0; i < 10; i++) {
        queue_push(queue, i);
    }
    assert(queue_full(queue));
    for (i = 0; i < 10; i++) {
        item = queue_pop(queue);
    }
    assert(item == 19);
}

int main() {
    int i;
    pthread_t child[NUM_CHILDREN];

    Shared *shared = make_shared();

    child[0] = make_thread(producer_entry, shared);
    child[1] = make_thread(consumer_entry, shared);

    for (i = 0; i < NUM_CHILDREN; i++) {
        join_thread(child[i]);
    }

    return 0;
}
