#pragma once

#include <juce_core/juce_core.h>

struct XSamplerSfzParams
{
    int   tuneCents      { 0 };       // -100..100
    bool  mono           { false };
    bool  legato         { false };
    int   filterType     { 0 };       // 0=LP,1=HP,2=BP
    float filterCutoff   { 8000.0f };
    float filterResonance{ 0.0f };    // 0..1
    float volA           { 0.01f };
    float volD           { 0.1f };
    float volS           { 0.8f };    // 0..1
    float volR           { 0.3f };
    float fltA           { 0.01f };
    float fltD           { 0.1f };
    float fltS           { 0.8f };
    float fltR           { 0.3f };
    bool  lfoEnabled     { false };
    int   lfoWave        { 0 };       // 0..4 (Sine,Tri,Saw,Sq,Random)
    float lfoRate        { 2.0f };    // Hz
    float lfoDepth       { 0.0f };    // 0..1
    float lfoDelay       { 0.0f };    // seconds
    int   lfoTarget      { 0 };       // 0=Pitch,1=Filter,2=Volume
    float velToVolume    { 0.8f };    // 0..1
    float velToFilter    { 0.0f };    // 0..1
    float analogAmount   { 0.0f };    // 0..1
};

// Build a combined SFZ source: a synthesised <global> override block followed
// by the original file contents. Returns empty if the file can't be read.
juce::String buildSfzWithOverride (const juce::File& originalSfz,
                                   const XSamplerSfzParams& p);
