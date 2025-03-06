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
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "freequeue.h"

struct FreeQueue* queue;

TTF_TextEngine* text_engine;
TTF_Font* font_small;
TTF_Font* font_big;

const size_t channels_count = 1;
const size_t data_freq = 44100;

SDL_Mutex* mutex;
SDL_Texture* buttons_texture;

float width_buttons_texture = 0.0f;
float height_buttons_texture = 0.0f;

// SDL_Texture* texture;

int window_width = 800;
int window_height = 600;

float capture_mouse_xxx = 0.0f;
float capture_mouse_yyy = 0.0f;

int button_1 = -1;
int button_2 = -1;
int button_3 = -1;
int button_4 = -1;

int WMC_ThreadCallback(void* data)
{
	SDL_LockMutex(mutex);



	SDL_UnlockMutex(mutex);
	return 0;
}

void WMC_PlaybackCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
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

void WMC_CaptureCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
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
			if (only_size == false) {
				SDL_RenderTexture(renderer, text_texture, NULL, &rect);
				SDL_DestroyTexture(text_texture);
			} else {
				SDL_DestroyTexture(text_texture);
			}
		}
		SDL_DestroySurface(ttf_surface);
		return rect;
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_RenderText_LCD failed: ( %s )\n", SDL_GetError());
	}
	return SDL_FRect();
}

bool WMC_IsInRect( SDL_FRect* r, float x, float y )
{
	if ( ( x >= r->x) && ( x <= ( r->x + r->w ) ) && ( y >= r->y ) && ( y <= ( r->y + r->h ) ) ) {
		return true;
	} else {
		return false;
	}
}

void WMC_MouseCallback(SDL_Renderer* renderer)
{	
	SDL_FRect rect = { 0, 0, 0, 0 };

	rect.x = window_width - (width_buttons_texture + 8.0f);
	rect.y = 8.0f;
	rect.w = 52;
	rect.h = 52;

	if ( WMC_IsInRect(&rect, capture_mouse_xxx, capture_mouse_yyy) ) {
		button_1 = 1;
	} else {
		button_1 = -1;
	}
	rect.x = rect.x + rect.w + 1;
	if ( WMC_IsInRect(&rect, capture_mouse_xxx, capture_mouse_yyy) ) {
		button_2 = 1;
	}
	else {
		button_2 = -1;
	}
	rect.x = rect.x + rect.w + 1;
	if ( WMC_IsInRect(&rect, capture_mouse_xxx, capture_mouse_yyy) ) {
		button_3 = 1;
	}
	else {
		button_3 = -1;
	}
	rect.x = rect.x + rect.w + 1;
	if ( WMC_IsInRect(&rect, capture_mouse_xxx, capture_mouse_yyy) ) {
		button_4 = 1;
	}
	else {
		button_4 = -1;
	}

	//////////////////////////////////////////////////////////////////////////////////
	// SDL_Log("Mouse: %f;%f", capture_mouse_xxx, capture_mouse_yyy);
}

