#pragma once
#include "PluginProcessor.h"

//==============================================================================
// Estilo visual: negro predominante + verde del logo
class JellyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static inline const juce::Colour bg       { 0xff08080a };
    static inline const juce::Colour panel    { 0xff111114 };
    static inline const juce::Colour outline  { 0xff25252b };
    static inline const juce::Colour accent   { 0xff5fbf30 };
    static inline const juce::Colour dimText  { 0xff8a8a94 };

    JellyLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

//==============================================================================
class JellyTuneEditor : public juce::AudioProcessorEditor
{
public:
    explicit JellyTuneEditor (JellyTuneProcessor&);
    ~JellyTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name, const juce::String& suffix);
    void setupCaption (juce::Label&, const juce::String& text);
    static void placeKnob (Knob&, juce::Rectangle<int> area);

    JellyLookAndFeel lnf;           // debe declararse antes que los componentes
    JellyTuneProcessor& proc;
    juce::Image logo;

    Knob retune, vibRate, vibDepth;
    juce::ComboBox keyBox, scaleBox;
    juce::TextButton sharpBtn { "SHARP  #" }, vibBtn { "VIBRATO" };
    juce::Label keyLabel, scaleLabel;

    juce::Rectangle<int> keyPanel, retunePanel, vibPanel;

    // los attachments se destruyen primero (declarados al final)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sharpAtt, vibAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JellyTuneEditor)
};
