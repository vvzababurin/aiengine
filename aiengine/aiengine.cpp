// aiengine.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"

#include <atomic>

#include <stdio.h>
#include <stdlib.h>
#include <string>

#include <io.h>
#include <windows.h>
#include <winnls.h>
#include <conio.h>
#include <fcntl.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>

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

uint32_t _getAvailableRead(
  struct FreeQueue *queue, 
  uint32_t read_index, 
  uint32_t write_index
) {  
  if (write_index >= read_index)
    return write_index - read_index;
  
  return write_index + queue->buffer_length - read_index;
}

uint32_t _getAvailableWrite(
  struct FreeQueue *queue, 
  uint32_t read_index, 
  uint32_t write_index
) {
  if (write_index >= read_index)
    return queue->buffer_length - write_index + read_index - 1;
  return read_index - write_index - 1;
}

struct FreeQueue *CreateFreeQueue(uint32_t length, uint32_t channel_count) {
  struct FreeQueue *queue = (struct FreeQueue *)malloc(sizeof(struct FreeQueue));
  queue->buffer_length = length + 1;
  queue->channel_count = channel_count;
  queue->state = (std::atomic<unsigned int> *)malloc(2 * sizeof(std::atomic<unsigned int>));
  atomic_store(queue->state + READ, 0);
  atomic_store(queue->state + WRITE, 0);
  queue->channel_data = (float **)malloc(channel_count * sizeof(float *));
  for (uint32_t i = 0; i < channel_count; i++) {
    queue->channel_data[i] = (float *)malloc(queue->buffer_length * sizeof(float));
    for (uint32_t j = 0; j < queue->buffer_length; j++) {
      queue->channel_data[i][j] = 0;
    }
  }
  return queue;
}

void DestroyFreeQueue(struct FreeQueue *queue) {
  if ( queue != nullptr ) {
    for (uint32_t i = 0; i < queue->channel_count; i++) {
      free(queue->channel_data[i]);
    }
    free(queue->channel_data);
	free(queue->state);
    free(queue);
  }
}

bool FreeQueuePush(struct FreeQueue *queue, float **input, size_t block_length) {
  if ( queue != nullptr ) {
    uint32_t current_read = atomic_load(queue->state + READ);
    uint32_t current_write = atomic_load(queue->state + WRITE);
    if (_getAvailableWrite(queue, current_read, current_write) < block_length) {
      return false;
    }
    for (uint32_t i = 0; i < block_length; i++) {
      for (uint32_t channel = 0; channel < queue->channel_count; channel++) {
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

bool FreeQueuePull(struct FreeQueue *queue, float **output, size_t block_length) {
  if ( queue != nullptr ) {
    uint32_t current_read = atomic_load(queue->state + READ);
    uint32_t current_write = atomic_load(queue->state + WRITE);
    if (_getAvailableRead(queue, current_read, current_write) < block_length) {
      return false;
    }
    for (uint32_t i = 0; i < block_length; i++) {
      for (uint32_t channel = 0; channel < queue->channel_count; channel++) {
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
  if ( queue != nullptr ) {
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

void PrintQueueInfo(struct FreeQueue *queue) {
  if ( queue != nullptr ) {
    uint32_t current_read = atomic_load(queue->state + READ);
    uint32_t current_write = atomic_load(queue->state + WRITE);
    for (uint32_t channel = 0; channel < queue->channel_count; channel++) {
      printf("channel %d: ", channel);
      uint32_t len = queue->buffer_length;
      if (len > 1000) len = 1000;
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

void PrintQueueAddresses(struct FreeQueue *queue) {
  if ( queue != nullptr ) {
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

SDL_Mutex *mutex;
struct FreeQueue* queue;

const size_t channel_count = 1;
const size_t data_freq = 44100;

void PlaybackCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	if ( SDL_TryLockMutex( mutex ) ) {
		/* Do stuff while mutex is locked */

        float* data[channel_count];

        data[0] = (float*)malloc(additional_amount);

        bool pull_result = FreeQueuePull(queue, data, additional_amount / sizeof(float));
        if (pull_result) {
            SDL_PutAudioStreamData(stream, (void*)data[0], additional_amount);
        }

        free( data[0] );

		SDL_UnlockMutex( mutex );
	} else {
		//SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Mutex is locked on another thread\n");
	}

}

void CaptureCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	if ( SDL_TryLockMutex( mutex ) ) {

		//SDL_Log("data: %d %d\n", additional_amount, total_amount);

        float* data[channel_count];
        data[0] = (float*)malloc(additional_amount);

		int read_bytes = SDL_GetAudioStreamData(stream, (void*)data[0], additional_amount);
        if (read_bytes > 0) {
            bool push_result = FreeQueuePush(queue, data, read_bytes / sizeof(float));
            if (!push_result) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "FreeQueuePush failed\n");
            }
        }

		free( data[0] );

		SDL_UnlockMutex( mutex );
	} else {
		//SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Mutex is locked on another thread\n");
	}
}

int main(int argc, char* argv[])
{
	int count = 0;

#ifdef _WIN32
	_setmode(_fileno(stdout), _O_TEXT);
	_setmode(_fileno(stdin),  _O_TEXT);
	_setmode(_fileno(stderr), _O_TEXT);

	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);
#endif

    queue = CreateFreeQueue(data_freq * 10, channel_count);

	SDL_Init(SDL_INIT_AUDIO);

	mutex = SDL_CreateMutex();
	if (!mutex) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create mutex: ( %s )\n", SDL_GetError() );
		return -1;
	}

	SDL_AudioDeviceID * audioDevices = SDL_GetAudioRecordingDevices( &count );
	if ( audioDevices == NULL ) {
		// TODO: fatal crash
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_AudioDeviceID is NULL: ( %s )\n", SDL_GetError() );
		return -1;
	} else {
		for ( int i = 0; i < count; i++ )
		{
			const char* deviceName = SDL_GetAudioDeviceName( audioDevices[i] );
			SDL_Log( "DeviceId: %d ( %s )\n", audioDevices[i], SDL_iconv_utf8_locale( deviceName ) );
		}

        SDL_AudioSpec spec;
		SDL_AudioStream * capture = NULL;
		SDL_AudioStream * playback = NULL;

		SDL_memset( &spec, 0, sizeof(spec));

		spec.format = SDL_AUDIO_F32;
		spec.channels = channel_count;
		spec.freq = data_freq;
	
		capture = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, CaptureCallback, NULL );
		if (capture == NULL) 
		{
			SDL_Log( "Failed to open audio: %s\n", SDL_GetError() );
		} else {
			SDL_ResumeAudioStreamDevice( capture ); 
		}

		playback = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, PlaybackCallback, NULL );
		if ( playback == NULL )
		{
			SDL_Log( "Failed to open audio: %s\n", SDL_GetError() );
		} else {
			SDL_ResumeAudioStreamDevice( playback ); 
		}

		SDL_Delay( 10000 ); 
        PrintQueueInfo( queue );

		SDL_DestroyAudioStream( capture );
        SDL_DestroyAudioStream( playback );

		SDL_DestroyMutex( mutex );
        DestroyFreeQueue( queue );
	}

	return 0;
}


