
#pragma once

#ifndef __AIENGINE_FREEQUEUE__
#define __AIENGINE_FREEQUEUE__

#include <atomic>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct FreeQueue {
  uint32_t buffer_length;
  uint32_t channel_count;
  float **channel_data;
  std::atomic<unsigned int> *state;
};

enum FreeQueueState {
  /** @type {number} A shared index for reading from the queue. (consumer) */
  READ = 0,
  /** @type {number} A shared index for writing into the queue. (producer) */
  WRITE = 1
};

struct FreeQueue *FQ_CreateFreeQueue(uint32_t length, uint32_t channel_count);
void FQ_DestroyFreeQueue(struct FreeQueue *queue);
bool FQ_FreeQueuePush(struct FreeQueue *queue, float **input, size_t block_length);
bool FQ_FreeQueuePushBack(struct FreeQueue* queue, float** input, size_t block_length);
bool FQ_FreeQueuePull(struct FreeQueue *queue, float **output, size_t block_length);
bool FQ_FreeQueuePullBack(struct FreeQueue *queue, float **output, size_t block_length);
void FQ_PrintQueueInfo(struct FreeQueue *queue);
void FQ_PrintQueueAddresses(struct FreeQueue *queue);

#endif // __AIENGINE_FREEQUEUE__
