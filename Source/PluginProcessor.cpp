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
    mod.initMod(sampleRate);
    mod.setMod(1.f);
    
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    
    for (int i = 0; i < NUM_HARM; i++)
    {
        leftChain[i].prepare(spec);
        leftChain[i].get<BPChainPositions::BPFilter>().setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        rightChain[i].prepare(spec);
        rightChain[i].get<BPChainPositions::BPFilter>().setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    }
    
    updateAll();
    
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
    // initialize variables
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto chainSettings = getChainSettings(apvts);
    int bufferSize = buffer.getNumSamples();
    int numHarm = int(50.f * pow(2.f, float(chainSettings.quality)));
    bool modState;
    float freq, modVal[bufferSize], nyquist, modDepth, detune;
    
    nyquist = float(getSampleRate()) / 2.f;
    detune = chainSettings.detune;
    modDepth = chainSettings.modDepth / 100.f;
    modState = chainSettings.modDetune || chainSettings.modFreq || chainSettings.modTimbre;
    mod.updateMod(chainSettings.modRate);
    
    // Fill modulator array with values
    for (int i = 0; i < bufferSize; i++)
        modVal[i] = mod.modBlock(bufferSize)[i] * modDepth;
    
    // update dsp based on parameters
    updateAll();
    
    // copy contents of input buffer and create a temporary buffer to fill
    juce::AudioBuffer<float> effectBuffer;
    effectBuffer.makeCopyOf(buffer);
    
    juce::AudioBuffer<float> tempBuffer;
    
    //for loop that iterates over how many BP filters will be made
    for (int harm = 0; harm < numHarm; harm++)
    {
        tempBuffer.makeCopyOf(buffer);
        freq = chainSettings.freq * pow(float(harm + 1), detune);
        
        if (freq < nyquist)
        {
            if (modState)
                processWithMod(tempBuffer, chainSettings, modVal, harm);
            else
                processNoMod(tempBuffer, harm);
            
            // fill effectBuffer with each harmonic tempBuffer
            if (harm == 0)
                for (int chan = 0; chan < totalNumInputChannels; chan++)
                    effectBuffer.copyFrom(chan, 0, tempBuffer, chan, 0, bufferSize);
            else
                for (int chan = 0; chan < totalNumInputChannels; chan++)
                    effectBuffer.addFrom(chan, 0, tempBuffer, chan, 0, bufferSize);
        }
        else
            break;
    }
    
    buffer.makeCopyOf(effectBuffer);
}

void ThesisAudioProcessor::processNoMod(juce::AudioBuffer<float> &buffer, int harm)
{
    juce::dsp::AudioBlock<float> tempBlock(buffer);
                
    auto leftBlock = tempBlock.getSingleChannelBlock(LEFT_CHANNEL);
    auto rightBlock = tempBlock.getSingleChannelBlock(RIGHT_CHANNEL);
                
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
                
    leftChain[harm].process(leftContext);
    rightChain[harm].process(rightContext);
}

