#include "termite/tee.h"
#include "termite/sound_driver.h"
#define TEE_CORE_API
#define TEE_ASSET_API
#include "termite/plugin_api.h"

#include "SDL_mixer.h"
#include "beep_ogg.h"
#include "blank_ogg.h"

using namespace tee;

class SoundLoader : public AssetLibCallbacksI
{
public:
    SoundLoader() {}

    bool loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
    void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
    void onReload(AssetHandle handle, bx::AllocatorI* alloc) override;
};

class MusicLoader : public AssetLibCallbacksI
{
public:
    MusicLoader() {}

    bool loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc) override;
    void unloadObj(uintptr_t obj, bx::AllocatorI* alloc) override;
    void onReload(AssetHandle handle, bx::AllocatorI* alloc) override;
};

struct MusicData
{
    void* buff;
    uint32_t size;
    Mix_Music* mus;

    MusicData() :
        buff(nullptr),
        size(0),
        mus(nullptr)
    {
    }
};

struct MixerWrapper
{
    bx::AllocatorI* alloc;
    CoreApi* core;
    AssetApi* asset;

    SoundLoader loader;
    MusicLoader musLoader;
    bool soundEnabled;
    bool musicEnabled;

    SoundFinishedCallback soundFinishedFn;
    void* soundFinishedUserData;

    MusicFinishedCallback musicFinishedFn;
    void* musicFinishedUserData;

    Mix_Chunk* failChunk;
    Mix_Chunk* asyncChunk;

    MixerWrapper()
    {
        alloc = nullptr;
        core = nullptr;
        soundFinishedFn = nullptr;
        soundFinishedUserData = nullptr;
        failChunk = nullptr;
        asyncChunk = nullptr;
        musicFinishedFn = nullptr;
        musicFinishedUserData = nullptr;
        soundEnabled = true;
        musicEnabled = true;
    }
};

static MixerWrapper gSdlMixer;

static void mixerSoundFinishedCallback(int channelId)
{
    BX_ASSERT(gSdlMixer.soundFinishedFn);
    gSdlMixer.soundFinishedFn(channelId, gSdlMixer.soundFinishedUserData);
}

static void mixerMusicFinishedCallback()
{
    BX_ASSERT(gSdlMixer.musicFinishedFn);
    gSdlMixer.musicFinishedFn(gSdlMixer.musicFinishedUserData);
}

static bool mixerInit(AudioFreq::Enum freq/* = AudioFreq::Freq22Khz*/,
                          AudioChannels::Enum channels/* = AudioChannels::Mono*/,
                          int bufferSize/* = 4096*/)
{
    BX_ASSERT(gSdlMixer.core);
    BX_ASSERT(gSdlMixer.alloc);

    if (Mix_OpenAudio(int(freq), MIX_DEFAULT_FORMAT, int(channels), bufferSize)) {
        TEE_ERROR_API(gSdlMixer.core, "Initializing SDL_AudioMixer failed: %s", Mix_GetError());
        return false;
    }
    
    Mix_Init(MIX_INIT_OGG);
    Mix_GetError();

    // Create Beep sound for failed loads
    gSdlMixer.failChunk = Mix_LoadWAV_RW(SDL_RWFromConstMem(kBeepOGG, sizeof(kBeepOGG)), 1);
    gSdlMixer.asyncChunk = Mix_LoadWAV_RW(SDL_RWFromConstMem(kBlankOGG, sizeof(kBlankOGG)), 1);

    // Register sound loader to resource_lib
    gSdlMixer.asset->registerType("sound", &gSdlMixer.loader, 0, uintptr_t(gSdlMixer.failChunk), uintptr_t(gSdlMixer.asyncChunk));
    gSdlMixer.asset->registerType("music", &gSdlMixer.musLoader, 0, 0, 0);

    return true;
}

static void mixerShutdown()
{
    BX_ASSERT(gSdlMixer.core);
    BX_ASSERT(gSdlMixer.alloc);

    if (gSdlMixer.failChunk)
        Mix_FreeChunk(gSdlMixer.failChunk);
    if (gSdlMixer.asyncChunk)
        Mix_FreeChunk(gSdlMixer.asyncChunk);

    Mix_Quit();
    Mix_CloseAudio();
}

static uint8_t mixerSetChunkVolume(SoundChunk* handle, uint8_t vol)
{
    static const uint8_t N = UINT8_MAX/MIX_MAX_VOLUME;
    return handle ? Mix_VolumeChunk((Mix_Chunk*)handle, vol/N)*N : 0;
}

