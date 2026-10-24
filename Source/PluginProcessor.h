#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "JellyTuneDSP.h"

class JellyTuneProcessor : public juce::AudioProcessor
{
public:
    JellyTuneProcessor();
    ~JellyTuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
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

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    jelly::JellyTuneDSP dsp;

    std::atomic<float>* retuneParam   = nullptr; // ms
    std::atomic<float>* keyParam      = nullptr; // 0..6 -> C D E F G A B
    std::atomic<float>* sharpParam    = nullptr; // 0/1
    std::atomic<float>* scaleParam    = nullptr; // 0 cromatica, 1 mayor, 2 menor
    std::atomic<float>* vibratoParam  = nullptr; // 0/1
    std::atomic<float>* vibRateParam  = nullptr; // Hz
    std::atomic<float>* vibDepthParam = nullptr; // cents

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JellyTuneProcessor)
};
