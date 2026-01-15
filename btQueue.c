#include <stdlib.h> // For malloc, free
#include <stdbool.h> // For bool type
#include "btQueue.h"


// Function prototypes
btBuffer *btBuffer_create(unsigned size);
void btBuffer_destroy(btBuffer *buffer);
void btQueue_enqueueBuffer(btQueue *queue, btBuffer *buffer);
btBuffer *btQueue_dequeue(btQueue *queue);
bool btQueue_avail(btQueue *queue);

void btQueue_init(btQueue *queue) {
    queue->m_pFirst = NULL;
    queue->m_pLast = NULL;
}

// Create a btBuffer instance
btBuffer *btBuffer_create(unsigned size) {
    btBuffer *buffer = (btBuffer *)malloc(sizeof(btBuffer));
    if (buffer == NULL) {
        return NULL;
    }

    buffer->buffer = (unsigned char *)malloc(size * sizeof(unsigned char));
    if (buffer->buffer == NULL) {
        free(buffer);
        return NULL;
    }

    buffer->length = size;
    buffer->data = NULL;
    buffer->pPrev = NULL;
    buffer->pNext = NULL;

    return buffer;
}
 
// Destroy a btBuffer instance
void btBuffer_destroy(btBuffer *buffer) {
    if (buffer != NULL) {
        free(buffer->buffer);
        free(buffer);
    }
}

// Enqueue a btBuffer instance to the queue
void btQueue_enqueueBuffer(btQueue *queue, btBuffer *buffer) {
    if (queue == NULL || buffer == NULL) {
        return;
    }

    if (queue->m_pFirst == NULL) {
        queue->m_pFirst = buffer;
        queue->m_pLast = buffer;
    } else {
        queue->m_pLast->pNext = buffer;
        buffer->pPrev = queue->m_pLast;
        queue->m_pLast = buffer;
    }
}

// Dequeue a btBuffer instance from the queue
// Buffer is destroied by the caller
btBuffer *btQueue_dequeue(btQueue *queue) {
    if (queue == NULL || queue->m_pFirst == NULL) {
        return NULL;
    }

    btBuffer *buffer = (btBuffer *)queue->m_pFirst;
    queue->m_pFirst = buffer->pNext;

    if (queue->m_pFirst != NULL) {
        queue->m_pFirst->pPrev = NULL;
    } else {
        queue->m_pLast = NULL;
    }

    return buffer;
}


// Check if the queue is empty
bool btQueue_avail(btQueue *queue) {
    return (queue != NULL && queue->m_pFirst != NULL);
}
