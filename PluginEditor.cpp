#include "PluginEditor.h"

using juce::Graphics; using juce::Rectangle;

static juce::Font F (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h).withStyle (bold ? "Bold" : "Regular"));
}

// ---------------------------------------------------------------- buttons
static void drawButton (Graphics& g, Rectangle<float> r, bool on, const juce::String& text, float fs = 17.f)
{
    g.setColour (on ? Col::orange : Col::button);
    g.fillRect (r);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRect (r, 1.f);
    g.setColour (Col::text);
    g.setFont (F (fs));
    g.drawText (text, r.toNearestInt(), juce::Justification::centred);
}

void ToggleBtn::paint (Graphics& g)
{
    const bool on = param.getValue() > 0.5f;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (squareStyle)    // small orange on/off square next to "Filter"
    {
        g.setColour (on ? Col::orange : Col::grid);
        g.fillRect (r.reduced (r.getWidth() * 0.05f));
        return;
    }
    drawButton (g, r, on, symbolInfinity || symbolLink ? juce::String() : label);
    if (symbolInfinity)
    {
        g.setColour (Col::text); g.setFont (F (26.f, true));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")), getLocalBounds(), juce::Justification::centred);
    }
    if (symbolLink)
    {
        g.setColour (Col::text);
        auto c = r.getCentre();
        g.drawRect (Rectangle<float> (c.x - 11, c.y - 4, 8, 8), 1.5f);
        g.drawRect (Rectangle<float> (c.x + 3, c.y - 4, 8, 8), 1.5f);
        g.drawLine (c.x - 3, c.y, c.x + 3, c.y, 1.5f);
    }
}

void ChoiceBtn::paint (Graphics& g)
{
    const int cur = juce::roundToInt (real());
    drawButton (g, getLocalBounds().toFloat().reduced (0.5f), cur == index, label);
}

// ---------------------------------------------------------------- drag values
void DragValue::mouseDrag (const juce::MouseEvent& e)
{
    const float sens = e.mods.isShiftDown() ? 1000.f : (style == Knob ? 160.f : 220.f);
    const float n = juce::jlimit (0.f, 1.f, startNorm - (float) e.getDistanceFromDragStartY() / sens);
    att.setValueAsPartOfGesture (param.convertFrom0to1 (n));
}

void DragValue::paint (Graphics& g)
{
    const auto txt = format (real());
    auto b = getLocalBounds();
    if (style == Field)
    {
        auto r = b.toFloat().reduced (0.5f);
        g.setColour (Col::button); g.fillRect (r);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawRect (r, 1.f);
        g.setColour (Col::text); g.setFont (F (17.f));
        g.drawText (txt, b.withTrimmedRight (14), juce::Justification::centred);
        juce::Path tri;   // little triangle at the right edge, like Live's number boxes
        const float x = r.getRight() - 4.f, cy = r.getCentreY();
        tri.addTriangle (x, cy - 6, x, cy + 6, x - 9, cy);
        g.fillPath (tri);
    }
    else if (style == Orange)
    {
        g.setColour (Col::orange); g.setFont (F (19.f, true));
        g.drawText (txt, b, juce::Justification::centred);
    }
    else    // Knob
    {
        const float n = param.getValue();
        auto area = b.removeFromTop (b.getHeight() - 24).toFloat();
        const auto c = area.getCentre();
        const float rad = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.f;
        const float a0 = juce::degreesToRadians (-135.f), a1 = juce::degreesToRadians (135.f);
        const float ang = a0 + n * (a1 - a0);
        juce::Path track, val;
        track.addCentredArc (c.x, c.y, rad, rad, 0.f, a0, a1, true);
        val.addCentredArc (c.x, c.y, rad, rad, 0.f, a0, ang, true);
        g.setColour (juce::Colour (0xff7a8a90)); g.strokePath (track, juce::PathStrokeType (4.f));
        g.setColour (Col::cyan);                 g.strokePath (val, juce::PathStrokeType (4.f));
        g.setColour (Col::display); g.fillEllipse (c.x - rad + 7, c.y - rad + 7, 2 * (rad - 7), 2 * (rad - 7));
        g.setColour (juce::Colours::white);
        g.drawLine (c.x + std::sin (ang) * (rad - 18), c.y - std::cos (ang) * (rad - 18),
                    c.x + std::sin (ang) * (rad - 8),  c.y - std::cos (ang) * (rad - 8), 2.5f);
        g.setColour (Col::text); g.setFont (F (18.f));
        g.drawText (txt, getLocalBounds().removeFromBottom (24), juce::Justification::centred);
    }
}

