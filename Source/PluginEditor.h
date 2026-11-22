#pragma once
#include "PluginProcessor.h"
#include <functional>
#include <array>

#define RU(s) juce::String::fromUTF8 (s)
using Fmt = std::function<juce::String (double)>;

struct ThemeColors { juce::Colour a, a2, liq1, liq2, bgTop, bgBot; };
namespace Fonts { juce::Font ui (float h); juce::Font title (float h); }

// Большая ручка: настоящая крышка банки + круговой спектр вокруг
class LidKnob : public juce::Slider
{
public:
    LidKnob (juce::Image sprite, Fmt f, const ThemeColors* t);
    void setBands (const float* b) { std::copy (b, b + 32, bands); repaint(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void()> onClick;
private:
    juce::Image src, scaled; Fmt fmt; const ThemeColors* th; float bands[32] {};
};

// Малая ручка: та же крышка, только меньше
class MiniKnob : public juce::Slider
{
public:
    MiniKnob (juce::String label, juce::Image sprite, Fmt f, const ThemeColors* t);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    juce::String label; juce::Image src, scaled; Fmt fmt; const ThemeColors* th;
};

// Банка-режим: клик выбирает режим и тему
class ModeCan : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModeCan (juce::String name, juce::Image sprite, juce::Colour glow);
    void setSelected (bool s) { selected = s; repaint(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    std::function<void()> onClick;
private:
    juce::String label; juce::Image src, scaled; juce::Colour glowC; bool selected = false;
};

class TubeMeter : public juce::Component
{
public:
    explicit TubeMeter (const ThemeColors* t) : th (t) {}
    void setLevel (float v) { level = v; }
    void tick (float spawnPerSec);
    void paint (juce::Graphics&) override;
private:
    struct Bubble { float x, y, r, vy, ph; };
    std::vector<Bubble> bubbles;
    juce::Random rng;
    float level = 0.f;
    const ThemeColors* th;
};

class GazyavaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GazyavaEditor (GazyavaProcessor&);
    ~GazyavaEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
private:
    struct Fx { float x, y, vx, vy, s, life, ph; int kind; };
    struct Ring { juce::Point<float> c; float r, life; };
    struct ChoiceGroup { std::vector<std::unique_ptr<juce::TextButton>> btns; std::unique_ptr<juce::ParameterAttachment> att; };
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void drawSideCan (juce::Graphics&, juce::Rectangle<int>, int side);
    MiniKnob* addKnob (const char* id, const char* label, Fmt f, juce::Rectangle<int> r, int page, const char* tip);
    void addChoice (const char* id, std::vector<const char*> names, int cols, juce::Rectangle<int> area, int bw, int bh, int gap, int radio, int page);
    juce::TextButton* addToggle (const char* id, const char* text, juce::Rectangle<int> r, int page, const char* tip);
    void setPage (int p);
    void applyTheme (int mode);
    void spawnFx (int kind, float x, float y, float vx, float vy, float s, float life);

    std::unique_ptr<juce::LookAndFeel_V4> lnf;
    GazyavaProcessor& proc;
    ThemeColors cur;
    juce::Image imgBig, imgSpk, imgCatL, imgCatR, imgLid;
    juce::Image bigS, spkS, catLS, catRS;
    juce::Rectangle<int> canL, canR, catRectL, catRectR;
    juce::Path titlePath;
    std::vector<Fx> fx;
    std::vector<Ring> rings;
    std::vector<juce::Point<float>> bolt;
    juce::Random rng;
    juce::dsp::FFT fft { 11 };
    juce::dsp::WindowingFunction<float> window { 2048, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 4096> fftData {};
    float bands[32] {}, fxAlpha[5] {}, t = 0.f;
    float lvIn = 0.f, lvOut = 0.f, bassEnv = 0.f, prevBass = 0.f, jx[2] {}, jy[2] {}, meowAnim[2] {}, boltAlpha = 0.f, flash = 0.f, boltCool = 0.f;
    int curMode = 0, page = 0, presetIdx = 0, themeAnim = 0;

    LidKnob lid;
    TubeMeter tubeL, tubeR;
    std::vector<std::unique_ptr<MiniKnob>> knobs;
    std::vector<std::unique_ptr<ModeCan>> modeCans;
    std::vector<std::unique_ptr<ChoiceGroup>> groups;
    std::vector<juce::TextButton*> allButtons;
    std::vector<juce::Component*> pageComps[4];
    std::vector<std::unique_ptr<juce::TextButton>> miscButtons;
    juce::TextButton limBtn, monoBtn, pageBtn[4], prevBtn, nextBtn, rndBtn, bypBtn;
    juce::TooltipWindow tooltip { this, 450 };

    std::unique_ptr<juce::ParameterAttachment> modeAtt;
    std::vector<std::unique_ptr<SA>> sliderAtts;
    std::vector<std::unique_ptr<BA>> buttonAtts;
    std::unique_ptr<SA> aLid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GazyavaEditor)
};