static int mixerSetChannels(int numChannels)
{
    return Mix_AllocateChannels(numChannels);
}

int mixerReserveChannels(int numChannels)
{
    return Mix_ReserveChannels(numChannels);
}

bool mixerTagChannel(int channelId, int tag/* = -1*/)
{
    return Mix_GroupChannel(channelId, tag) ? true : false;
}

int mixerTagChannels(int fromChannelId, int toChannelId, int tag/* = -1*/)
{
    return Mix_GroupChannels(fromChannelId, toChannelId, tag);
}

int mixerGetTagChannelCount(int tag/* = -1*/)
{
    return Mix_GroupCount(tag);
}

int mixerGetAvailChannel(int tag/* = -1*/)
{
    return  Mix_GroupAvailable(tag);
}

int mixerGetActiveChannelOldest(int tag)
{
    return Mix_GroupOldest(tag);
}

int mixerGetActiveChannelNewest(int tag)
{
    return Mix_GroupNewer(tag);
}

void mixerFadeoutTag(int tag, int timeMilli)
{
    Mix_FadeOutGroup(tag, timeMilli);
}

void mixerStopTag(int tag)
{
    Mix_HaltGroup(tag);
}

uint8_t mixerSetVolume(int channelId, uint8_t vol)
{
    static const uint8_t N = UINT8_MAX/MIX_MAX_VOLUME;
    return Mix_Volume(channelId, vol/N)*N;
}

uint8_t mixerSetMusicVolume(uint8_t vol)
{
    return Mix_VolumeMusic(vol/2)*2;
}

void mixerPause(int channelId)
{
    Mix_Pause(channelId);
}

void mixerResume(int channelId)
{
    Mix_Resume(channelId);
}

void mixerStop(int channelId)
{
    Mix_HaltChannel(channelId);
}

void mixerStopAfterTime(int channelId, int timeMilli)
{
    Mix_ExpireChannel(channelId, timeMilli);
}

void mixerFadeout(int channelId, int timeMilli)
{
    Mix_FadeOutChannel(channelId, timeMilli);
}

void mixerSetFinishedCallback(SoundFinishedCallback callback, void* userData)
{
    gSdlMixer.soundFinishedFn = callback;
    gSdlMixer.soundFinishedUserData = userData;
    Mix_ChannelFinished(callback ? mixerSoundFinishedCallback : nullptr);
}

bool mixerIsPlaying(int channelId)
{
    return Mix_Playing(channelId) ? true : false;
}

bool mixerIsPaused(int channelId)
{
    return Mix_Paused(channelId) ? true : false;
}

void mixerSetGlobalSoundEnabled(bool enabled)
{
    gSdlMixer.soundEnabled = enabled;
}

void mixerSetGlobalMusicEnabled(bool enabled)
{
    gSdlMixer.musicEnabled = enabled;
}

SoundFadeStatus::Enum mixerGetFadingStatus(int channelId)
{
    switch (Mix_FadingChannel(channelId)) {
    case MIX_NO_FADING:
        return SoundFadeStatus::NoFading;
    case MIX_FADING_OUT:
        return SoundFadeStatus::FadingOut;
    case MIX_FADING_IN:
        return SoundFadeStatus::FadingIn;
    default:
        BX_ASSERT(0);
        return SoundFadeStatus::NoFading;
    }
}

SoundChunk* mixerGetChannelChunk(int channelId)
{
    Mix_Chunk* chunk = Mix_GetChunk(channelId);
    return (SoundChunk*)chunk;
}

int mixerPlay(int channelId, SoundChunk* handle, int numLoops/* = 0*/)
{
    if (handle && gSdlMixer.soundEnabled)
        return Mix_PlayChannel(channelId, (Mix_Chunk*)handle, numLoops);
    else
        return 0;
}

int mixerPlayTimed(int channelId, SoundChunk* handle, int numLoops/* = 0*/, int maxTimeMilli/* = -1*/)
{
    if (handle && gSdlMixer.soundEnabled)
        return Mix_PlayChannelTimed(channelId, (Mix_Chunk*)handle, numLoops, maxTimeMilli);
    else
        return 0;
}

int mixerPlayFadeIn(int channelId, SoundChunk* handle, int numLoops, int timeMilli)
{
    if (handle && gSdlMixer.soundEnabled)
        return Mix_FadeInChannel(channelId, (Mix_Chunk*)handle, numLoops, timeMilli);
    else
        return 0;
}

