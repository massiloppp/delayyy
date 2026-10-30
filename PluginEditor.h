#pragma once
#include "PluginProcessor.h"

namespace Col
{
    const juce::Colour bg      { 0xffaaaaaa };
    const juce::Colour title   { 0xffaaaaaa };
    const juce::Colour display { 0xff1b181b };
    const juce::Colour orange  { 0xffffb204 };
    const juce::Colour button  { 0xffbdbfc1 };
    const juce::Colour cyan    { 0xff35d4f5 };
    const juce::Colour text    { 0xff1a1a1a };
    const juce::Colour grid    { 0xff3a363a };
}

// Base for any control bound to one plugin parameter.
class ParamControl : public juce::Component
{
public:
    explicit ParamControl (juce::RangedAudioParameter& p)
        : param (p), att (p, [this] (float) { repaint(); }) { att.sendInitialUpdate(); }

    float real() const { return param.convertFrom0to1 (param.getValue()); }
    juce::RangedAudioParameter& param;
protected:
    juce::ParameterAttachment att;
};

class ToggleBtn : public ParamControl
{
public:
    ToggleBtn (juce::RangedAudioParameter& p, juce::String l) : ParamControl (p), label (std::move (l)) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { att.setValueAsCompleteGesture (param.getValue() > 0.5f ? 0.f : 1.f); }
    juce::String label;
    bool symbolInfinity = false, symbolLink = false, squareStyle = false;
};

class ChoiceBtn : public ParamControl
{
public:
    ChoiceBtn (juce::RangedAudioParameter& p, int idx, juce::String l) : ParamControl (p), index (idx), label (std::move (l)) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { att.setValueAsCompleteGesture ((float) index); }
    int index; juce::String label;
};

class DragValue : public ParamControl
{
public:
    enum Style { Field, Orange, Knob };
    DragValue (juce::RangedAudioParameter& p, Style s, std::function<juce::String (float)> fmt)
        : ParamControl (p), style (s), format (std::move (fmt)) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { att.beginGesture(); startNorm = param.getValue(); }
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override
    { att.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); }
    Style style;
    std::function<juce::String (float)> format;
private:
    float startNorm = 0.f;
};

class FilterDisplay : public juce::Component
{
public:
    FilterDisplay (DelayAudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { fAtt.endGesture(); wAtt.endGesture(); }
private:
    juce::Point<float> handlePos() const;
    static float xFromFreq (float f, float w) { return w * std::log (f / 20.f) / std::log (1000.f); }
    DelayAudioProcessor& proc;
    juce::RangedAudioParameter *fP, *wP, *onP;
    juce::ParameterAttachment fAtt, wAtt, oAtt;
};

class DelayEditor : public juce::AudioProcessorEditor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit DelayEditor (DelayAudioProcessor&);
    ~DelayEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged (const juce::String& id, float) override;
    void updateVisibility();
    juce::RangedAudioParameter& P (const char* id) { return *proc.apvts.getParameter (id); }

    DelayAudioProcessor& proc;
    std::vector<std::unique_ptr<juce::Component>> owned;
    template <typename T, typename... A> T* add (A&&... a)
    { auto c = std::make_unique<T> (std::forward<A> (a)...); auto* raw = c.get(); addAndMakeVisible (*c); owned.push_back (std::move (c)); return raw; }

    ToggleBtn *lSync, *rSync, *link, *freeze, *filterOn, *pingpong;
    std::vector<ChoiceBtn*> lDivs, rDivs, modes;
    DragValue *lOff, *rOff, *lTime, *rTime, *fFreq, *fWidth, *mRate, *mFilter, *mTime, *feedback, *dryWet;
    FilterDisplay* display;
    juce::ParameterAttachment lSyncWatch, rSyncWatch;
    bool mirroring = false;
    void mirror (const juce::String& id, float value);
};
