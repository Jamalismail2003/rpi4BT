#include <gtest/gtest.h>
#include <stdlib.h>
#include <stdbool.h>

extern "C" {
#include "../btQueue.h"
}

// Test btBuffer_create function
TEST(BtQueueTest, BtBufferCreateAllocatesMemory) {
    unsigned size = 256;
    btBuffer *buffer = btBuffer_create(size);
    
    // Verify buffer was allocated
    ASSERT_NE(buffer, nullptr);
    
    // Verify internal buffer was allocated
    ASSERT_NE(buffer->buffer, nullptr);
    
    // Verify size is correctly set
    EXPECT_EQ(buffer->length, size);
    
    // Verify pointers are initialized to NULL
    EXPECT_EQ(buffer->data, nullptr);
    EXPECT_EQ(buffer->pPrev, nullptr);
    EXPECT_EQ(buffer->pNext, nullptr);
    
    btBuffer_destroy(buffer);
}

TEST(BtQueueTest, BtBufferCreateWithDifferentSizes) {
    unsigned sizes[] = {1, 64, 256, 1024, 4096};
    
    for (unsigned size : sizes) {
        btBuffer *buffer = btBuffer_create(size);
        
        ASSERT_NE(buffer, nullptr);
        EXPECT_EQ(buffer->length, size);
        EXPECT_NE(buffer->buffer, nullptr);
        
        btBuffer_destroy(buffer);
    }
}

TEST(BtQueueTest, BtBufferCreateFailsWithZeroSize) {
    // Edge case: creating buffer with size 0
    btBuffer *buffer = btBuffer_create(0);
    
    // Should still allocate the btBuffer structure but with zero-sized buffer
    if (buffer != nullptr) {
        EXPECT_EQ(buffer->length, 0);
        btBuffer_destroy(buffer);
    }
}

// Test btBuffer_destroy function
TEST(BtQueueTest, BtBufferDestroyFreesMemory) {
    btBuffer *buffer = btBuffer_create(256);
    ASSERT_NE(buffer, nullptr);
    
    // This should not crash or cause issues
    btBuffer_destroy(buffer);
    
    // Test that destroy handles NULL gracefully
    btBuffer_destroy(nullptr);
}

// Test btQueue_init function
TEST(BtQueueTest, BtQueueInitClearsQueue) {
    btQueue queue;
    
    // Initialize queue
    btQueue_init(&queue);
    
    // Verify queue pointers are NULL
    EXPECT_EQ(queue.m_pFirst, nullptr);
    EXPECT_EQ(queue.m_pLast, nullptr);
}

TEST(BtQueueTest, BtQueueInitMultipleTimes) {
    btQueue queue;
    
    // Initialize multiple times
    btQueue_init(&queue);
    EXPECT_EQ(queue.m_pFirst, nullptr);
    
    btQueue_init(&queue);
    EXPECT_EQ(queue.m_pFirst, nullptr);
}

// Test btQueue_enqueueBuffer function
TEST(BtQueueTest, BtQueueEnqueueSingleBuffer) {
    btQueue queue;
    btBuffer *buffer = btBuffer_create(256);
    
    btQueue_init(&queue);
    btQueue_enqueueBuffer(&queue, buffer);
    
    // Verify buffer is in queue
    EXPECT_EQ(queue.m_pFirst, buffer);
    EXPECT_EQ(queue.m_pLast, buffer);
    EXPECT_EQ(buffer->pPrev, nullptr);
    EXPECT_EQ(buffer->pNext, nullptr);
    
    btBuffer_destroy(buffer);
}

TEST(BtQueueTest, BtQueueEnqueueMultipleBuffers) {
    btQueue queue;
    btBuffer *buf1 = btBuffer_create(256);
    btBuffer *buf2 = btBuffer_create(256);
    btBuffer *buf3 = btBuffer_create(256);
    
    btQueue_init(&queue);
    
    // Enqueue first buffer
    btQueue_enqueueBuffer(&queue, buf1);
    EXPECT_EQ(queue.m_pFirst, buf1);
    EXPECT_EQ(queue.m_pLast, buf1);
    
    // Enqueue second buffer
    btQueue_enqueueBuffer(&queue, buf2);
    EXPECT_EQ(queue.m_pFirst, buf1);
    EXPECT_EQ(queue.m_pLast, buf2);
    EXPECT_EQ(buf1->pNext, buf2);
    EXPECT_EQ(buf2->pPrev, buf1);
    
    // Enqueue third buffer
    btQueue_enqueueBuffer(&queue, buf3);
    EXPECT_EQ(queue.m_pLast, buf3);
    EXPECT_EQ(buf2->pNext, buf3);
    EXPECT_EQ(buf3->pPrev, buf2);
    
    btBuffer_destroy(buf1);
    btBuffer_destroy(buf2);
    btBuffer_destroy(buf3);
}