void ThesisAudioProcessor::processWithMod(juce::AudioBuffer<float> &buffer, const ChainSettings &chainSettings, float* modVal, int harm)
{
    int bufferSize = buffer.getNumSamples();
    
    float leftSamp, rightSamp;
    
    bool filterCheck = true;
    
    //iterate through each sample in buffer to fill harmonic tempBuffer
    for (int n = 0; n < bufferSize; n++)
    {
        //update BP filter's frequency based on modulator
        filterCheck = updateSVFilter(chainSettings, modVal[n], harm);

        leftSamp = buffer.getSample(LEFT_CHANNEL, n);
        rightSamp = buffer.getSample(RIGHT_CHANNEL, n);
        
        // 1.) apply band-pass filter to sample
        leftSamp = leftChain[harm].get<BPChainPositions::BPFilter>().processSample(LEFT_CHANNEL, leftSamp);
        rightSamp = rightChain[harm].get<BPChainPositions::BPFilter>().processSample(RIGHT_CHANNEL, rightSamp);
        
        // 2.) apply curve gain
        leftSamp = leftChain[harm].get<BPChainPositions::CurveGain>().processSample(leftSamp);
        rightSamp = rightChain[harm].get<BPChainPositions::CurveGain>().processSample(rightSamp);
        
        // 3.) apply odd/even harmonic gain
        leftSamp = leftChain[harm].get<BPChainPositions::OddEvenGain>().processSample(leftSamp);
        rightSamp = rightChain[harm].get<BPChainPositions::OddEvenGain>().processSample(rightSamp);
        
        // 4.) rewrite sample to tempBuffer
        buffer.setSample(LEFT_CHANNEL, n, leftSamp);
        buffer.setSample(RIGHT_CHANNEL, n, rightSamp);
        
        // 5.) silence sample if the frequency is outside of bounds
        if (!filterCheck)
            buffer.applyGain(n, 1, 0.f);
    }
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
            updateSVFilter(chainSettings, modVal, i);
            updateCurveGain(chainSettings, i);
            updateOddEvenGain(chainSettings, i);
        }
        else
            break;
    }
}

bool ThesisAudioProcessor::updateSVFilter(const ChainSettings &chainSettings, float modVal, int i)
{
    float freq, q, nyquist, detune;
    
    if (i > 0)
    {
        detune = chainSettings.detune;
        
        if (chainSettings.modDetune)
            detune += (modVal / 200.f);
    }
    else
        detune = 1;
    
    detune = detune == 0 ? detune = 0.1 : detune;
    
    freq = (modVal + 1.f) * (chainSettings.freq) * pow(float(i + 1), detune);
    q = chainSettings.q * (float(i) / 2.f + 1);
    
    freq = wrap(freq, getSampleRate());
    
    nyquist = getSampleRate() / 2.f;
    
    if (freq < 20.f || freq >= nyquist)
        return false;
    
    //LEFT CHANNEL
    leftChain[i].get<BPChainPositions::BPFilter>().setCutoffFrequency(freq);
    leftChain[i].get<BPChainPositions::BPFilter>().setResonance(q);
    
    //RIGHT CHANNEL
    rightChain[i].get<BPChainPositions::BPFilter>().setCutoffFrequency(freq);
    rightChain[i].get<BPChainPositions::BPFilter>().setResonance(q);
    
    return true;
}

void ThesisAudioProcessor::updateCurveGain(const ChainSettings &chainSettings, int i)
{
    float gain, normGain, q;
    
    q = chainSettings.q * (float(i) / 2.f + 1);
    
    normGain = pow((1.f/q), 0.8);
    
    gain = pow((-1.f / float(NUM_HARM)) * float(i) + 1.f, (1.f / chainSettings.curve));
    
    gain *= normGain;
    
    leftChain[i].get<BPChainPositions::CurveGain>().setGainLinear(gain);
    rightChain[i].get<BPChainPositions::CurveGain>().setGainLinear(gain);
}

void ThesisAudioProcessor::updateOddEvenGain(const ChainSettings &chainSettings, int i)
{
    float oddGain = chainSettings.timbre * -1.f + 1.f;
    float evenGain = chainSettings.timbre;
    if (i == 0)
    {
        leftChain[i].get<BPChainPositions::OddEvenGain>().setGainLinear(1.f);
        rightChain[i].get<BPChainPositions::OddEvenGain>().setGainLinear(1.f);
    }
    else if (i % 2 == 1)
    {
        leftChain[i].get<BPChainPositions::OddEvenGain>().setGainLinear(oddGain);
        rightChain[i].get<BPChainPositions::OddEvenGain>().setGainLinear(oddGain);
    }
    else if (i % 2 == 0)
    {
        leftChain[i].get<BPChainPositions::OddEvenGain>().setGainLinear(evenGain);
        rightChain[i].get<BPChainPositions::OddEvenGain>().setGainLinear(evenGain);
    }
}

