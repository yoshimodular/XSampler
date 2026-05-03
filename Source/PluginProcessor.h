#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <sfizz.hpp>
#include <atomic>
#include <memory>

class XSamplerAudioProcessor : public juce::AudioProcessor
{
public:
    XSamplerAudioProcessor();
    ~XSamplerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "XSampler"; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // SFZ loading. Safe to call from UI thread; actual swap is guarded.
    bool loadSfzFile (const juce::File& file);
    juce::File getCurrentSfzFile() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void applyParametersToSfizz();
    void applyStereoWidth (juce::AudioBuffer<float>& buffer, float width);

    std::unique_ptr<sfz::Sfizz> synth;
    juce::CriticalSection synthLock;

    std::atomic<bool> sfzLoaded { false };
    juce::File currentSfzFile;

    double currentSampleRate { 44100.0 };
    int    currentBlockSize  { 512 };

    // Cached parameter pointers
    std::atomic<float>* pMasterGain        { nullptr };
    std::atomic<float>* pTuneGlobal        { nullptr };
    std::atomic<float>* pPitchbendRange    { nullptr };
    std::atomic<float>* pOctaveTranspose   { nullptr };
    std::atomic<float>* pStartOffset       { nullptr };
    std::atomic<float>* pAnalogAmount      { nullptr };
    std::atomic<float>* pDoublerEnabled    { nullptr };
    std::atomic<float>* pVoiceMode         { nullptr };
    std::atomic<float>* pLegatoEnabled     { nullptr };
    std::atomic<float>* pPortamentoTime    { nullptr };
    std::atomic<float>* pFingeredPort      { nullptr };
    std::atomic<float>* pFilterType        { nullptr };
    std::atomic<float>* pFilterCutoff      { nullptr };
    std::atomic<float>* pFilterResonance   { nullptr };
    std::atomic<float>* pVolAttack         { nullptr };
    std::atomic<float>* pVolDecay          { nullptr };
    std::atomic<float>* pVolSustain        { nullptr };
    std::atomic<float>* pVolRelease        { nullptr };
    std::atomic<float>* pFilterAttack      { nullptr };
    std::atomic<float>* pFilterDecay       { nullptr };
    std::atomic<float>* pFilterSustain     { nullptr };
    std::atomic<float>* pFilterRelease     { nullptr };
    std::atomic<float>* pLfoEnabled        { nullptr };
    std::atomic<float>* pLfoWaveform       { nullptr };
    std::atomic<float>* pLfoRate           { nullptr };
    std::atomic<float>* pLfoDepth          { nullptr };
    std::atomic<float>* pLfoDelay          { nullptr };
    std::atomic<float>* pLfoTarget         { nullptr };
    std::atomic<float>* pOutputWidth       { nullptr };
    std::atomic<float>* pVelToVolume       { nullptr };
    std::atomic<float>* pVelToFilter       { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XSamplerAudioProcessor)
};
