#pragma once
#include "MainLib.h"
#define SYNTH_LENGHT 16

#define TIMER_DELAY 100
#define PAYLOAD_MS 10000
#define PAYLOAD_TIME ( PAYLOAD_MS / TIMER_DELAY )

#define SineWave(t, freq, sampleRate) \
    FastSine(2.f * PI * (freq) * (t) / (sampleRate))

#define SquareWave(t, freq, sampleRate) \
    ((((int)((t) * (int)(freq) / (sampleRate))) & 1) ? 1.f : -1.f)

#define TriangleWave(t, freq, sampleRate) \
    ((float)(((int)((t) * (int)(freq) / (sampleRate))) & 0xFF) / 128.f - 1.f)

#define SawtoothWave(t, freq, sampleRate) \
    ((float)(((int)((t) * (int)(freq) / (sampleRate))) & 0xFF) / 128.f - 1.f)




VOID
WINAPI
BeepEx(
	_In_ INT nWaveformIndex,
	_In_ INT nDuration,
	_In_ PSHORT psFreq,
	_In_ PFLOAT pfVolume
);

VOID
WINAPI
AudioPayloadThread( VOID );

VOID
WINAPI
AudioSequenceThread(
	_In_ PAUDIO_SEQUENCE_PARAMS pAudioParams
);

VOID
WINAPI
ExecuteAudioSequence(
	_In_ INT nSamplesPerSec,
	_In_ INT nSampleCount,
	_In_ AUDIO_SEQUENCE pAudioSequence,
	_In_opt_ AUDIOSEQUENCE_OPERATION pPreSynthOp,
	_In_opt_ AUDIOSEQUENCE_OPERATION pPostSynthOp
);

VOID
WINAPI
AudioSequence1(
    INT nSamplesPerSec,
    INT nSampleCount,
    PSHORT psSamples
);
