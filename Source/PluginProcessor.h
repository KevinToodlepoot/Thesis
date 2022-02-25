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
    /* Main Section */
    float timbre {0};
    float freq {0};
    float q {0};
    
    /* Secondary Section */
    float stereoLink {0};
    int quality {0};
    float curve {0};
    float detune {0};
    
    /* Mod Section */
    bool modFreq {0};
    bool modDetune {0};
    float modRate {0};
    float modDepth {0};

    float oddGain {0};
    float evenGain {0};
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
    
    std::vector<float> modVector;

private:
    void processWithMod(juce::AudioBuffer<float>& buffer, const ChainSettings& chainSettings, float* modVal, int harm);
    void processNoMod(juce::AudioBuffer<float>& buffer, int harm);
    
    float wrap(float x, int sampleRate);
    float getCurFreq(const ChainSettings& chainSettings, int harm);
    
    using SVFilter = juce::dsp::StateVariableTPTFilter<float>;
    
    using Gain = juce::dsp::Gain<float>;
    
    using BPChain = juce::dsp::ProcessorChain<SVFilter, Gain, Gain>;
    
    BPChain leftChain[NUM_HARM], rightChain[NUM_HARM];
    
    enum BPChainPositions
    {
        BPFilter,
        CurveGain,
        OddEvenGain
    };
    
    void updateAll();
    bool updateSVFilter (const ChainSettings& chainSettings, int i);
    void updateCurveGain (const ChainSettings& chainSettings, int i);
    void updateOddEvenGain (const ChainSettings& chainSettings, int i);
    

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThesisAudioProcessor)
};