float ThesisAudioProcessor::wrap(float x, int sampleRate)
{
    float y;
    
    if (x > sampleRate / 2)
        y = x - int(ceil( x )) % sampleRate;
    
    else if (x < 20)
        y = 20 - x + 20;
    
    else
        y = x;
        
    return y;
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
        updateAll();
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;
    
    //==============================================================================
    settings.timbre = apvts.getRawParameterValue("Timbre")->load();
    settings.freq = apvts.getRawParameterValue("Center Frequency")->load();
    settings.q = apvts.getRawParameterValue("Q")->load();
    
    //==============================================================================
    settings.stereoLink = apvts.getRawParameterValue("Stereo Link")->load();
    settings.quality = apvts.getRawParameterValue("Quality")->load();
    settings.curve = apvts.getRawParameterValue("Curve")->load();
    settings.detune = apvts.getRawParameterValue("Detune")->load();
    
    //==============================================================================
    settings.modTimbre = apvts.getRawParameterValue("Mod Timbre")->load();
    settings.modFreq = apvts.getRawParameterValue("Mod Freq")->load();
    settings.modDetune = apvts.getRawParameterValue("Mod Detune")->load();
    settings.modDepth = apvts.getRawParameterValue("Mod Depth")->load();
    settings.modRate = apvts.getRawParameterValue("Mod Rate")->load();
    
    return settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout
    ThesisAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    //==============================================================================
    layout.add(std::make_unique<juce::AudioParameterFloat>("Timbre",
                                                           "Timbre",
                                                           juce::NormalisableRange<float>(0.f, 1.f, 0.05f, 1.f),
                                                           0.5f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Center Frequency",
                                                           "Center Frequency",
                                                           juce::NormalisableRange<float>(20.f, 10000.f, 1.f, 0.25f),
                                                           100.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Q",
                                                           "Q",
                                                           juce::NormalisableRange<float>(1.f, 100.f, 0.1f, 1.f),
                                                           20.f));
        
    layout.add(std::make_unique<juce::AudioParameterFloat>("Detune",
                                                           "Detune",
                                                           juce::NormalisableRange<float>(0.5f, 2.f, 0.01f, 1.f),
                                                           1.f));
        
    //==============================================================================
    layout.add(std::make_unique<juce::AudioParameterFloat>("Stereo Link",
                                                           "Stereo Link",
                                                           juce::NormalisableRange<float>(0.f, 1.f, 0.05f, 1.f),
                                                           1.f));
        
    layout.add(std::make_unique<juce::AudioParameterInt>("Quality",
                                                         "Quality",
                                                         0, 2, 1));
        
    layout.add(std::make_unique<juce::AudioParameterFloat>("Curve",
                                                           "Curve",
                                                           juce::NormalisableRange<float>(0.1f, 10.f, 0.1f, 0.25f),
                                                           1.f));
    
        
    //==============================================================================
    layout.add(std::make_unique<juce::AudioParameterBool>("Mod Timbre", "Mod Timbre", false));
        
    layout.add(std::make_unique<juce::AudioParameterBool>("Mod Freq", "Mod Freq", false));
        
    layout.add(std::make_unique<juce::AudioParameterBool>("Mod Detune", "Mod Detune", false));
        
    layout.add(std::make_unique<juce::AudioParameterFloat>("Mod Depth",
                                                           "Mod Depth",
                                                           juce::NormalisableRange<float>(0.f, 100.f, 0.1f, 1.f),
                                                           1.f));
        
    layout.add(std::make_unique<juce::AudioParameterFloat>("Mod Rate",
                                                           "Mod Rate",
                                                           juce::NormalisableRange<float>(0.1f, 20.f, 0.1f, 1.f),
                                                           1.f));
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThesisAudioProcessor();
}