int mixerPlayFadeInTimed(int channelId, SoundChunk* handle, int numLoops/* = 0*/, int timeMilli, int maxTimeMilli/* = -1*/)
{
    if (handle && gSdlMixer.soundEnabled)
        return Mix_FadeInChannelTimed(channelId, (Mix_Chunk*)handle, numLoops, timeMilli, maxTimeMilli);
    else
        return 0;
}

bool mixerPlayMusic(Music* handle, int numLoops/* = -1*/)
{
    if (handle && gSdlMixer.musicEnabled)
        return Mix_PlayMusic(((MusicData*)handle)->mus, numLoops) == 0 ? true : false;
    else
        return false;
}

bool mixerPlayMusicFadeIn(Music* handle, int numLoops/* = -1*/, int timeMilli)
{
    if (handle && gSdlMixer.musicEnabled)
        return Mix_FadeInMusic(((MusicData*)handle)->mus, numLoops, timeMilli) == 0 ? true : false;
    else
        return false;
}

bool mixerPlayMusicFadeInPos(Music* handle, int numLoops/* = -1*/, int timeMilli, double posTime)
{
    if (handle && gSdlMixer.musicEnabled)
        return Mix_FadeInMusicPos(((MusicData*)handle)->mus, numLoops, timeMilli, posTime) == 0 ? true : false;
    else
        return false;
}

bool mixerSetMusicPos(double posTime)
{
    return Mix_SetMusicPosition(posTime) == 0 ? true : false;
}

void mixerPauseMusic()
{
    Mix_PauseMusic();
}

void mixerResumeMusic()
{
    Mix_ResumeMusic();
}

void mixerRewindMusic()
{
    Mix_RewindMusic();
}

void mixerStopMusic()
{
    Mix_HaltMusic();
}

void mixerFadeoutMusic(int timeMilli)
{
    Mix_FadeOutMusic(timeMilli);
}

void mixerSetMusicFinishedCallback(MusicFinishedCallback callback, void* userData)
{
    gSdlMixer.musicFinishedFn = callback;
    gSdlMixer.musicFinishedUserData = userData;
    Mix_HookMusicFinished(callback ? mixerMusicFinishedCallback : nullptr);
}

bool mixerIsMusicPlaying()
{
    return Mix_PlayingMusic() ? true : false;
}

bool mixerIsMusicPaused()
{
    return Mix_PausedMusic() ? true : false;
}

SoundFadeStatus::Enum mixerGetMusicStatus()
{
    return SoundFadeStatus::NoFading;
}

bool SoundLoader::loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
{
    SDL_RWops* rw = SDL_RWFromConstMem(mem->data, (int)mem->size);
    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1);
    if (!chunk)
        return false;

    *obj = uintptr_t(chunk);
    return true;
}

void SoundLoader::unloadObj(uintptr_t obj, bx::AllocatorI* alloc)
{
    Mix_Chunk* chunk = (Mix_Chunk*)obj;
    if (chunk) {
        Mix_FreeChunk(chunk);
    }
}

void SoundLoader::onReload(AssetHandle handle, bx::AllocatorI* alloc)
{
}


bool MusicLoader::loadObj(const MemoryBlock* mem, const AssetParams& params, uintptr_t* obj, bx::AllocatorI* alloc)
{
    MusicData* mdata;
    if (alloc)
        mdata = BX_NEW(alloc, MusicData);
    else
        mdata = BX_NEW(gSdlMixer.alloc, MusicData);

    // Copy memory to data
    mdata->buff = BX_ALLOC(alloc ? alloc : gSdlMixer.alloc, mem->size);
    if (!mdata->buff)
        return false;
    memcpy(mdata->buff, mem->data, mem->size);
    mdata->size = mem->size;

    // Create music
    SDL_RWops* rw = SDL_RWFromConstMem(mdata->buff, mdata->size);
    mdata->mus = Mix_LoadMUS_RW(rw, 1);
    if (!mdata->mus)
        return false;

    *obj = uintptr_t(mdata);
    return true;
}

void MusicLoader::unloadObj(uintptr_t obj, bx::AllocatorI* alloc)
{
    MusicData* mdata = (MusicData*)obj;
    if (mdata->mus) {
        Mix_FreeMusic(mdata->mus);
    }

    if (mdata->buff) {
        BX_FREE(alloc ? alloc : gSdlMixer.alloc, mdata->buff);
    }
    BX_DELETE(alloc ? alloc : gSdlMixer.alloc, mdata);
}

