#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class XSamplerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit XSamplerAudioProcessorEditor (XSamplerAudioProcessor&);
    ~XSamplerAudioProcessorEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void chooseSfzFile();
    void updatePathLabel();

    XSamplerAudioProcessor& processor;

    juce::Label                       pathLabel;
    juce::TextButton                  loadButton { "Load SFZ…" };
    juce::MidiKeyboardComponent       keyboard;
    juce::GenericAudioProcessorEditor genericEditor;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XSamplerAudioProcessorEditor)
};
