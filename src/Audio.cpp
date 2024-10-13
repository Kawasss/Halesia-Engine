#include <iostream>
#include "Audio.h"

SoLoud::Soloud soloud;

void Audio::Init()
{
	#ifdef _DEBUG
	std::cout << "Initialized SoLoud (version: " << SOLOUD_VERSION << ")\n";
	#endif
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