void MusicLoader::onReload(AssetHandle handle, bx::AllocatorI* alloc)
{

}

//
PluginDesc* getSdlMixerDriverDesc()
{
    static PluginDesc desc;
    strcpy(desc.name, "SDL_mixer");
    strcpy(desc.description, "SDL_mixer Driver");
    desc.type = PluginType::SimpleSoundDriver;
    desc.version = TEE_MAKE_VERSION(1, 0);
    return &desc;
}

void* initSdlMixerDriver(bx::AllocatorI* alloc, GetApiFunc getApi)
{
    gSdlMixer.core = (CoreApi*)getApi(uint16_t(ApiId::Core), 0);
    gSdlMixer.asset = (AssetApi*)getApi(uint16_t(ApiId::Asset), 0);
    if (!gSdlMixer.core || !gSdlMixer.asset)
        return nullptr;
    gSdlMixer.alloc = alloc;
    
    static SimpleSoundDriver soundApi;
    bx::memSet(&soundApi, 0x00, sizeof(soundApi));

    soundApi.init = mixerInit;
    soundApi.shutdown = mixerShutdown;
    soundApi.setChunkVolume = mixerSetChunkVolume;
    soundApi.setChannels = mixerSetChannels;
    soundApi.reserveChannels = mixerReserveChannels;
    soundApi.tagChannel = mixerTagChannel;
    soundApi.tagChannels = mixerTagChannels;
    soundApi.getTagChannelCount = mixerGetTagChannelCount;
    soundApi.getAvailChannel = mixerGetAvailChannel;
    soundApi.getActiveChannelNewest = mixerGetActiveChannelNewest;
    soundApi.getActiveChannelOldest = mixerGetActiveChannelOldest;
    soundApi.fadeoutTag = mixerFadeoutTag;
    soundApi.stopTag = mixerStopTag;
    soundApi.setVolume = mixerSetVolume;
    soundApi.pause = mixerPause;
    soundApi.resume = mixerResume;
    soundApi.stop = mixerStop;
    soundApi.stopAfterTime = mixerStopAfterTime;
    soundApi.fadeout = mixerFadeout;
    soundApi.setFinishedCallback = mixerSetFinishedCallback;
    soundApi.isPlaying = mixerIsPlaying;
    soundApi.isPaused = mixerIsPaused;
    soundApi.getFadingStatus = mixerGetFadingStatus;
    soundApi.getChannelChunk = mixerGetChannelChunk;
    soundApi.play = mixerPlay;
    soundApi.playTimed = mixerPlayTimed;
    soundApi.playFadeIn = mixerPlayFadeIn;
    soundApi.playFadeInTimed = mixerPlayFadeInTimed;
    soundApi.playMusic = mixerPlayMusic;
    soundApi.playMusicFadeIn = mixerPlayMusicFadeIn;
    soundApi.playMusicFadeInPos = mixerPlayMusicFadeInPos;
    soundApi.setMusicPos = mixerSetMusicPos;
    soundApi.pauseMusic = mixerPauseMusic;
    soundApi.resumeMusic = mixerResumeMusic;
    soundApi.rewindMusic = mixerRewindMusic;
    soundApi.stopMusic = mixerStopMusic;
    soundApi.fadeoutMusic = mixerFadeoutMusic;
    soundApi.setMusicFinishedCallback = mixerSetMusicFinishedCallback;
    soundApi.isMusicPlaying = mixerIsMusicPlaying;
    soundApi.isMusicPaused = mixerIsMusicPaused;
    soundApi.getMusicStatus = mixerGetMusicStatus;
    soundApi.setGlobalMusicEnabled = mixerSetGlobalMusicEnabled;
    soundApi.setGlobalSoundEnabled = mixerSetGlobalSoundEnabled;
    soundApi.setMusicVolume = mixerSetMusicVolume;

    return &soundApi;
}

void shutdownSdlMixerDriver()
{
}

#ifdef termite_SHARED_LIB
TEE_PLUGIN_EXPORT void* termiteGetPluginApi(uint16_t apiId, uint32_t version)
{
    static PluginApi v0;

    if (version == 0) {
        v0.init = initSdlMixerDriver;
        v0.shutdown = shutdownSdlMixerDriver;
        v0.getDesc = getSdlMixerDriverDesc;
        return &v0;
    } else {
        return nullptr;
    }
}
#endif
