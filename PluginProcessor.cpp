#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::array<int, 8> DelayAudioProcessor::divisions { 1, 2, 3, 4, 5, 6, 8, 16 };

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

static juce::String pctText (float v, int) { return juce::String (v, 1) + " %"; }

Layout DelayAudioProcessor::createLayout()
{
    using namespace juce;
    Layout l;
    auto pid = [] (const char* s) { return ParameterID { s, 1 }; };

    StringArray divNames; for (int d : divisions) divNames.add (String (d));
    auto msRange = NormalisableRange<float> (1.f, 5000.f, 0.01f, 0.35f);
    auto offRange = NormalisableRange<float> (-33.f, 33.f, 0.1f);

    l.add (std::make_unique<AudioParameterBool> (pid (IDs::lSync), "Left Sync", true));
    l.add (std::make_unique<AudioParameterBool> (pid (IDs::rSync), "Right Sync", true));
    l.add (std::make_unique<AudioParameterChoice> (pid (IDs::lDiv), "Left Division", divNames, 3));
    l.add (std::make_unique<AudioParameterChoice> (pid (IDs::rDiv), "Right Division", divNames, 3));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::lTime), "Left Time", msRange, 250.f,
           AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::rTime), "Right Time", msRange, 250.f,
           AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::lOff), "Left Offset", offRange, 0.f,
           AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::rOff), "Right Offset", offRange, 0.f,
           AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    l.add (std::make_unique<AudioParameterBool> (pid (IDs::link), "Link", true));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::feedback), "Feedback",
           NormalisableRange<float> (0.f, 100.f, 0.1f), 50.f,
           AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    l.add (std::make_unique<AudioParameterBool> (pid (IDs::freeze), "Freeze", false));
    l.add (std::make_unique<AudioParameterBool> (pid (IDs::fOn), "Filter On", true));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::fFreq), "Filter Freq",
           NormalisableRange<float> (50.f, 18000.f, 0.f, 0.3f), 1000.f,
           AudioParameterFloatAttributes().withLabel ("Hz")));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::fWidth), "Filter Width",
           NormalisableRange<float> (0.3f, 18.f, 0.f, 0.4f), 2.5f));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::mRate), "Mod Rate",
           NormalisableRange<float> (0.01f, 40.f, 0.f, 0.35f), 1.f,
           AudioParameterFloatAttributes().withLabel ("Hz")));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::mFilter), "Mod Filter",
           NormalisableRange<float> (0.f, 100.f, 1.f), 0.f,
           AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::mTime), "Mod Time",
           NormalisableRange<float> (0.f, 100.f, 1.f), 0.f,
           AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterChoice> (pid (IDs::mode), "Mode",
           StringArray { "Repitch", "Fade", "Jump" }, 1));
    l.add (std::make_unique<AudioParameterBool> (pid (IDs::pingpong), "Ping Pong", false));
    l.add (std::make_unique<AudioParameterFloat> (pid (IDs::dryWet), "Dry/Wet",
           NormalisableRange<float> (0.f, 100.f, 0.1f), 50.f,
           AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    return l;
}

DelayAudioProcessor::DelayAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool DelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo()) && (l.getMainInputChannelSet() == out
            || l.getMainInputChannelSet() == juce::AudioChannelSet::mono());
}

void DelayAudioProcessor::prepareToPlay (double rate, int)
{
    sr = rate;
    const int size = (int) std::ceil (sr * 12.0) + 16;
    for (auto& c : ch) c.reset (size);
    lfoPhase = 0.0;
    smFeedback.reset (sr, 0.03);
    smDryWet.reset (sr, 0.03);
    smFeedback.setCurrentAndTargetValue (*apvts.getRawParameterValue (IDs::feedback) * 0.01f);
    smDryWet.setCurrentAndTargetValue (*apvts.getRawParameterValue (IDs::dryWet) * 0.01f);
}

// 4-point Hermite interpolation
float DelayAudioProcessor::readTap (const Channel& c, double d) const noexcept
{
    const int n = (int) c.buf.size();
    d = juce::jlimit (3.0, (double) (n - 8), d);
    const double pos = (double) c.w - d;
    int i = (int) std::floor (pos);
    const float f = (float) (pos - i);
    auto at = [&] (int k) { k %= n; if (k < 0) k += n; return c.buf[(size_t) k]; };
    const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
    const float c0 = y1, c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
}

float DelayAudioProcessor::tap (Channel& c, double target, int mode, double mod) noexcept
{
    if (! c.init) { c.dSmooth = c.d0 = c.d1 = target; c.fade = 1.0; c.init = true; }

    if (mode == 0)   // Repitch: delay time glides, pitch changes while it moves
    {
        c.dSmooth += (target - c.dSmooth) * (1.0 - std::exp (-1.0 / (0.12 * sr)));
        return readTap (c, c.dSmooth + mod);
    }
    if (mode == 2)   // Jump: the new time is used immediately
    {
        c.dSmooth = target;
        return readTap (c, target + mod);
    }
    // Fade: crossfade between old and new tap
    if (std::abs (target - c.d1) > 0.5)
    {
        c.d0 = (c.fade > 0.5) ? c.d1 : c.d0;
        c.d1 = target;
        c.fade = 0.0;
    }
    if (c.fade < 1.0)
    {
        c.fade = std::min (1.0, c.fade + 1.0 / (0.06 * sr));
        const float a = (float) c.fade;
        const float s0 = readTap (c, c.d0 + mod), s1 = readTap (c, c.d1 + mod);
        const float g0 = std::cos (a * juce::MathConstants<float>::halfPi);
        const float g1 = std::sin (a * juce::MathConstants<float>::halfPi);
        if (c.fade >= 1.0) c.d0 = c.d1;
        return s0 * g0 + s1 * g1;
    }
    return readTap (c, c.d1 + mod);
}

