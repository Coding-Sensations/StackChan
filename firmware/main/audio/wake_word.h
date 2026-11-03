#ifndef WAKE_WORD_H
#define WAKE_WORD_H

#include <string>
#include <companion/audio_owner.hpp>
#include <vector>
#include <functional>

#include <model_path.h>
#include "audio_codec.h"

class WakeWord {
public:
    virtual ~WakeWord() = default;
    // Checked APIs default closed for unported children/configurations.
    virtual bool SupportsCheckedLifecycle() const { return false; }
    virtual bool BindAudioLease(companion::AudioLease) { return false; }
    virtual bool StartChecked() { return false; }
    virtual bool StopChecked(uint32_t=2000) { return false; }
    virtual bool PauseChecked(uint32_t=2000) { return false; }
    virtual bool EncodeWakeWordDataChecked() { return false; }

    
    virtual bool Initialize(AudioCodec* codec, srmodel_list_t* models_list) = 0;
    virtual void Feed(const std::vector<int16_t>& data) = 0;
    virtual void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual size_t GetFeedSize() = 0;
    virtual void EncodeWakeWordData() = 0;
    virtual bool GetWakeWordOpus(std::vector<uint8_t>& opus) = 0;
    virtual const std::string& GetLastDetectedWakeWord() const = 0;
};

#endif
