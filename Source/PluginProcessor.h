#pragma once
#include <JuceHeader.h>

class GazyavaProcessor : public juce::AudioProcessor
{
public:
    GazyavaProcessor();
    ~GazyavaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Gazyava"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void triggerMeow (int which, float pitch);   // 0 мяу, 1 вопль, 2 шипение
    void applyPreset (int idx);
    void randomize();
    static int getNumPresets();
    static juce::String getPresetName (int idx);
    void getScope (float* dst, int n) const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f }, bassLevel { 0.f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static float shape (float x, float k, int type) noexcept;
    void updateTargets (bool snap);
    void loadMeow (int idx, const void* data, int size);
    float getP (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    using Duplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    juce::dsp::Oversampling<float> oversampling { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::dsp::LinkwitzRileyFilter<float> xover;
    juce::dsp::StateVariableTPTFilter<float> tone, revHp, wob;
    juce::dsp::Limiter<float> limiter;
    juce::dsp::Compressor<float> comp;
    juce::dsp::Reverb reverb;
    juce::dsp::Chorus<float> chorus;
    Duplicator eqL, eqM, eqH;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> revDelay { 16384 }, echo { 131072 };
    juce::AudioBuffer<float> dryBuf, compBuf, revBuf;
    juce::SmoothedValue<float> sIn, sDrive, sBoost, sSub, sCrunch, sWidth, sSat, sDist, sOvl, sMix, sOut, sBypass;
    double sampleRate = 44100.0, freeBeats = 0.0;

    float subEnv[2] {}, subL1[2] {}, subL2[2] {}, tz1[2] {}, tz2[2] {}, held[2] {};
    int   subSgn[2] { 1, 1 }, crushCnt[2] {};
    bool  subFlip[2] {};
    float fizzSm = 0.f, fizzTarget = 0.f, revLp[2] {}, duckEnv = 0.f;
    int   fizzCnt = 0;
    float gateEnv = 0.f, gateGain = 1.f, trPhase = 0.f, pumpG = 1.f, echoLp[2] {};
    float lastEq[4] { 1e9f, 1e9f, 1e9f, 1e9f };
    juce::Random rnd;

    std::array<float, 4096> scope {};
    std::atomic<int> scopePos { 0 };
    int scopeW = 0;

    juce::AudioBuffer<float> meow[3];
    double meowRate[3] { 44100.0, 44100.0, 44100.0 }, voicePos = 0.0, voiceStep = 1.0;
    std::atomic<int> meowReq { -1 };
    std::atomic<float> meowPitch { 1.f };
    int voice = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GazyavaProcessor)
};
