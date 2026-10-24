#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
JellyLookAndFeel::JellyLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);

    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::Label::textColourId, juce::Colours::white);

    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b1b20));
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd0d0d6));
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);

    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1b1b20));
    setColour (juce::ComboBox::textColourId, juce::Colours::white);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, accent);

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff111114));
    setColour (juce::PopupMenu::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
}

void JellyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (8.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const float trackW = 5.0f;
    const float arcR = radius - trackW * 0.5f;

    juce::Path track;
    track.addCentredArc (cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff26262d));
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float bodyR = arcR - trackW - 4.0f;
    g.setColour (juce::Colour (0xff17171b));
    g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour (0xff2e2e36));
    g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.5f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-2.0f, -bodyR + 5.0f, 4.0f, bodyR * 0.42f, 2.0f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
    g.setColour (juce::Colours::white);
    g.fillPath (pointer);
}

//==============================================================================
JellyTuneEditor::JellyTuneEditor (JellyTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    setupKnob (retune,  "retune",   "RETUNE SPEED", " ms");
    setupKnob (vibRate, "vibRate",  "RATE",         " Hz");
    setupKnob (vibDepth, "vibDepth", "DEPTH",       " ct");

    keyBox.addItemList ({ "C", "D", "E", "F", "G", "A", "B" }, 1);
    keyBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (keyBox);

    scaleBox.addItemList ({ "Chromatic", "Major", "Minor" }, 1);
    scaleBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (scaleBox);

    sharpBtn.setClickingTogglesState (true);
    addAndMakeVisible (sharpBtn);

    vibBtn.setClickingTogglesState (true);
    addAndMakeVisible (vibBtn);

    setupCaption (keyLabel, "KEY");
    setupCaption (scaleLabel, "SCALE");

    keyAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "scale", scaleBox);
    sharpAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "sharp", sharpBtn);
    vibAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "vibrato", vibBtn);

    setSize (600, 430);
}

JellyTuneEditor::~JellyTuneEditor()
{
    setLookAndFeel (nullptr);
}

void JellyTuneEditor::setupKnob (Knob& k, const juce::String& paramId,
                                 const juce::String& name, const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                  juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 22);
    k.slider.setTextValueSuffix (suffix);
    addAndMakeVisible (k.slider);

    setupCaption (k.label, name);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, paramId, k.slider);
}

void JellyTuneEditor::setupCaption (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, JellyLookAndFeel::dimText);
    l.setJustificationType (juce::Justification::centred);
    l.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (l);
}

void JellyTuneEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (22));
    k.slider.setBounds (area);
}

//==============================================================================
void JellyTuneEditor::paint (juce::Graphics& g)
{
    g.fillAll (JellyLookAndFeel::bg);

    // cabecera: logo + nombre
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (logo, juce::Rectangle<float> (22.0f, 12.0f, 64.0f, 64.0f),
                 juce::RectanglePlacement::centred);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    g.drawText ("JELLYTUNE", 100, 14, 320, 38, juce::Justification::centredLeft);

    g.setColour (JellyLookAndFeel::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("VOCAL PITCH CORRECTION", 102, 50, 320, 18, juce::Justification::centredLeft);

    g.setColour (JellyLookAndFeel::accent);
    g.fillRect (0, 88, getWidth(), 2);

    // paneles
    for (auto r : { keyPanel, retunePanel, vibPanel })
    {
        g.setColour (JellyLookAndFeel::panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (JellyLookAndFeel::outline);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }
}

void JellyTuneEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (90);
    area = area.reduced (18, 14);

    keyPanel = area.removeFromTop (64);
    area.removeFromTop (12);

    retunePanel = area.removeFromLeft (area.getWidth() * 45 / 100);
    area.removeFromLeft (12);
    vibPanel = area;

    // fila: tonalidad + sharp + escala
    {
        auto r = keyPanel.reduced (14, 15);
        keyLabel.setBounds (r.removeFromLeft (40));
        keyBox.setBounds (r.removeFromLeft (80));
        r.removeFromLeft (14);
        sharpBtn.setBounds (r.removeFromLeft (110));
        r.removeFromLeft (34);
        scaleLabel.setBounds (r.removeFromLeft (56));
        scaleBox.setBounds (r.removeFromLeft (150));
    }

    placeKnob (retune, retunePanel.reduced (10));

    {
        auto r = vibPanel.reduced (14, 12);
        vibBtn.setBounds (r.removeFromTop (34));
        r.removeFromTop (8);
        const int half = r.getWidth() / 2;
        placeKnob (vibRate, r.removeFromLeft (half));
        placeKnob (vibDepth, r);
    }
}
