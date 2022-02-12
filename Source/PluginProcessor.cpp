/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ThesisAudioProcessor::ThesisAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

ThesisAudioProcessor::~ThesisAudioProcessor()
{
}

//==============================================================================
const juce::String ThesisAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ThesisAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ThesisAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ThesisAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ThesisAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ThesisAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ThesisAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ThesisAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ThesisAudioProcessor::getProgramName (int index)
{
    return {};
}

void ThesisAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ThesisAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    
    mod.initMod(sampleRate);
    mod.setMod(1.f);
    
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    
    for (int i = 0; i < NUM_HARM; i++)
    {
        leftChain[i].prepare(spec);
        rightChain[i].prepare(spec);
    }
    
    updateAll(0.f);
}

void ThesisAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    for (int i = 0; i < NUM_HARM; i++)
    {
        leftChain[i].reset();
        rightChain[i].reset();
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ThesisAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ThesisAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto chainSettings = getChainSettings(apvts);
    int bufferSize = buffer.getNumSamples();
    
    float freq, modVal[bufferSize], leftSamp, rightSamp;
    
    float nyquist = float(getSampleRate()) / 2.f;
    
    juce::AudioBuffer<float> noiseBuffer;
    noiseBuffer.makeCopyOf(buffer);
    
    juce::Random noiseGen;
    
    /* fill buffer with noise */
    if (chainSettings.noise)
        for (int i = 0; i < buffer.getNumChannels(); i++)
            for (int samp = 0; samp < buffer.getNumSamples(); samp++)
            {
                buffer.setSample(i, samp, (noiseGen.nextFloat() * 2.f) - 1.f);
            }
    
//    updateAll();
    
    for (int i = 0; i < bufferSize; i++)
    {
        modVal[i] = mod.modBlock(bufferSize)[i];
        DBG(modVal[i]);
    }
    
    juce::AudioBuffer<float> effectBuffer;
    effectBuffer.makeCopyOf(buffer);
    
    juce::AudioBuffer<float> tempBuffer;
    
    for (int i = 0; i < chainSettings.quality; i++)
    {
        tempBuffer.makeCopyOf(buffer);
        
        for (int j = 0; j < bufferSize; j++)
        {
            updateAll(modVal[j]);
        
            freq = (chainSettings.freq + 1) * float(i + 1);
        
            if (freq < nyquist)
            {
//                float *lp = tempBuffer.getWritePointer(LEFT_CHANNEL, j);
//                float *rp = tempBuffer.getWritePointer(RIGHT_CHANNEL, j);
                leftSamp = tempBuffer.getSample(LEFT_CHANNEL, j);
                rightSamp = tempBuffer.getSample(RIGHT_CHANNEL, j);
                
                leftSamp = leftChain[i].get<BPChainPositions::BPFilter>().processSample(leftSamp);
                leftSamp = leftChain[i].get<BPChainPositions::CurveGain>().processSample(leftSamp);
                leftSamp = leftChain[i].get<BPChainPositions::OddEvenGain>().processSample(leftSamp);
                
                rightSamp = rightChain[i].get<BPChainPositions::BPFilter>().processSample(rightSamp);
                rightSamp = rightChain[i].get<BPChainPositions::CurveGain>().processSample(rightSamp);
                rightSamp = rightChain[i].get<BPChainPositions::OddEvenGain>().processSample(rightSamp);
                
                tempBuffer.setSample(LEFT_CHANNEL, j, leftSamp);
                tempBuffer.setSample(RIGHT_CHANNEL, j, rightSamp);
                
                /*
                 juce::dsp::AudioBlock<float> tempBlock(tempBuffer);
            
                 auto leftBlock = tempBlock.getSingleChannelBlock(LEFT_CHANNEL);
                 auto rightBlock = tempBlock.getSingleChannelBlock(RIGHT_CHANNEL);
            
                 juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
                 juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
            
                 leftChain[i].process(leftContext);
                 rightChain[i].process(rightContext);
                 */
            
                if (i == 0)
                    for (int chan = 0; chan < totalNumInputChannels; chan++)
                        effectBuffer.copyFrom(chan, 0, tempBuffer, chan, 0, bufferSize);
                else
                    for (int chan = 0; chan < totalNumInputChannels; chan++)
                        effectBuffer.addFrom(chan, 0, tempBuffer, chan, 0, bufferSize);
            }
        
            else
                break;
        }
    }
    
    
    /*
    for (int i = 0; i < chainSettings.quality; i++)
    {
        freq = (chainSettings.freq + 1) * float(i + 1);
        
        if (freq < nyquist)
        {
            tempBuffer.makeCopyOf(buffer);
            
            
            juce::dsp::AudioBlock<float> tempBlock(tempBuffer);
        
            auto leftBlock = tempBlock.getSingleChannelBlock(LEFT_CHANNEL);
            auto rightBlock = tempBlock.getSingleChannelBlock(RIGHT_CHANNEL);
        
            juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
            juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
        
            leftChain[i].process(leftContext);
            rightChain[i].process(rightContext);
             
        
            if (i == 0)
                for (int chan = 0; chan < totalNumInputChannels; chan++)
                    effectBuffer.copyFrom(chan, 0, tempBuffer, chan, 0, bufferSize);
            else
                for (int chan = 0; chan < totalNumInputChannels; chan++)
                    effectBuffer.addFrom(chan, 0, tempBuffer, chan, 0, bufferSize);
        }
    
        else
            break;
    }
     */
    
    buffer.makeCopyOf(effectBuffer);
}

