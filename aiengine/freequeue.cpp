// aiengine.cpp: ���������� ����� ����� ��� ����������� ����������.
//

#include "stdafx.h"

#include "freequeue.h"

uint32_t _getAvailableRead(
  struct FreeQueue *queue, 
  uint32_t read_index, 
  uint32_t write_index
) 
{  
  if (write_index >= read_index)
    return write_index - read_index;
  
  return write_index + queue->buffer_length - read_index;
}

uint32_t _getAvailableWrite(
  struct FreeQueue *queue, 
  uint32_t read_index, 
  uint32_t write_index
) 
{
  if (write_index >= read_index)
    return queue->buffer_length - write_index + read_index - 1;
  return read_index - write_index - 1;
}

struct FreeQueue *FQ_CreateFreeQueue(uint32_t length, uint32_t channel_count) 
{
  struct FreeQueue *queue = (struct FreeQueue *)malloc(sizeof(struct FreeQueue));
  queue->buffer_length = length + 1;
  queue->channel_count = channel_count;
  queue->state = (std::atomic<unsigned int> *)malloc(2 * sizeof(std::atomic<unsigned int>));
  atomic_store(queue->state + READ, 0);
  atomic_store(queue->state + WRITE, 0);
  queue->channel_data = (float **)malloc(channel_count * sizeof(float *));
  for (uint32_t i = 0; i < channel_count; i++) 
  {
    queue->channel_data[i] = (float *)malloc(queue->buffer_length * sizeof(float));
    for (uint32_t j = 0; j < queue->buffer_length; j++) 
    {
      queue->channel_data[i][j] = 0;
    }
  }
  return queue;
}

void FQ_DestroyFreeQueue(struct FreeQueue *queue) 
{
  if ( queue != nullptr ) 
  {
    for (uint32_t i = 0; i < queue->channel_count; i++) 
    {
      free(queue->channel_data[i]);
    }
    free(queue->channel_data);
	free(queue->state);
    free(queue);
  }
}

bool FQ_FreeQueuePush(struct FreeQueue *queue, float **input, size_t block_length) 
{
  if ( queue != nullptr ) 
  {
    uint32_t current_read = atomic_load(queue->state + READ);
    uint32_t current_write = atomic_load(queue->state + WRITE);
    if (_getAvailableWrite(queue, current_read, current_write) < block_length) 
    {
      return false;
    }
    for (uint32_t i = 0; i < block_length; i++) 
    {
      for (uint32_t channel = 0; channel < queue->channel_count; channel++) 
      {
        queue->channel_data[channel][(current_write + i) % queue->buffer_length] = 
            input[channel][i];
      }
    }
    uint32_t next_write = (current_write + block_length) % queue->buffer_length;
    atomic_store(queue->state + WRITE, next_write);
    return true;
  }
  return false;
}

bool FQ_FreeQueuePull(struct FreeQueue *queue, float **output, size_t block_length) 
{
  if ( queue != nullptr ) 
  {
    uint32_t current_read = atomic_load(queue->state + READ);
    uint32_t current_write = atomic_load(queue->state + WRITE);
    if (_getAvailableRead(queue, current_read, current_write) < block_length) 
    {
      return false;
    }
    for (uint32_t i = 0; i < block_length; i++) 
    {
      for (uint32_t channel = 0; channel < queue->channel_count; channel++) 
      {
        output[channel][i] = 
            queue->channel_data[channel][(current_read + i) % queue->buffer_length];
      }
    }
    uint32_t nextRead = (current_read + block_length) % queue->buffer_length;
    atomic_store(queue->state + READ, nextRead);
    return true;
  }
  return false;
}

void *GetFreeQueuePointers( struct FreeQueue* queue, char* data ) 
{
  if ( queue != nullptr ) 
  {
    if (strcmp(data, "buffer_length") == 0) {
      return ( void* )&queue->buffer_length;
    }
    else if (strcmp(data, "channel_count") == 0) {
      return ( void* )&queue->channel_count;
    }
    else if (strcmp(data, "state") == 0) {
      return ( void* )&queue->state;
    }
    else if (strcmp(data, "channel_data") == 0) {
      return ( void* )&queue->channel_data;
    }
  }
  return 0;
}

void FQ_PrintQueueInfo(struct FreeQueue *queue) 
{
  if ( queue != nullptr ) 
  {
    uint32_t current_read = atomic_load(queue->state + READ);
    uint32_t current_write = atomic_load(queue->state + WRITE);
    for (uint32_t channel = 0; channel < queue->channel_count; channel++) 
    {
      printf("channel %d: ", channel);
      uint32_t len = queue->buffer_length;
      if (len > 100) len = 100;
      for (uint32_t i = 0; i < len; i++) {
          printf("%f ", queue->channel_data[channel][i]);
      }
      printf("\n");
    }
    printf("----------\n");
    printf("current_read: %u  | current_write: %u\n", current_read, current_write);
    printf("available_read: %u  | available_write: %u\n",
        _getAvailableRead(queue, current_read, current_write), 
        _getAvailableWrite(queue, current_read, current_write));
    printf("----------\n");
  }
}

void FQ_PrintQueueAddresses(struct FreeQueue *queue) 
{
  if ( queue != nullptr ) 
  {
      printf("buffer_length: %p   uint: %zu\n",
        &queue->buffer_length, (size_t)&queue->buffer_length);
      printf("channel_count: %p   uint: %zu\n",
        &queue->channel_count, (size_t)&queue->channel_count);
      printf("state       : %p   uint: %zu\n",
        &queue->state, (size_t)&queue->state);
      printf("channel_data    : %p   uint: %zu\n",
        &queue->channel_data, (size_t)&queue->channel_data);
    for (uint32_t channel = 0; channel < queue->channel_count; channel++) {
        printf("channel_data[%d]    : %p   uint: %zu\n", channel,
            &queue->channel_data[channel], (size_t)&queue->channel_data[channel]);
    }
    printf("state[0]    : %p   uint: %zu\n",
        &queue->state[0], (size_t)&queue->state[0]);
    printf("state[1]    : %p   uint: %zu\n",
        &queue->state[1], (size_t)&queue->state[1]);
  }
}