TEST(BtQueueTest, BtQueueEnqueueNullBuffer) {
    btQueue queue;
    btQueue_init(&queue);
    
    // Enqueue NULL buffer should handle gracefully
    btQueue_enqueueBuffer(&queue, nullptr);
    
    // Queue should still be empty
    EXPECT_EQ(queue.m_pFirst, nullptr);
    EXPECT_EQ(queue.m_pLast, nullptr);
}

// Test btQueue_dequeue function
TEST(BtQueueTest, BtQueueDequeueFromEmptyQueue) {
    btQueue queue;
    btQueue_init(&queue);
    
    btBuffer *result = btQueue_dequeue(&queue);
    
    // Dequeuing from empty queue should return NULL
    EXPECT_EQ(result, nullptr);
}

TEST(BtQueueTest, BtQueueDequeueSingleBuffer) {
    btQueue queue;
    btBuffer *buffer = btBuffer_create(256);
    
    btQueue_init(&queue);
    btQueue_enqueueBuffer(&queue, buffer);
    
    btBuffer *dequeued = btQueue_dequeue(&queue);
    
    // Verify we got the same buffer back
    EXPECT_EQ(dequeued, buffer);
    
    // Verify queue is now empty
    EXPECT_EQ(queue.m_pFirst, nullptr);
    EXPECT_EQ(queue.m_pLast, nullptr);
    
    btBuffer_destroy(buffer);
}

TEST(BtQueueTest, BtQueueDequeueMultipleBuffers) {
    btQueue queue;
    btBuffer *buf1 = btBuffer_create(256);
    btBuffer *buf2 = btBuffer_create(256);
    btBuffer *buf3 = btBuffer_create(256);
    
    btQueue_init(&queue);
    btQueue_enqueueBuffer(&queue, buf1);
    btQueue_enqueueBuffer(&queue, buf2);
    btQueue_enqueueBuffer(&queue, buf3);
    
    // Dequeue first buffer
    btBuffer *dequeued1 = btQueue_dequeue(&queue);
    EXPECT_EQ(dequeued1, buf1);
    EXPECT_EQ(queue.m_pFirst, buf2);
    EXPECT_EQ(buf2->pPrev, nullptr);
    
    // Dequeue second buffer
    btBuffer *dequeued2 = btQueue_dequeue(&queue);
    EXPECT_EQ(dequeued2, buf2);
    EXPECT_EQ(queue.m_pFirst, buf3);
    
    // Dequeue third buffer
    btBuffer *dequeued3 = btQueue_dequeue(&queue);
    EXPECT_EQ(dequeued3, buf3);
    EXPECT_EQ(queue.m_pFirst, nullptr);
    EXPECT_EQ(queue.m_pLast, nullptr);
    
    btBuffer_destroy(buf1);
    btBuffer_destroy(buf2);
    btBuffer_destroy(buf3);
}

TEST(BtQueueTest, BtQueueDequeueNullQueue) {
    btBuffer *result = btQueue_dequeue(nullptr);
    
    // Dequeuing from NULL queue should return NULL
    EXPECT_EQ(result, nullptr);
}

// Integration test: Queue operations
TEST(BtQueueTest, QueueFifoOrder) {
    btQueue queue;
    btBuffer *buf1 = btBuffer_create(100);
    btBuffer *buf2 = btBuffer_create(200);
    btBuffer *buf3 = btBuffer_create(300);
    
    btQueue_init(&queue);
    btQueue_enqueueBuffer(&queue, buf1);
    btQueue_enqueueBuffer(&queue, buf2);
    btQueue_enqueueBuffer(&queue, buf3);
    
    // Verify FIFO order
    EXPECT_EQ(btQueue_dequeue(&queue), buf1);
    EXPECT_EQ(btQueue_dequeue(&queue), buf2);
    EXPECT_EQ(btQueue_dequeue(&queue), buf3);
    EXPECT_EQ(btQueue_dequeue(&queue), nullptr);
    
    btBuffer_destroy(buf1);
    btBuffer_destroy(buf2);
    btBuffer_destroy(buf3);
}
