#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <sfizz.hpp>
#include <atomic>
#include <array>
#include <memory>

#include "Arpeggiator.h"

class XSamplerAudioProcessor
    : public juce::AudioProcessor,
      private juce::Timer,
      private juce::AudioProcessorValueTreeState::Listener
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

    bool loadSfzFile (const juce::File& file);
    juce::File getCurrentSfzFile() const;

    // Force-rebuild the overlay synchronously (tests / SFZ load).
    void flushOverlayNow();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState            keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    void rebuildAndApplyOverlay();
    void flushParamCCs (bool forceAll);
    void applyArpSettingsFromParams();
    void applyStereoWidth (juce::AudioBuffer<float>& buffer);

    std::unique_ptr<sfz::Sfizz> synth;
    juce::CriticalSection synthLock;

    std::atomic<bool> sfzLoaded { false };
    juce::File   currentSfzFile;
    juce::String currentSfzText;

    int    lastNumVoices    { -1 };
    double currentSampleRate{ 44100.0 };

    std::atomic<bool>         overlayDirty { false };
    std::atomic<juce::uint32> lastChangeMs { 0 };

    Arpeggiator arp;

    // Smoothed master gain & stereo width — applied per-sample to avoid
    // zipper noise on rapid parameter changes.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmooth;

    // Cache last sent HDCC values for change detection.
    std::array<float, 20> lastCC { };

    // Cached parameter pointers
    std::atomic<float>* pMasterGain      { nullptr };
    std::atomic<float>* pTuneGlobal      { nullptr };
    std::atomic<float>* pPitchbendRange  { nullptr };
    std::atomic<float>* pOctaveTranspose { nullptr };
    std::atomic<float>* pStartOffset     { nullptr };
    std::atomic<float>* pAnalogAmount    { nullptr };
    std::atomic<float>* pDoublerEnabled  { nullptr };
    std::atomic<float>* pVoiceMode       { nullptr };
    std::atomic<float>* pLegatoEnabled   { nullptr };
    std::atomic<float>* pPortamentoTime  { nullptr };
    std::atomic<float>* pFingeredPort    { nullptr };
    std::atomic<float>* pFilterType      { nullptr };
    std::atomic<float>* pFilterCutoff    { nullptr };
    std::atomic<float>* pFilterResonance { nullptr };
    std::atomic<float>* pVolAttack       { nullptr };
    std::atomic<float>* pVolDecay        { nullptr };
    std::atomic<float>* pVolSustain      { nullptr };
    std::atomic<float>* pVolRelease      { nullptr };
    std::atomic<float>* pFilterAttack    { nullptr };
    std::atomic<float>* pFilterDecay     { nullptr };
    std::atomic<float>* pFilterSustain   { nullptr };
    std::atomic<float>* pFilterRelease   { nullptr };
    std::atomic<float>* pLfoEnabled      { nullptr };
    std::atomic<float>* pLfoWaveform     { nullptr };
    std::atomic<float>* pLfoRate         { nullptr };
    std::atomic<float>* pLfoDepth        { nullptr };
    std::atomic<float>* pLfoDelay        { nullptr };
    std::atomic<float>* pLfoTarget       { nullptr };
    std::atomic<float>* pOutputWidth     { nullptr };
    std::atomic<float>* pVelToVolume     { nullptr };
    std::atomic<float>* pVelToFilter     { nullptr };

    std::atomic<float>* pArpEnabled  { nullptr };
    std::atomic<float>* pArpHold     { nullptr };
    std::atomic<float>* pArpMode     { nullptr };
    std::atomic<float>* pArpRate     { nullptr };
    std::atomic<float>* pArpOctaves  { nullptr };
    std::atomic<float>* pArpGate     { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XSamplerAudioProcessor)
};