void ThesisAudioProcessor::updateAll(float modVal)
{
    ChainSettings chainSettings = getChainSettings(apvts);
    
    float freq;
    
    float nyquist = float(getSampleRate()) / 2.f;
    
    for (int i = 0; i < NUM_HARM; i++)
    {
        freq = chainSettings.freq * float(i + 1);
        
        if (freq < nyquist)
        {
            updateFilter(chainSettings, modVal, i);
            updateCurveGain(chainSettings, i);
            updateOddEvenGain(chainSettings, i);
        }
        else
            break;
    }
}

void ThesisAudioProcessor::updateCoefficients(Coefficients &old, const Coefficients &replacements)
{
    *old = *replacements;
}

void ThesisAudioProcessor::updateFilter(const ChainSettings &chainSettings, float modVal, int i)
{
    float freq, q;
    
    freq = (chainSettings.freq + modVal) * float(i + 1);
    q = chainSettings.q * (float(i) / 2.f + 1);
//    q = chainSettings.q;
    
    auto bpCoefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(getSampleRate(),
                                                                            freq,
                                                                            q);
        
    updateCoefficients(leftChain[i].get<BPChainPositions::BPFilter>().coefficients, bpCoefficients);
    updateCoefficients(rightChain[i].get<BPChainPositions::BPFilter>().coefficients, bpCoefficients);
}

void ThesisAudioProcessor::updateCurveGain(const ChainSettings &chainSettings, int i)
{
    float gain = pow((-1.f / float(NUM_HARM)) * float(i) + 1.f, (1.f / chainSettings.curve));
    
    leftChain[i].get<BPChainPositions::CurveGain>().setGainLinear(gain);
    rightChain[i].get<BPChainPositions::CurveGain>().setGainLinear(gain);
}

void ThesisAudioProcessor::updateOddEvenGain(const ChainSettings &chainSettings, int i)
{
    if (i == 0)
    {
        leftChain[i].get<BPChainPositions::OddEvenGain>().setGainDecibels(chainSettings.fundGain);
        rightChain[i].get<BPChainPositions::OddEvenGain>().setGainDecibels(chainSettings.fundGain);
    }
    else if (i % 2 == 1)
    {
        leftChain[i].get<BPChainPositions::OddEvenGain>().setGainDecibels(chainSettings.oddGain);
        rightChain[i].get<BPChainPositions::OddEvenGain>().setGainDecibels(chainSettings.oddGain);
    }
    else if (i % 2 == 0)
    {
        leftChain[i].get<BPChainPositions::OddEvenGain>().setGainDecibels(chainSettings.evenGain);
        rightChain[i].get<BPChainPositions::OddEvenGain>().setGainDecibels(chainSettings.evenGain);
    }
}

float ThesisAudioProcessor::applyFilter(float samp, int i)
{
    float out = leftChain[i].get<BPChainPositions::BPFilter>().processSample(samp);
    
    return out;
}

//==============================================================================
bool ThesisAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ThesisAudioProcessor::createEditor()
{
//    return new ThesisAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void ThesisAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void ThesisAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if ( tree.isValid() )
    {
        apvts.replaceState(tree);
        updateAll(0.f);
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;
    
    settings.freq = apvts.getRawParameterValue("Center Frequency")->load();
    settings.q = apvts.getRawParameterValue("Q")->load();
    settings.curve = apvts.getRawParameterValue("Curve")->load();
    settings.oddGain = apvts.getRawParameterValue("Odd Gain")->load();
    settings.evenGain = apvts.getRawParameterValue("Even Gain")->load();
    settings.fundGain = apvts.getRawParameterValue("Fundamental Gain")->load();
    settings.quality = apvts.getRawParameterValue("Quality")->load();
    settings.noise = apvts.getRawParameterValue("Noise")->load();
    
    return settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout
    ThesisAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Center Frequency",
                                                           "Center Frequency",
                                                           juce::NormalisableRange<float>(20.f, 10000.f, 1.f, 0.25f),
                                                           100.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Q",
                                                           "Q",
                                                           juce::NormalisableRange<float>(1.f, 50.f, 0.1f, 1.f),
                                                           1.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Curve",
                                                           "Curve",
                                                           juce::NormalisableRange<float>(0.1f, 10.f, 0.1f, 0.25f),
                                                           1.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Odd Gain",
                                                           "Odd Gain",
                                                           juce::NormalisableRange<float>(-24.f, 6.f, 0.1f, 1.0f),
                                                           1.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Even Gain",
                                                           "Even Gain",
                                                           juce::NormalisableRange<float>(-24.f, 6.f, 0.1f, 1.0f),
                                                           1.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Fundamental Gain",
                                                           "Fundamental Gain",
                                                           juce::NormalisableRange<float>(-24.f, 6.f, 0.1f, 1.0f),
                                                           1.f));
    
    layout.add(std::make_unique<juce::AudioParameterInt>("Quality",
                                                         "Quality",
                                                         50,
                                                         200,
                                                         100));
    
    layout.add(std::make_unique<juce::AudioParameterBool>("Noise",
                                                          "Noise",
                                                          false));
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThesisAudioProcessor();
}
