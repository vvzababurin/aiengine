#include <atomic>

#include <stdio.h>
#include <stdlib.h>
#include <string>

#ifdef _WIN32

#include <io.h>
#include <windows.h>
#include <winnls.h>
#include <conio.h>
#include <fcntl.h>

#endif

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

struct FreeQueue *CreateFreeQueue(uint32_t length, uint32_t channel_count);
void DestroyFreeQueue(struct FreeQueue *queue);
bool FreeQueuePush(struct FreeQueue *queue, float **input, size_t block_length);
bool FreeQueuePull(struct FreeQueue *queue, float **output, size_t block_length);
void PrintQueueInfo(struct FreeQueue *queue);