void WMC_RenderCallback(SDL_Renderer* renderer)
{
	SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);

	SDL_RenderLine(renderer, .0f, (float)window_height / 2.0f, (float)window_width, (float)window_height / 2.0f);
	SDL_RenderLine(renderer, (float)window_width / 2.0f, .0f, (float)window_width / 2.0f, (float)window_height);

	SDL_Color fg = { 0, 0, 0, 255 };
	SDL_Color bg = { 255, 255, 255, 255 };

	WMC_DrawText(renderer, font_small, "const char* text", 20.0f, 20.0f, fg, bg);
	WMC_DrawText(renderer, font_big, "test", 40.0f, 40.0f, fg, bg);

	SDL_FRect rect = { 0, 0, 0, 0 };

	SDL_GetTextureSize(buttons_texture, &width_buttons_texture, &height_buttons_texture);

	rect.x = window_width - (width_buttons_texture + 8);
	rect.y = 8;
	rect.w = width_buttons_texture;
	rect.h = height_buttons_texture;

	SDL_RenderTexture(renderer, buttons_texture, NULL, &rect);

	rect.w = 52;
	rect.h = 52;

	if ( button_1 > 0 ) {
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
		SDL_RenderRect(renderer, &rect);
	}
	rect.x = rect.x + rect.w + 1;
	if (button_2 > 0) {
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
		SDL_RenderRect(renderer, &rect);
	}
	rect.x = rect.x + rect.w + 1;
	if (button_3 > 0) {
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
		SDL_RenderRect(renderer, &rect);
	}
	rect.x = rect.x + rect.w + 1;
	if (button_4 > 0) {
		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
		SDL_RenderRect(renderer, &rect);
	}

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
	if ( !TTF_Init() ) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_Init failed: ( %s )\n", SDL_GetError());
	}

	mutex = SDL_CreateMutex();
	if ( !mutex ) {
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "Couldn't create mutex: ( %s )\n", SDL_GetError() );
		return -1;
	}

	SDL_AudioDeviceID * audioDevices = SDL_GetAudioRecordingDevices( &count );
	if ( audioDevices == NULL ) {
		SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "SDL_AudioDeviceID is NULL: ( %s )\n", SDL_GetError() );
		return -1;
	} else {
		for ( int i = 0; i < count; i++ ) {
			const char* deviceName = SDL_GetAudioDeviceName( audioDevices[i] );
			SDL_Log( "audioRecordingDeviceId: %d ( %s )\n", audioDevices[i], SDL_iconv_utf8_locale( deviceName ) );
		}
		
		SDL_AudioSpec spec;
		SDL_AudioStream * capture = NULL;
		SDL_AudioStream * playback = NULL;
		
		SDL_memset( &spec, 0, sizeof(spec) );
		
		spec.format = SDL_AUDIO_F32;
		spec.channels = channels_count;
		spec.freq = data_freq;

		capture = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, WMC_CaptureCallback, NULL );
		if (capture == NULL) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio: %s\n", SDL_GetError() );
		} else {
			SDL_ResumeAudioStreamDevice( capture ); 
		}

		playback = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, WMC_PlaybackCallback, NULL );
		if ( playback == NULL ) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio: %s\n", SDL_GetError() );
		} else {
			SDL_ResumeAudioStreamDevice( playback ); 
		}

		SDL_Window* wnd = NULL;
		SDL_Renderer* renderer = NULL;
		SDL_Thread* thread = NULL;
			
		// thread = SDL_CreateThread( WNC_ThreadCallback, "", NULL );

		int vd_software = -1;
		int vd_opengl = -1;
		int vd_opengles2 = -1;
		int vd_direct3d = -1;

		int numberof_drivers = SDL_GetNumRenderDrivers();
		for (int i = 0; i < numberof_drivers; i++) {
			const char* deviceName = SDL_GetRenderDriver(i);

			if (strcmp(deviceName, "software") == 0) vd_software = 1;
			if (strcmp(deviceName, "opengl") == 0) vd_opengl = 1;
			if (strcmp(deviceName, "direct3d") == 0) vd_direct3d = 1;
			if (strcmp(deviceName, "opengles3") == 0) vd_opengles2 = 1;
			
			SDL_Log("videoDeviceId: %d ( %s )\n", i, SDL_iconv_utf8_locale(deviceName));
		}

		wnd = SDL_CreateWindow( "An SDL3 window", window_width, window_height, SDL_WINDOW_MOUSE_CAPTURE);
		if (!wnd) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s\n", SDL_GetError());
			return -1;
		}

		if ( vd_opengles2 == 1 )
			renderer = SDL_CreateRenderer(wnd, "opengles3");
		else if ( vd_opengl == 1 )
			renderer = SDL_CreateRenderer(wnd, "opengl");
		else if ( vd_direct3d == 1 )
			renderer = SDL_CreateRenderer(wnd, "direct3d");
		else if ( vd_software == 1 )
			renderer = SDL_CreateRenderer(wnd, "software");

		if (!renderer) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
			return -1;
		}

		SDL_Surface* buttons = IMG_Load("./external_images/buttons.png");
		if (!buttons) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IMG_Load failed: %s\n", SDL_GetError());
		}

		buttons_texture = SDL_CreateTextureFromSurface(renderer, buttons);
		if (!buttons_texture) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
		}

		SDL_DestroySurface(buttons);

		int top = 0;
		int left = 0;
		int bottom = 0;
		int right = 0;

		SDL_SetWindowResizable(wnd, true);
		SDL_SetWindowBordered(wnd, true);

		bool border_result = SDL_GetWindowBordersSize(wnd, &top, &left, &bottom, &right);
		if ( !border_result ) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetWindowBordersSize failed: %s\n", SDL_GetError());
		}

		bool presentation_result = SDL_SetRenderLogicalPresentation( renderer, window_width, window_height, SDL_LOGICAL_PRESENTATION_DISABLED );
		if ( !presentation_result ){
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_SetRenderLogicalPresentation failed: %s\n", SDL_GetError());
		}

		text_engine = TTF_CreateRendererTextEngine( renderer );

		font_small = TTF_OpenFont("./external_fonts/segoeui.ttf", 8);
		font_big = TTF_OpenFont("./external_fonts/segoeui.ttf", 12);

		while ( !done ) 
		{
			SDL_Event event;

			SDL_PollEvent( &event );
			
			switch ( event.type )
			{
				case SDL_EVENT_QUIT:
				{
					done = true;
					break;
				}
				case SDL_EVENT_WINDOW_RESIZED:
				{
					// SDL_GetWindowSize(wnd, &window_width, &window_height);
					window_width = event.window.data1;
					window_height = event.window.data2;
					break;
				}
				case SDL_EVENT_MOUSE_MOTION:
				{
					capture_mouse_xxx = event.motion.x;
					capture_mouse_yyy = event.motion.y;
					break;
				}
			}

			WMC_MouseCallback( renderer );
			WMC_RenderCallback( renderer );
		}

		SDL_DestroyRenderer( renderer );
		SDL_DestroyWindow( wnd );
		SDL_DestroyTexture( buttons_texture );

		TTF_CloseFont( font_big );
		TTF_CloseFont( font_small );
		TTF_DestroyRendererTextEngine( text_engine );

		SDL_DestroyAudioStream( capture );
		SDL_DestroyAudioStream( playback );

		SDL_DestroyMutex( mutex );

		FQ_PrintQueueInfo( queue );
		FQ_DestroyFreeQueue( queue );

		SDL_Quit();
	}

	return 0;
}

