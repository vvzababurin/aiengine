// aiengine.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"

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


void capture_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	SDL_Log("data: %d %d\n", additional_amount, total_amount);

	int avail = SDL_GetAudioStreamAvailable(stream);

	SDL_Log("avail data: %d\n", avail);

	void* buf = malloc( avail );
	int read_bytes = SDL_GetAudioStreamData(stream, buf, avail);

	SDL_Log("read data: %d\n", read_bytes);

	int len = read_bytes / 4;

	float* data = (float*)buf;

	for ( int i = 0; i < len; i++ )
	{
		if ( data[i] > 0.3f || data[i] < -0.3f ) printf("%f; ", data[i]);
	}

	free( buf );

	return;
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

	SDL_Init(SDL_INIT_AUDIO);

	SDL_AudioDeviceID * audioDevices = SDL_GetAudioRecordingDevices( &count );
	if ( audioDevices == NULL ) {
		// TODO: fatal crash
		SDL_Log( "SDL_AudioDeviceID is NULL: ( %s )\n", SDL_GetError() );
		return -1;
	} else {
		for ( int i = 0; i < count; i++ )
		{
			const char* deviceName = SDL_GetAudioDeviceName( audioDevices[i] );
			SDL_Log( "DeviceId: %d ( %s )\n", audioDevices[i], SDL_iconv_utf8_locale( deviceName ) );
		}

		SDL_AudioSpec spec;
		SDL_AudioStream * stream = NULL;

		SDL_memset(&spec, 0, sizeof(spec));
		spec.format = SDL_AUDIO_F32;
		spec.channels = 1;
		spec.freq = 44100;
	
		stream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, capture_callback, NULL );
		if (stream == NULL) {
			SDL_Log("Failed to open audio: %s", SDL_GetError());
		} else {
			SDL_ResumeAudioStreamDevice( stream ); 
			SDL_Delay( 5000 ); 
			SDL_DestroyAudioStream( stream );
		}
	}

	return 0;
}


