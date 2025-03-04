// aiengine.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"

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

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>

#include "freequeue.h"

SDL_Mutex *mutex;
struct FreeQueue* queue;

const size_t channels_count = 1;
const size_t data_freq = 44100;

int window_width = 800;
int window_height = 600;

void PlaybackCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	SDL_LockMutex( mutex );

	float* data[channels_count];
	data[0] = (float*)malloc(additional_amount);
	bool pull_result = FQ_FreeQueuePull(queue, data, additional_amount / sizeof(float));
	if ( pull_result ) {
		SDL_PutAudioStreamData(stream, (void*)data[0], additional_amount);
	} 
	free( data[0] );

	SDL_UnlockMutex( mutex );
}

void CaptureCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	SDL_LockMutex( mutex );

	float* data[channels_count];
	data[0] = (float*)malloc(additional_amount);
	int read_bytes = SDL_GetAudioStreamData(stream, (void*)data[0], additional_amount);
	if (read_bytes > 0) {
		FQ_FreeQueuePush(queue, data, read_bytes / sizeof(float));
	}
	free( data[0] );

	SDL_UnlockMutex( mutex );
}

void RenderCallback(SDL_Renderer* renderer)
{
	SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
	SDL_RenderClear(renderer);

	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);

	SDL_RenderLine(renderer, (float)0, (float)window_height / 2, (float)window_width, (float)window_height / 2);
	SDL_RenderLine(renderer, (float)window_width / 2, (float)0, (float)window_width / 2, (float)window_height);

	//SDL_RenderTexture(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[])
{
	int count = 0;
	bool done = false;

#ifdef _WIN32
	_setmode(_fileno(stdout), _O_TEXT);
	_setmode(_fileno(stdin),  _O_TEXT);
	_setmode(_fileno(stderr), _O_TEXT);

	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);
#endif

	queue = FQ_CreateFreeQueue(data_freq * 10, channels_count);

	SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

	mutex = SDL_CreateMutex();
	if ( !mutex ) 
	{
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Couldn't create mutex: ( %s )\n", SDL_GetError() );
		return -1;
	}

	SDL_AudioDeviceID * audioDevices = SDL_GetAudioRecordingDevices( &count );
	if ( audioDevices == NULL ) 
	{
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "SDL_AudioDeviceID is NULL: ( %s )\n", SDL_GetError() );
		return -1;
	} 
	else 
	{
		for ( int i = 0; i < count; i++ )
		{
			const char* deviceName = SDL_GetAudioDeviceName( audioDevices[i] );
			SDL_Log( "DeviceId: %d ( %s )\n", audioDevices[i], SDL_iconv_utf8_locale( deviceName ) );
		}

		SDL_AudioSpec spec;
		SDL_AudioStream * capture = NULL;
		SDL_AudioStream * playback = NULL;

		SDL_memset( &spec, 0, sizeof(spec) );

		spec.format = SDL_AUDIO_F32;
		spec.channels = channels_count;
		spec.freq = data_freq;
	
		capture = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, CaptureCallback, NULL );
		if (capture == NULL) 
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio: %s\n", SDL_GetError() );
		} 
		else
		{
			SDL_ResumeAudioStreamDevice( capture ); 
		}

		playback = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, PlaybackCallback, NULL );
		if ( playback == NULL )
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio: %s\n", SDL_GetError() );
		}
		else 
		{
			SDL_ResumeAudioStreamDevice( playback ); 
		}

		SDL_Window* wnd = NULL;
		SDL_Renderer* renderer = NULL;

		bool result = SDL_CreateWindowAndRenderer( "An SDL3 window", window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &wnd, &renderer );

		//SDL_SyncWindow(wnd);

		while ( !done ) 
		{
			SDL_Event event;

			while ( SDL_PollEvent(&event) ) 
			{
				if (event.type == SDL_EVENT_QUIT) 
				{
					done = true;
				} 
				else if (event.type == SDL_EVENT_WINDOW_RESIZED) 
				{
					SDL_GetWindowSizeInPixels( wnd, &window_width, &window_height );
				}
			}

			RenderCallback(renderer);
		}

		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(wnd);

		FQ_PrintQueueInfo( queue );

		SDL_DestroyAudioStream( capture );
		SDL_DestroyAudioStream( playback );

		SDL_DestroyMutex( mutex );
		FQ_DestroyFreeQueue( queue );

		SDL_Quit();
	}

	return 0;
}

