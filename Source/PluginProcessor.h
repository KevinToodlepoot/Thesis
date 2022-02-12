/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Modulator.h"

#define LEFT_CHANNEL    0
#define RIGHT_CHANNEL   1
#define NUM_HARM        200


struct ChainSettings
{
    float freq {0};
    float q {0};
    float curve {0};
    float oddGain {0};
    float evenGain {0};
    float fundGain {0};
    
    float modFreq {0};
    float modDepth {0};
    
    int quality {0};
    bool noise {0};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class ThesisAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ThesisAudioProcessor();
    ~ThesisAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};
    
    Modulator mod;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    
    using Gain = juce::dsp::Gain<float>;
    
    using BPChain = juce::dsp::ProcessorChain<Filter, Gain, Gain>;
    using APChain = juce::dsp::ProcessorChain<Filter, Filter>;
    
    BPChain leftChain[NUM_HARM], rightChain[NUM_HARM];
    
    APChain leftAPChain, rightAPChain;
    
    enum BPChainPositions
    {
        BPFilter,
        CurveGain,
        OddEvenGain
    };
    
    enum APChainPositions
    {
        APFilter1,
        APFilter2
    };
    
    using Coefficients = Filter::CoefficientsPtr;
    
    void updateAll(float modVal = 0.f);
    static void updateCoefficients (Coefficients& old, const Coefficients& replacements);
    void updateFilter (const ChainSettings& chainSettings, float modVal, int i);
    void updateCurveGain (const ChainSettings& chainSettings, int i);
    void updateOddEvenGain (const ChainSettings& chainSettings, int i);
    
    float applyFilter(float samp, int i);
    float applyCurveGain(float samp, int i);
    float applyOddEvenGain(float samp, int i);
    

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThesisAudioProcessor)
};
