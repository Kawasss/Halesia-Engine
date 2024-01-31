#pragma once
#include "soloud/include/soloud.h"
#include "soloud/include/soloud_wav.h"

typedef SoLoud::Wav WavSound;
typedef SoLoud::handle AudioHandle;

class Audio
{
public:
	static void Init();
	static void Destroy();
	static AudioHandle PlayWavSound(WavSound& sound);
	
private:
	static SoLoud::Soloud soloud;
};