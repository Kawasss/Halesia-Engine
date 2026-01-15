module;

#include <soloud/include/soloud.h>
#include <soloud/include/soloud_wav.h>

module Audio;

import std;

SoLoud::Soloud soloud;

void Audio::Init()
{
	#ifdef _DEBUG
	std::println("Initialized SoLoud (version: {})", SOLOUD_VERSION);
	#endif // _DEBUG
	soloud.init();
	
}

AudioHandle Audio::PlayWavSound(WavSound& sound)
{
	return soloud.play(sound);
}

void Audio::Destroy()
{
	soloud.deinit();
}