// ---------------------------------------------------------------- filter display
FilterDisplay::FilterDisplay (DelayAudioProcessor& p)
    : proc (p),
      fP (p.apvts.getParameter (IDs::fFreq)), wP (p.apvts.getParameter (IDs::fWidth)), onP (p.apvts.getParameter (IDs::fOn)),
      fAtt (*fP, [this] (float) { repaint(); }), wAtt (*wP, [this] (float) { repaint(); }), oAtt (*onP, [this] (float) { repaint(); })
{
    fAtt.sendInitialUpdate(); wAtt.sendInitialUpdate(); oAtt.sendInitialUpdate();
}

juce::Point<float> FilterDisplay::handlePos() const
{
    const float f = fP->convertFrom0to1 (fP->getValue());
    const float wn = wP->getValue();                       // 0..1
    return { xFromFreq (f, (float) getWidth()), (float) getHeight() * (0.95f - 0.75f * wn) };
}

void FilterDisplay::mouseDown (const juce::MouseEvent& e)
{
    fAtt.beginGesture(); wAtt.beginGesture(); mouseDrag (e);
}

void FilterDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float f = 20.f * std::pow (1000.f, juce::jlimit (0.f, 1.f, (float) e.x / w));
    const float wn = juce::jlimit (0.f, 1.f, (0.95f - (float) e.y / h) / 0.75f);
    fAtt.setValueAsPartOfGesture (juce::jlimit (50.f, 18000.f, f));
    wAtt.setValueAsPartOfGesture (wP->convertFrom0to1 (wn));
}

void FilterDisplay::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), h = b.getHeight();
    const bool on = onP->getValue() > 0.5f;

    // log-frequency grid
    for (int dec = 0; dec < 3; ++dec)
        for (int m = 1; m < 10; ++m)
        {
            const float f = 10.f * (float) m * std::pow (10.f, (float) dec + 1.f);   // 100 Hz ...
            const float x = xFromFreq (f, w);
            if (x < 0 || x > w) continue;
            g.setColour (m == 1 ? Col::grid.brighter (0.25f) : Col::grid);
            g.drawVerticalLine ((int) x, 0.f, h);
        }
    g.setColour (Col::grid);
    for (int i = 1; i < 5; ++i) g.drawHorizontalLine ((int) (h * i / 5.f), 0.f, w);

    // band-pass response
    const float f0 = fP->convertFrom0to1 (fP->getValue()), q = wP->convertFrom0to1 (wP->getValue());
    juce::Path curve;
    for (int px = 0; px <= (int) w; ++px)
    {
        const float f = 20.f * std::pow (1000.f, (float) px / w);
        const float x = f / f0;
        const float mag = (x / q) / std::sqrt ((1.f - x * x) * (1.f - x * x) + (x / q) * (x / q));
        const float db = juce::jlimit (-40.f, 6.f, 20.f * std::log10 (juce::jmax (mag, 1.0e-4f)));
        const float y = h * (1.f - (db + 40.f) / 62.f) ;
        if (px == 0) curve.startNewSubPath (0.f, y); else curve.lineTo ((float) px, y);
    }
    g.setColour (on ? Col::cyan : juce::Colour (0xff6a6a6a));
    g.strokePath (curve, juce::PathStrokeType (3.f));

    if (on)
    {
        const auto hp = handlePos();
        g.setColour (Col::orange);
        g.drawEllipse (hp.x - 12.f, hp.y - 12.f, 24.f, 24.f, 3.f);
    }
}

