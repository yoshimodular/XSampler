#include "PluginEditor.h"

XSamplerAudioProcessorEditor::XSamplerAudioProcessorEditor (XSamplerAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      genericEditor (p)
{
    pathLabel.setJustificationType (juce::Justification::centredLeft);
    pathLabel.setColour (juce::Label::backgroundColourId, juce::Colours::black.withAlpha (0.25f));
    pathLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    pathLabel.setBorderSize (juce::BorderSize<int> (4));
    addAndMakeVisible (pathLabel);

    loadButton.onClick = [this] { chooseSfzFile(); };
    addAndMakeVisible (loadButton);

    keyboard.setLowestVisibleKey (36);   // C2
    keyboard.setKeyWidth (18.0f);
    keyboard.setAvailableRange (0, 127);
    keyboard.setVelocity (0.8f, true);
    addAndMakeVisible (keyboard);

    addAndMakeVisible (genericEditor);

    updatePathLabel();
    setSize (560, 880);
}

XSamplerAudioProcessorEditor::~XSamplerAudioProcessorEditor() = default;

void XSamplerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e22));
}

void XSamplerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto top = area.removeFromTop (32);
    loadButton.setBounds (top.removeFromRight (110));
    top.removeFromRight (8);
    pathLabel.setBounds (top);

    area.removeFromTop (8);
    keyboard.setBounds (area.removeFromTop (90));

    area.removeFromTop (8);
    genericEditor.setBounds (area);
}

void XSamplerAudioProcessorEditor::updatePathLabel()
{
    const auto f = processor.getCurrentSfzFile();
    pathLabel.setText (f.existsAsFile() ? f.getFullPathName() : "No file loaded",
                       juce::dontSendNotification);
}

void XSamplerAudioProcessorEditor::chooseSfzFile()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Select an SFZ file…",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.sfz");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file.existsAsFile())
        {
            processor.loadSfzFile (file);
            updatePathLabel();
        }
    });
}
