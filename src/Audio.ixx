module;

#include <soloud/include/soloud.h>
#include <soloud/include/soloud_wav.h>

export module Audio;

export using WavSound    = SoLoud::Wav;
export using AudioHandle = SoLoud::handle;

export class Audio
{
public:
	static void Init();
	static void Destroy();
	static AudioHandle PlayWavSound(WavSound& sound);

private:
};