#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

namespace IDs
{
    inline constexpr const char* lSync = "lSync";   inline constexpr const char* rSync = "rSync";
    inline constexpr const char* lDiv  = "lDiv";    inline constexpr const char* rDiv  = "rDiv";
    inline constexpr const char* lTime = "lTime";   inline constexpr const char* rTime = "rTime";
    inline constexpr const char* lOff  = "lOff";    inline constexpr const char* rOff  = "rOff";
    inline constexpr const char* link  = "link";
    inline constexpr const char* feedback = "feedback";
    inline constexpr const char* freeze   = "freeze";
    inline constexpr const char* fOn      = "fOn";
    inline constexpr const char* fFreq    = "fFreq";
    inline constexpr const char* fWidth   = "fWidth";
    inline constexpr const char* mRate    = "mRate";
    inline constexpr const char* mFilter  = "mFilter";
    inline constexpr const char* mTime    = "mTime";
    inline constexpr const char* mode     = "mode";     // 0 Repitch, 1 Fade, 2 Jump
    inline constexpr const char* pingpong = "pingpong";
    inline constexpr const char* dryWet   = "dryWet";
}

class DelayAudioProcessor : public juce::AudioProcessor
{
public:
    DelayAudioProcessor();
    ~DelayAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Classic Delay"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static const std::array<int, 8> divisions;   // in sixteenth notes
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    struct Channel
    {
        std::vector<float> buf;
        int w = 0;
        double dSmooth = 0.0, d0 = 0.0, d1 = 0.0, fade = 1.0;
        bool init = false;
        float z1 = 0.f, z2 = 0.f, b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        void reset (int size) { buf.assign ((size_t) size, 0.f); w = 0; init = false; z1 = z2 = 0.f; fade = 1.0; }
    };

    float readTap (const Channel&, double delaySamples) const noexcept;
    float tap (Channel&, double target, int mode, double modSamples) noexcept;
    void setBandpass (Channel&, double freq, double q) noexcept;

    std::array<Channel, 2> ch;
    double sr = 44100.0, lfoPhase = 0.0;
    juce::SmoothedValue<float> smFeedback, smDryWet;
    std::array<std::atomic<float>*, 20> p {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayAudioProcessor)
};
