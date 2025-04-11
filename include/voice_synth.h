#pragma once

#ifdef VOICE_SYNTH_EXPORTS
#define VOICE_SYNTH_API __declspec(dllexport)
#else
#define VOICE_SYNTH_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct VoiceData {
    const unsigned char* data;
    int size;
};

VOICE_SYNTH_API bool synthesize_text(
    const wchar_t* text,
    const wchar_t* voice_name,
    VoiceData* out_voice
);

VOICE_SYNTH_API void free_voice_data(const VoiceData* voice);

#ifdef __cplusplus
}
#endif