// ---------------------------------------------------------------- editor
static juce::String pct (float v, int dp = 0) { return juce::String (v, dp) + " %"; }
static juce::String hz (float v) { return v >= 1000.f ? juce::String (v / 1000.f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }

DelayEditor::DelayEditor (DelayAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      lSyncWatch (*p.apvts.getParameter (IDs::lSync), [this] (float) { updateVisibility(); }),
      rSyncWatch (*p.apvts.getParameter (IDs::rSync), [this] (float) { updateVisibility(); })
{
    lSync = add<ToggleBtn> (P (IDs::lSync), "Sync");  rSync = add<ToggleBtn> (P (IDs::rSync), "Sync");
    for (int i = 0; i < 8; ++i)
    {
        const auto t = juce::String (DelayAudioProcessor::divisions[(size_t) i]);
        lDivs.push_back (add<ChoiceBtn> (P (IDs::lDiv), i, t));
        rDivs.push_back (add<ChoiceBtn> (P (IDs::rDiv), i, t));
    }
    auto timeFmt = [] (float v) { return v >= 1000.f ? juce::String (v / 1000.f, 2) + " s" : juce::String (v, 1) + " ms"; };
    lTime = add<DragValue> (P (IDs::lTime), DragValue::Field, timeFmt);
    rTime = add<DragValue> (P (IDs::rTime), DragValue::Field, timeFmt);
    lOff = add<DragValue> (P (IDs::lOff), DragValue::Field, [] (float v) { return pct (v, 1); });
    rOff = add<DragValue> (P (IDs::rOff), DragValue::Field, [] (float v) { return pct (v, 1); });

    link = add<ToggleBtn> (P (IDs::link), ""); link->symbolLink = true;
    feedback = add<DragValue> (P (IDs::feedback), DragValue::Knob, [] (float v) { return pct (v, 0); });
    freeze = add<ToggleBtn> (P (IDs::freeze), ""); freeze->symbolInfinity = true;
    dryWet = add<DragValue> (P (IDs::dryWet), DragValue::Knob, [] (float v) { return pct (v, 0); });

    for (int i = 0; i < 3; ++i)
        modes.push_back (add<ChoiceBtn> (P (IDs::mode), i, juce::StringArray { "Repitch", "Fade", "Jump" }[i]));
    pingpong = add<ToggleBtn> (P (IDs::pingpong), "Ping Pong");

    display = add<FilterDisplay> (proc);
    filterOn = add<ToggleBtn> (P (IDs::fOn), ""); filterOn->squareStyle = true;
    fFreq  = add<DragValue> (P (IDs::fFreq),  DragValue::Orange, hz);
    fWidth = add<DragValue> (P (IDs::fWidth), DragValue::Orange, [] (float v) { return juce::String (v, 2); });
    mRate   = add<DragValue> (P (IDs::mRate),   DragValue::Orange, [] (float v) { return juce::String (v, 2) + " Hz"; });
    mFilter = add<DragValue> (P (IDs::mFilter), DragValue::Orange, [] (float v) { return pct (v); });
    mTime   = add<DragValue> (P (IDs::mTime),   DragValue::Orange, [] (float v) { return pct (v); });

    for (auto* id : { IDs::lSync, IDs::rSync, IDs::lDiv, IDs::rDiv, IDs::lTime, IDs::rTime, IDs::link })
        proc.apvts.addParameterListener (id, this);

    lSyncWatch.sendInitialUpdate(); rSyncWatch.sendInitialUpdate();
    setSize (742, 376);   // after all children exist, so resized() never sees null pointers
}

DelayEditor::~DelayEditor()
{
    for (auto* id : { IDs::lSync, IDs::rSync, IDs::lDiv, IDs::rDiv, IDs::lTime, IDs::rTime, IDs::link })
        proc.apvts.removeParameterListener (id, this);
}

void DelayEditor::updateVisibility()
{
    const bool ls = P (IDs::lSync).getValue() > 0.5f, rs = P (IDs::rSync).getValue() > 0.5f;
    for (auto* b : lDivs) b->setVisible (ls);
    for (auto* b : rDivs) b->setVisible (rs);
    lTime->setVisible (! ls);  rTime->setVisible (! rs);
}

void DelayEditor::parameterChanged (const juce::String& id, float value)
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<DelayEditor> (this), id, value]
    { if (safe != nullptr) safe->mirror (id, value); });
}

// When "link" is on, the right side follows the left (and vice versa) so the display matches what the DSP uses.
void DelayEditor::mirror (const juce::String& id, float value)
{
    if (mirroring) return;
    const bool linked = P (IDs::link).getValue() > 0.5f;
    if (! linked) return;
    const char* pairs[][2] = { { IDs::lSync, IDs::rSync }, { IDs::lDiv, IDs::rDiv }, { IDs::lTime, IDs::rTime } };
    mirroring = true;
    auto copy = [&] (const char* from, const char* to)
    {
        auto& src = P (from); auto& dst = P (to);
        const float v = id == "link" ? src.convertFrom0to1 (src.getValue()) : value;
        if (std::abs (dst.convertFrom0to1 (dst.getValue()) - v) > 1.0e-4f)
        { dst.beginChangeGesture(); dst.setValueNotifyingHost (dst.convertTo0to1 (v)); dst.endChangeGesture(); }
    };
    for (auto& pr : pairs)
    {
        if (id == "link" || id == pr[0]) copy (pr[0], pr[1]);
        else if (id == pr[1])            copy (pr[1], pr[0]);
    }
    mirroring = false;
}

