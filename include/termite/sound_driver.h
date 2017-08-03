#pragma once

#include "bx/bx.h"
#include "bx/allocator.h"
#include "types.h"

namespace termite
{
    struct AudioFreq
    {
        enum Enum
        {
            Freq22Khz = 22050,
            Freq44Khz = 44100
        };
    };

    struct AudioChannels
    {
        enum Enum
        {
            Mono = 1,
            Stereo = 2
        };
    };

    struct SoundFadeStatus
    {
        enum Enum
        {
            NoFading,
            FadingOut,
            FadingIn
        };
    };

    struct SoundChunkT {};
    struct MusicT {};
    typedef PhantomType<void*, SoundChunkT, nullptr> SoundChunkHandle;
    typedef PhantomType<void*, MusicT, nullptr> MusicHandle;

    typedef void (*SoundFinishedCallback)(int channelId, void* userData);
    typedef void (*MusicFinishedCallback)(void* userData);

    /// This is actually SDL_mixer wrapper
    struct SoundDriverApi
    {
        result_t(*init)(AudioFreq::Enum freq/* = AudioFreq::Freq22Khz*/, 
                        AudioChannels::Enum channels/* = AudioChannels::Mono*/,
                        int bufferSize/* = 4096*/);
        void(*shutdown)();

        // Chunks
        /// Returns previous volume
        uint8_t (*setChunkVolume)(SoundChunkHandle handle, uint8_t vol);
        
        // Channels
        int(*setChannels)(int numChannels);

        /// Reserves N channels from being using when playing samples passing -1 as channel number
        /// Channels are reserved from 0~N-1, passing 0 will unreserve all channels
        int(*reserveChannels)(int numChannels);

        /// tag = -1, resets channel tag
        bool(*tagChannel)(int channelId, int tag/* = -1*/);
        int(*tagChannels)(int fromChannelId, int toChannelId, int tag/* = -1*/);

        /// tag = -1, get all channels
        int(*getTagChannelCount)(int tag/* = -1*/);

        /// tag = -1, search in all channels
        int(*getAvailChannel)(int tag/* = -1*/);
        int(*getActiveChannelOldest)(int tag);
        int(*getActiveChannelNewest)(int tag);

        /// Returns number of channels that is set to fade-out
        void(*fadeoutTag)(int tag, int timeMilli);
        void(*stopTag)(int tag);

        /// channelId=-1 , applies to all channels
        uint8_t(*setVolume)(int channelId, uint8_t vol);
        void(*pause)(int channelId);
        void(*resume)(int channelId);
        void(*stop)(int channelId);
        void(*stopAfterTime)(int channelId, int timeMilli);
        void(*fadeout)(int channelId, int timeMilli);
        void(*setFinishedCallback)(SoundFinishedCallback callback, void* userData);
        bool(*isPlaying)(int channelId);
        bool(*isPaused)(int channelId);
        SoundFadeStatus::Enum (*getFadingStatus)(int channelId);
        SoundChunkHandle(*getChannelChunk)(int channelId); 

        /// numLoops = -1, infinite loop
        /// channelId = -1, play on a free 
        /// Returns the channel being played on, or -1 if any errors happened
        int(*play)(int channelId, SoundChunkHandle handle, int numLoops/* = 0*/);

        /// maxTimeMilli = -1, play infinite just like 'play'
        int(*playTimed)(int channelId, SoundChunkHandle handle, int numLoops/* = 0*/, int maxTimeMilli/* = -1*/);

        int(*playFadeIn)(int channelId, SoundChunkHandle handle, int numLoops, int timeMilli);

        int(*playFadeInTimed)(int channelId, SoundChunkHandle handle, int numLoops/* = 0*/, int timeMilli, 
                              int maxTimeMilli/* = -1*/);

        // Music
        bool(*playMusic)(MusicHandle handle, int numLoops/* = -1*/);
        bool(*playMusicFadeIn)(MusicHandle handle, int numLoops/* = -1*/, int timeMilli);
        bool(*playMusicFadeInPos)(MusicHandle handle, int numLoops/* = -1*/, int timeMilli, double posTime);
        bool(*setMusicPos)(double posTime);
        void(*pauseMusic)();
        void(*resumeMusic)();
        void(*rewindMusic)();
        void(*stopMusic)();
        void(*fadeoutMusic)(int timeMilli);
        void(*setMusicFinishedCallback)(MusicFinishedCallback callback, void* userData);
        bool(*isMusicPlaying)();
        bool(*isMusicPaused)();
        SoundFadeStatus::Enum(*getMusicStatus)();

        void(*setGlobalSoundEnabled)(bool enabled);
        void(*setGlobalMusicEnabled)(bool enabled);
    };
};
