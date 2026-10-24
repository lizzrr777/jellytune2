#include "PluginProcessor.h"
#include "PluginEditor.h"

JellyTuneProcessor::JellyTuneProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    retuneParam   = apvts.getRawParameterValue ("retune");
    keyParam      = apvts.getRawParameterValue ("key");
    sharpParam    = apvts.getRawParameterValue ("sharp");
    scaleParam    = apvts.getRawParameterValue ("scale");
    vibratoParam  = apvts.getRawParameterValue ("vibrato");
    vibRateParam  = apvts.getRawParameterValue ("vibRate");
    vibDepthParam = apvts.getRawParameterValue ("vibDepth");
}

juce::AudioProcessorValueTreeState::ParameterLayout JellyTuneProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    using A = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // 0 ms = correccion instantanea (efecto robotico), 400 ms = muy natural
    layout.add (std::make_unique<P> (juce::ParameterID { "retune", 1 }, "Retune Speed",
                                     R (0.0f, 400.0f, 0.1f, 0.5f), 25.0f, A().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "key", 1 }, "Key",
                                     juce::StringArray { "C", "D", "E", "F", "G", "A", "B" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "sharp", 1 }, "Sharp", false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "scale", 1 }, "Scale",
                                     juce::StringArray { "Chromatic", "Major", "Minor" }, 1));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "vibrato", 1 }, "Vibrato", false));

    layout.add (std::make_unique<P> (juce::ParameterID { "vibRate", 1 }, "Vibrato Rate",
                                     R (0.5f, 12.0f, 0.1f), 5.5f, A().withLabel ("Hz")));

    layout.add (std::make_unique<P> (juce::ParameterID { "vibDepth", 1 }, "Vibrato Depth",
                                     R (0.0f, 100.0f, 1.0f), 35.0f, A().withLabel ("cents")));

    return layout;
}

void JellyTuneProcessor::prepareToPlay (double sampleRate, int)
{
    dsp.prepare (sampleRate, juce::jmax (1, getTotalNumInputChannels()));
    setLatencySamples (dsp.getLatencySamples());
}

bool JellyTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void JellyTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    static const int naturalPc[7] = { 0, 2, 4, 5, 7, 9, 11 }; // C D E F G A B
    const int keyIndex = juce::jlimit (0, 6, (int) std::lround (keyParam->load()));

    jelly::JellyTuneDSP::Settings s;
    s.retuneMs      = retuneParam->load();
    s.rootPc        = (naturalPc[keyIndex] + (sharpParam->load() > 0.5f ? 1 : 0)) % 12;
    s.scale         = juce::jlimit (0, 2, (int) std::lround (scaleParam->load()));
    s.vibrato       = vibratoParam->load() > 0.5f;
    s.vibRateHz     = vibRateParam->load();
    s.vibDepthCents = vibDepthParam->load();
    dsp.setSettings (s);

    dsp.process (buffer.getArrayOfWritePointers(), juce::jmin (numIn, 2), numSamples);
}

juce::AudioProcessorEditor* JellyTuneProcessor::createEditor()
{
    return new JellyTuneEditor (*this);
}

void JellyTuneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void JellyTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JellyTuneProcessor();
}