void DelayAudioProcessor::setBandpass (Channel& c, double f, double q) noexcept
{
    f = juce::jlimit (20.0, sr * 0.45, f);
    const double w0 = juce::MathConstants<double>::twoPi * f / sr;
    const double alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    c.b0 = (float) (alpha / a0);  c.b1 = 0.f;  c.b2 = (float) (-alpha / a0);
    c.a1 = (float) (-2.0 * std::cos (w0) / a0);  c.a2 = (float) ((1.0 - alpha) / a0);
}

void DelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (nCh < 1) return;

    auto get = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);

    const bool link = get (IDs::link) > 0.5f;
    auto sideTime = [&] (bool right) -> double
    {
        const bool useR = right && ! link;           // linked: both sides follow the left controls
        const bool sync = get (useR ? IDs::rSync : IDs::lSync) > 0.5f;
        const float off = get (right ? IDs::rOff : IDs::lOff);
        double ms;
        if (sync)
        {
            const int idx = (int) get (useR ? IDs::rDiv : IDs::lDiv);
            ms = divisions[(size_t) juce::jlimit (0, 7, idx)] * (60000.0 / bpm) * 0.25;
        }
        else ms = get (useR ? IDs::rTime : IDs::lTime);
        ms *= 1.0 + (double) off * 0.01;
        return ms * 0.001 * sr;
    };
    const double tL = sideTime (false), tR = sideTime (true);

    const bool freeze = get (IDs::freeze) > 0.5f;
    const bool fOn = get (IDs::fOn) > 0.5f;
    const double fFreq = get (IDs::fFreq), fWidth = get (IDs::fWidth);
    const double mRate = get (IDs::mRate);
    const float mFil = get (IDs::mFilter) * 0.01f, mTim = get (IDs::mTime) * 0.01f;
    const int mode = (int) get (IDs::mode);
    const bool pp = get (IDs::pingpong) > 0.5f;
    smFeedback.setTargetValue (freeze ? 1.f : get (IDs::feedback) * 0.01f);
    smDryWet.setTargetValue (get (IDs::dryWet) * 0.01f);

    float* d0 = buffer.getWritePointer (0);
    float* d1 = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const double lfoInc = juce::MathConstants<double>::twoPi * mRate / sr;

    for (int s = 0; s < n; ++s)
    {
        const float inL = d0[s], inR = d1 ? d1[s] : inL;
        const float fb = smFeedback.getNextValue(), wet = smDryWet.getNextValue();

        const double lfo = std::sin (lfoPhase);
        lfoPhase += lfoInc; if (lfoPhase > juce::MathConstants<double>::twoPi) lfoPhase -= juce::MathConstants<double>::twoPi;

        if ((s & 15) == 0 && fOn)
        {
            const double f = fFreq * std::pow (2.0, lfo * mFil * 2.0);
            setBandpass (ch[0], f, fWidth);
            ch[1].b0 = ch[0].b0; ch[1].b1 = ch[0].b1; ch[1].b2 = ch[0].b2; ch[1].a1 = ch[0].a1; ch[1].a2 = ch[0].a2;
        }

        auto modFor = [&] (double target) { return lfo * mTim * std::min (0.012 * sr, 0.5 * target); };
        const float wL = tap (ch[0], tL, mode, modFor (tL));
        const float wR = tap (ch[1], tR, mode, modFor (tR));

        const float inGain = freeze ? 0.f : 1.f;
        float wrL, wrR;
        if (pp)
        {
            wrL = inGain * 0.5f * (inL + inR) + fb * wR;
            wrR = fb * wL;
        }
        else
        {
            wrL = inGain * inL + fb * wL;
            wrR = inGain * inR + fb * wR;
        }

        if (fOn)
        {
            auto bq = [] (Channel& c, float x)
            {
                const float y = c.b0 * x + c.z1;
                c.z1 = c.b1 * x - c.a1 * y + c.z2;
                c.z2 = c.b2 * x - c.a2 * y;
                return y;
            };
            wrL = bq (ch[0], wrL);  wrR = bq (ch[1], wrR);
        }
        wrL = juce::jlimit (-8.f, 8.f, wrL);  wrR = juce::jlimit (-8.f, 8.f, wrR);

        ch[0].buf[(size_t) ch[0].w] = wrL;  ch[1].buf[(size_t) ch[1].w] = wrR;
        for (auto& c : ch) if (++c.w >= (int) c.buf.size()) c.w = 0;

        d0[s] = inL * (1.f - wet) + wL * wet;
        if (d1) d1[s] = inR * (1.f - wet) + wR * wet;
    }
}

juce::AudioProcessorEditor* DelayAudioProcessor::createEditor() { return new DelayEditor (*this); }

void DelayAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void DelayAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DelayAudioProcessor(); }
