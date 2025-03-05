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
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_thread.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "freequeue.h"

struct FreeQueue* queue;

TTF_TextEngine* text_engine;
TTF_Font* small_font;
TTF_Font* big_font;

const size_t channels_count = 1;
const size_t data_freq = 44100;

SDL_Mutex* mutex;
// SDL_Texture* texture;

int window_width = 800;
int window_height = 600;

int WMC_ThreadCallback(void* data)
{
	SDL_LockMutex(mutex);



	SDL_UnlockMutex(mutex);
	return 0;
}

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

SDL_FRect WMC_DrawText(SDL_Renderer* renderer, TTF_Font* font, const char* text, float x, float y, SDL_Color fg, SDL_Color bg, bool only_size = false)
{
	SDL_Surface* ttf_surface = TTF_RenderText_LCD(font, text, 0, fg, bg);
	if (ttf_surface) {
		SDL_FRect rect = { x, y, 0, 0 };
		SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, ttf_surface);
		if (text_texture) {
			float w;
			float h;
			SDL_GetTextureSize(text_texture, &w, &h);
			rect.w = w;
			rect.h = h;
			if (only_size == false) 
			{
				SDL_RenderTexture(renderer, text_texture, NULL, &rect);
				SDL_DestroyTexture(text_texture);
			}
			else 
			{
				SDL_DestroyTexture(text_texture);
			}
		}
		SDL_DestroySurface(ttf_surface);
		return rect;
	}
	else 
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_RenderText_LCD failed: ( %s )\n", SDL_GetError());
	}
	return SDL_FRect();
}

void WMC_RenderCallback(SDL_Renderer* renderer)
{
	SDL_Texture* t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, window_width, window_height);
	SDL_SetRenderTarget(renderer, t);

	SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
	SDL_RenderClear(renderer);

	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);

	SDL_RenderLine(renderer, .0f, (float)window_height / 2.0f, (float)window_width, (float)window_height / 2.0f);
	SDL_RenderLine(renderer, (float)window_width / 2.0f, .0f, (float)window_width / 2.0f, (float)window_height);

	SDL_Color fg = {0, 0, 0, 255 };
	SDL_Color bg = { 255, 255, 255, 255 };

	WMC_DrawText(renderer, small_font, "const char* text", 20.0f, 20.0f, fg, bg);
	WMC_DrawText(renderer, big_font, "test", 40.0f, 40.0f, fg, bg);

	SDL_SetRenderTarget(renderer, NULL);
	SDL_RenderTexture(renderer, t, NULL, NULL);

	SDL_RenderPresent(renderer);
	SDL_DestroyTexture(t);
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
	if ( !TTF_Init() ) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_Init failed: ( %s )\n", SDL_GetError());
	}

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
		SDL_Thread* thread = NULL;
			
		// thread = SDL_CreateThread( ThreadCallback, "", NULL );

		bool result = SDL_CreateWindowAndRenderer("An SDL3 window", window_width, window_height, SDL_WINDOW_OPENGL, &wnd, &renderer);
		if (result == false) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError());
		}

		text_engine = TTF_CreateRendererTextEngine(renderer);

		small_font = TTF_OpenFont("./fonts/segoeui.ttf", 8);
		big_font = TTF_OpenFont("./fonts/segoeui.ttf", 12);

		while ( !done ) 
		{
			SDL_Event event;

			while ( SDL_PollEvent(&event) ) 
			{
				if (event.type == SDL_EVENT_QUIT) 
				{
					done = true;
					break;
				} 
				else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
				{
					// SDL_GetWindowSize(wnd, &window_width, &window_height);
					// SDL_SetWindowSize(wnd, window_width, window_height);

					// SDL_Surface* surface = SDL_GetWindowSurface(wnd);
					// SDL_BlitSurface(image, NULL, surface, NULL);
	
					// SDL_Palette* palette = SDL_GetSurfacePalette(surface);
					// const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_ARGB8888);

					// uint32_t black = SDL_MapRGBA(format, palette, 0, 0, 0, 255);

					// SDL_FillSurfaceRect(surface, NULL, black);
					// SDL_UpdateWindowSurface(wnd);
					break;
				}
			}

			WMC_RenderCallback( renderer );
		}

		SDL_DestroyRenderer( renderer );
		SDL_DestroyWindow( wnd );

		TTF_CloseFont( big_font );
		TTF_CloseFont( small_font );
		TTF_DestroyRendererTextEngine( text_engine );

		FQ_PrintQueueInfo( queue );

		SDL_DestroyAudioStream( capture );
		SDL_DestroyAudioStream( playback );

		SDL_DestroyMutex( mutex );
		FQ_DestroyFreeQueue( queue );

		SDL_Quit();
	}

	return 0;
}