void DelayEditor::resized()
{
    auto setGrid = [] (std::vector<ChoiceBtn*>& v, int x0)
    {
        for (size_t i = 0; i < v.size(); ++i)
            v[i]->setBounds (x0 + (int) (i % 2) * 49, 103 + (int) (i / 2) * 32, 43, 26);
    };
    lSync->setBounds (13, 70, 92, 27);   rSync->setBounds (153, 70, 92, 27);
    setGrid (lDivs, 13);                 setGrid (rDivs, 153);
    lTime->setBounds (13, 103, 92, 26);  rTime->setBounds (153, 103, 92, 26);
    lOff->setBounds (13, 230, 92, 26);   rOff->setBounds (153, 230, 92, 26);
    link->setBounds (113, 150, 32, 27);

    feedback->setBounds (80, 272, 100, 100);   // knob + readout
    freeze->setBounds (200, 305, 58, 28);

    display->setBounds (258, 42, 350, 179);
    filterOn->setBounds (270, 234, 17, 17);
    fFreq->setBounds (355, 232, 100, 22);
    fWidth->setBounds (530, 232, 60, 22);
    mRate->setBounds (285, 328, 80, 22);
    mFilter->setBounds (392, 328, 80, 22);
    mTime->setBounds (495, 328, 80, 22);

    for (size_t i = 0; i < modes.size(); ++i) modes[i]->setBounds (620, 70 + (int) i * 32, 110, 27);
    pingpong->setBounds (620, 198, 110, 27);
    dryWet->setBounds (618, 272, 100, 100);
}

void DelayEditor::paint (Graphics& g)
{
    g.fillAll (Col::bg);
    // title bar
    g.setColour (Col::bg.brighter (0.05f)); g.fillRect (0, 0, getWidth(), 34);
    g.setColour (Col::orange); g.fillEllipse (8.f, 6.f, 22.f, 22.f);
    g.setColour (Col::text);   g.drawEllipse (8.f, 6.f, 22.f, 22.f, 1.5f);
    g.setFont (F (20.f, true)); g.drawText ("Delay", 40, 0, 150, 34, juce::Justification::centredLeft);

    g.setFont (F (19.f));
    g.drawText ("Left",  13, 42, 92, 24, juce::Justification::centred);
    g.drawText ("Right", 153, 42, 92, 24, juce::Justification::centred);
    g.drawText ("Mode",  620, 42, 110, 24, juce::Justification::centred);
    g.drawText ("Feedback", 80, 266, 100, 0, juce::Justification::centred);
    g.setColour (Col::grid.darker (0.2f).withAlpha (0.6f));
    g.drawVerticalLine (128, 72.f, 256.f);

    g.setColour (Col::text); g.setFont (F (19.f));
    g.drawText ("Feedback", Rectangle<int> (60, 264, 140, 24), juce::Justification::centred);
    g.drawText ("Dry/Wet",  Rectangle<int> (598, 264, 140, 24), juce::Justification::centred);
    g.setColour (Col::text.withAlpha (0.8f));
    g.drawHorizontalLine (320, 182.f, 198.f);

    // dark lower panel behind display
    g.setColour (Col::display);
    g.fillRoundedRectangle (258.f, 221.f, 350.f, 145.f, 6.f);
    g.setColour (juce::Colour (0xffb0b0b0)); g.setFont (F (19.f));
    g.drawText ("Filter", 292, 232, 60, 22, juce::Justification::centredLeft);
    g.drawText ("Width", 468, 232, 60, 22, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff8a8a8a)); g.setFont (F (19.f));
    g.drawText ("Modulation", 258, 270, 350, 24, juce::Justification::centred);
    g.setColour (Col::grid);
    g.drawHorizontalLine (282, 270.f, 355.f);  g.drawHorizontalLine (282, 510.f, 596.f);
    g.setColour (juce::Colour (0xffb0b0b0)); g.setFont (F (19.f));
    g.drawText ("Rate",   285, 298, 80, 24, juce::Justification::centred);
    g.drawText ("Filter", 392, 298, 80, 24, juce::Justification::centred);
    g.drawText ("Time",   495, 298, 80, 24, juce::Justification::centred);
}
