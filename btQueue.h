#ifndef btBuffer_H
#define btBuffer_H

#include <stdlib.h> // For malloc, free
#include <stdbool.h> // For bool type

// Define btBuffer structure
typedef struct btBuffer {
    unsigned char *buffer;
    unsigned length;
    unsigned char *data;
    struct btBuffer *pPrev;
    struct btBuffer *pNext;
} btBuffer;

// Define btQueue structure
typedef struct {
    btBuffer *m_pFirst;
    btBuffer *m_pLast;
} btQueue;

// Function prototypes
btBuffer *btBuffer_create(unsigned size);
void btBuffer_destroy(btBuffer *buffer);
void btQueue_enqueueBuffer(btQueue *queue, btBuffer *buffer);
btBuffer *btQueue_dequeue(btQueue *queue);
bool btQueue_avail(btQueue *queue);
void btQueue_init(btQueue *queue);
#endif