#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const int kModeShape[5] = { 0, 1, 2, 4, 5 };   // лайм / тропик / гром / синий / чёрный
    const int kSatShape[3]  = { 0, 1, 4 };
    const double kBeats[5]  = { 4.0, 2.0, 1.0, 0.5, 0.25 };

    struct PV { const char* id; float v; };
    struct PresetDef { const char* name; std::vector<PV> v; };

    const std::vector<PresetDef>& presets()
    {
        static const std::vector<PresetDef> p = {
            { "ЛАЙМ 808",        { {"mode",0}, {"boost",.55f}, {"drive",.45f}, {"sub",.35f}, {"tight",.35f}, {"crunch",.15f}, {"sat",.2f} } },
            { "ТРОПИК САБ",      { {"mode",1}, {"boost",.5f}, {"drive",.35f}, {"sub",.5f}, {"ice",.2f}, {"rmix",.2f}, {"rsize",.6f}, {"chmix",.25f} } },
            { "ГРОМОВОЙ ФАЗЗ",   { {"mode",2}, {"boost",.6f}, {"drive",.7f}, {"dist",.35f}, {"disttype",1}, {"overload",.2f}, {"crunch",.4f}, {"cmix",.5f}, {"cratio",6} } },
            { "СИНИЙ ЛЁД",       { {"mode",3}, {"boost",.5f}, {"sub",.7f}, {"drive",.3f}, {"ice",.35f}, {"width",.7f}, {"rmix",.3f}, {"rfizz",.6f}, {"dmix",.2f}, {"dtime",375} } },
            { "ЧЁРНАЯ ЯМА",      { {"mode",4}, {"boost",.8f}, {"drive",.8f}, {"overload",.45f}, {"dist",.3f}, {"tight",.5f}, {"cmix",.8f}, {"cthr",-20}, {"cratio",10} } },
            { "ДАБСТЕП ВОБЛ",    { {"mode",2}, {"boost",.6f}, {"drive",.6f}, {"wmix",.9f}, {"wcut",.4f}, {"wres",.6f}, {"wdiv",3}, {"wdepth",.8f}, {"dist",.2f} } },
            { "ТРЭП БУМ",        { {"mode",4}, {"boost",.7f}, {"sub",.8f}, {"drive",.5f}, {"tight",.25f}, {"cmix",.5f}, {"cthr",-16}, {"gthr",-50}, {"grel",80} } },
            { "КЛУБНЫЙ ПАМП",    { {"mode",0}, {"boost",.5f}, {"sub",.4f}, {"pamt",.7f}, {"pdiv",0}, {"pcurve",.5f}, {"cmix",.4f}, {"dmix",.15f}, {"rmix",.15f} } },
            { "ГАЗ НА ПОЛНУЮ",   { {"mode",2}, {"drive",1}, {"boost",.9f}, {"dist",.5f}, {"sat",.5f}, {"overload",.6f}, {"rmix",.3f}, {"rfizz",.9f} } },
            { "ЧИСТЫЙ САБ",      { {"mode",3}, {"boost",.35f}, {"drive",.15f}, {"sub",.6f}, {"crunch",0}, {"ice",.5f} } },
            { "ЛОУФАЙ ЛЕНТА",    { {"mode",1}, {"drive",.5f}, {"sat",.6f}, {"sattype",1}, {"ice",.7f}, {"chmix",.4f}, {"chrate",.3f}, {"tdepth",.2f}, {"dmix",.25f} } },
            { "МАНУЛ-ГРУВ",      { {"mode",1}, {"boost",.55f}, {"sub",.3f}, {"pamt",.35f}, {"pdiv",1}, {"tdepth",.25f}, {"trate",3}, {"rmix",.25f}, {"rfizz",.5f}, {"width",.8f} } },
        };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout GazyavaProcessor::createLayout()
{
    using NR = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&p] (const char* id, const char* name, NR r, float def)
    { p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def)); };
    auto skew = [] (float lo, float hi, float step, float centre) { NR r (lo, hi, step); r.setSkewForCentre (centre); return r; };
    auto n01 = [] { return NR (0.f, 1.f, 0.001f); };
    auto choice = [&p] (const char* id, const char* name, juce::StringArray s, int def)
    { p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, s, def)); };
    auto toggle = [&p] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def)); };

    add ("in", "Input", NR (-12.f, 12.f, 0.01f), 0.f);
    add ("drive", "Drive", n01(), 0.4f);       add ("boost", "Bass Boost", n01(), 0.4f);
    add ("sub", "Sub", n01(), 0.f);            add ("freq", "Crossover", skew (40.f, 300.f, 0.1f, 100.f), 100.f);
    add ("tight", "Tight", n01(), 0.3f);       add ("crunch", "Crunch", n01(), 0.2f);
    add ("ice", "Ice Tone", n01(), 0.f);       add ("width", "Width", n01(), 0.5f);
    add ("overload", "Overload", n01(), 0.f);  add ("mix", "Mix", n01(), 1.f);
    add ("out", "Output", NR (-18.f, 6.f, 0.01f), 0.f);
    add ("sat", "Saturation", n01(), 0.f);     add ("dist", "Distortion", n01(), 0.f);
    add ("cthr", "Comp Thresh", NR (-40.f, 0.f, 0.1f), -14.f);
    add ("cratio", "Comp Ratio", skew (1.f, 20.f, 0.1f, 4.f), 3.f);
    add ("catk", "Comp Attack", skew (1.f, 100.f, 0.1f, 20.f), 10.f);
    add ("crel", "Comp Release", skew (20.f, 500.f, 1.f, 120.f), 120.f);
    add ("cgain", "Comp Makeup", NR (0.f, 12.f, 0.1f), 0.f);   add ("cmix", "Comp Mix", n01(), 0.f);
    add ("gthr", "Gate Thresh", NR (-80.f, -20.f, 0.1f), -80.f);
    add ("grel", "Gate Release", skew (20.f, 400.f, 1.f, 100.f), 100.f);
    add ("rsize", "Reverb Size", n01(), 0.5f); add ("rdamp", "Reverb Damp", n01(), 0.5f);
    add ("rpre", "Reverb Predelay", NR (0.f, 100.f, 0.1f), 20.f);
    add ("rfizz", "Reverb Fizz", n01(), 0.4f); add ("rduck", "Reverb Duck", n01(), 0.5f); add ("rmix", "Reverb Mix", n01(), 0.f);
    add ("dtime", "Echo Time", skew (20.f, 1000.f, 1.f, 300.f), 250.f);
    add ("dfb", "Echo Feedback", NR (0.f, 0.95f, 0.001f), 0.35f);
    add ("dtone", "Echo Tone", n01(), 0.5f);   add ("dmix", "Echo Mix", n01(), 0.f);
    add ("chrate", "Chorus Rate", skew (0.1f, 5.f, 0.01f, 1.f), 0.8f);
    add ("chdepth", "Chorus Depth", n01(), 0.4f); add ("chmix", "Chorus Mix", n01(), 0.f);
    add ("eqlow", "EQ Low", NR (-12.f, 12.f, 0.1f), 0.f);  add ("eqmid", "EQ Mid", NR (-12.f, 12.f, 0.1f), 0.f);
    add ("eqmidf", "EQ Mid Freq", skew (200.f, 5000.f, 1.f, 1000.f), 1000.f);
    add ("eqhigh", "EQ High", NR (-12.f, 12.f, 0.1f), 0.f);
    add ("wcut", "Wobble Cutoff", n01(), 0.5f); add ("wres", "Wobble Reso", n01(), 0.3f);
    add ("wdepth", "Wobble Depth", n01(), 0.6f); add ("wmix", "Wobble Mix", n01(), 0.f);
    add ("pamt", "Pump Amount", n01(), 0.f);   add ("pcurve", "Pump Curve", n01(), 0.5f);
    add ("trate", "Tremolo Rate", skew (0.5f, 20.f, 0.01f, 4.f), 4.f); add ("tdepth", "Tremolo Depth", n01(), 0.f);
    choice ("mode", "Bass Mode", { "Lime", "Tropic", "Thunder", "Blue", "Black" }, 0);
    choice ("sattype", "Sat Type", { "Tube", "Tape", "Transformer" }, 0);
    choice ("disttype", "Dist Type", { "Hard", "Fuzz", "Fold", "Crush" }, 0);
    choice ("wdiv", "Wobble Rate", { "1/1", "1/2", "1/4", "1/8", "1/16" }, 2);
    choice ("wtype", "Wobble Type", { "LP", "BP", "HP" }, 0);
    choice ("pdiv", "Pump Rate", { "1/4", "1/8", "1/16" }, 0);
    toggle ("limiter", "Limiter", true); toggle ("monobass", "Mono Bass", false);
    toggle ("dpp", "Echo Ping-Pong", true); toggle ("bypass", "Bypass", false);
    return { p.begin(), p.end() };
}

GazyavaProcessor::GazyavaProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GAZYAVA", createLayout())
{
    loadMeow (0, BinaryData::meow_a_wav, BinaryData::meow_a_wavSize);
    loadMeow (1, BinaryData::meow_b_wav, BinaryData::meow_b_wavSize);
    loadMeow (2, BinaryData::meow_c_wav, BinaryData::meow_c_wavSize);
}

void GazyavaProcessor::loadMeow (int idx, const void* data, int size)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
    if (r == nullptr) return;
    meow[idx].setSize (1, (int) r->lengthInSamples);
    r->read (&meow[idx], 0, (int) r->lengthInSamples, 0, true, false);
    meowRate[idx] = r->sampleRate;
}

void GazyavaProcessor::triggerMeow (int which, float pitch)
{
    meowPitch.store (pitch);
    meowReq.store (which);
}

int GazyavaProcessor::getNumPresets() { return (int) presets().size(); }
juce::String GazyavaProcessor::getPresetName (int i) { return juce::String::fromUTF8 (presets()[(size_t) juce::jlimit (0, getNumPresets() - 1, i)].name); }

void GazyavaProcessor::applyPreset (int idx)
{
    if (idx < 0 || idx >= getNumPresets()) return;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "bypass") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& pv : presets()[(size_t) idx].v)
        if (auto* rp = apvts.getParameter (pv.id)) rp->setValueNotifyingHost (rp->convertTo0to1 (pv.v));
}

void GazyavaProcessor::randomize()
{
    juce::Random r ((juce::int64) juce::Time::getMillisecondCounter());
    auto set = [this] (const char* id, float v) { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    auto R = [&r] (float a, float b) { return a + r.nextFloat() * (b - a); };
    auto maybe = [&r] (float chance) { return r.nextFloat() < chance; };
    set ("mode", (float) r.nextInt (5)); set ("sattype", (float) r.nextInt (3)); set ("disttype", (float) r.nextInt (4));
    set ("drive", R (.2f, .85f)); set ("boost", R (.3f, .8f)); set ("sub", R (0, .7f)); set ("tight", R (.2f, .6f));
    set ("crunch", R (0, .6f)); set ("ice", R (0, .5f)); set ("width", R (.3f, .8f));
    set ("sat", maybe (.5f) ? R (.1f, .6f) : 0.f); set ("dist", maybe (.4f) ? R (.1f, .5f) : 0.f); set ("overload", maybe (.3f) ? R (.1f, .5f) : 0.f);
    set ("cmix", R (0, .8f)); set ("cthr", R (-30, -8)); set ("cratio", R (2, 8));
    set ("rmix", R (0, .35f)); set ("rfizz", R (0, 1)); set ("rsize", R (.2f, .9f));
    set ("dmix", R (0, .3f)); set ("dtime", R (120, 600)); set ("dfb", R (.2f, .6f));
    set ("chmix", maybe (.4f) ? R (.1f, .5f) : 0.f);
    set ("wmix", maybe (.35f) ? R (.4f, 1.f) : 0.f); set ("wcut", R (.2f, .7f)); set ("wres", R (.1f, .7f));
    set ("wdiv", (float) r.nextInt (5)); set ("wdepth", R (.3f, 1.f)); set ("wtype", (float) r.nextInt (3));
    set ("pamt", maybe (.4f) ? R (.3f, .8f) : 0.f); set ("pdiv", (float) r.nextInt (3));
    set ("tdepth", maybe (.2f) ? R (.1f, .5f) : 0.f);
    set ("eqlow", R (-4, 6)); set ("eqmid", R (-4, 4)); set ("eqhigh", R (-4, 4));
}

void GazyavaProcessor::getScope (float* dst, int n) const
{
    const int pos = scopePos.load();
    for (int k = 0; k < n; ++k) dst[k] = scope[(size_t) ((pos - n + k) & 4095)];
}

bool GazyavaProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

float GazyavaProcessor::shape (float x, float k, int type) noexcept
{
    switch (type)
    {
        case 0:  return (std::tanh (k * x + 0.2f) - 0.19738f) / std::tanh (k * 0.9f);                        // лампа
        case 1:  return (std::atan (k * x * 1.2f + 0.15f) - 0.14889f) / std::atan (k * 1.2f);                // лента
        case 2:  { const float a = 1.f - std::exp (-std::abs (x) * k * 1.4f);                                // фазз
                   return (x < 0 ? -a : a) / (1.f - std::exp (-k * 1.4f)); }
        case 3:  return std::sin (x * (1.f + k * 0.35f) * 1.2f);                                             // фолд
        case 4:  { const float g = 0.6f + k * 0.35f; return std::tanh (x * g) / std::tanh (g); }             // мягкий/чистый
        default: return juce::jlimit (-1.f, 1.f, x * (1.f + k * 0.8f)) * 0.9f;                               // жёсткий клип
    }
}

void GazyavaProcessor::updateTargets (bool snap)
{
    auto set = [snap] (juce::SmoothedValue<float>& s, float v)
    { if (snap) s.setCurrentAndTargetValue (v); else s.setTargetValue (v); };
    set (sIn,     juce::Decibels::decibelsToGain (getP ("in")));
    set (sDrive,  getP ("drive"));
    set (sBoost,  juce::Decibels::decibelsToGain (getP ("boost") * 18.f));
    set (sSub,    getP ("sub"));
    set (sCrunch, getP ("crunch"));
    set (sWidth,  getP ("width") * 2.f);
    set (sSat,    getP ("sat"));
    set (sDist,   getP ("dist"));
    set (sOvl,    getP ("overload"));
    set (sMix,    getP ("mix"));
    set (sOut,    juce::Decibels::decibelsToGain (getP ("out")));
    set (sBypass, getP ("bypass") > 0.5f ? 1.f : 0.f);
}

void GazyavaProcessor::prepareToPlay (double sr, int spb)
{
    sampleRate = sr;
    const double osr = sr * 4.0;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) spb, 2 };
    juce::dsp::ProcessSpec ospec { osr, (juce::uint32) (spb * 4), 2 };

    oversampling.initProcessing ((size_t) spb);
    oversampling.reset();
    xover.prepare (ospec);
    tone.prepare (spec);   tone.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    revHp.prepare (spec);  revHp.setType (juce::dsp::StateVariableTPTFilterType::highpass); revHp.setCutoffFrequency (180.f);
    wob.prepare (spec);    wob.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    limiter.prepare (spec); limiter.setThreshold (-0.5f); limiter.setRelease (60.f);
    comp.prepare (spec);
    reverb.prepare (spec);
    chorus.prepare (spec); chorus.setCentreDelay (7.f); chorus.setFeedback (0.f);
    eqL.prepare (spec); eqM.prepare (spec); eqH.prepare (spec);
    for (float& f : lastEq) f = 1e9f;
    revDelay.prepare (spec);
    echo.prepare (spec);

    const int lat = (int) std::round (oversampling.getLatencyInSamples());
    setLatencySamples (lat);
    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) lat);
    dryBuf.setSize (2, spb); compBuf.setSize (2, spb); revBuf.setSize (2, spb);

    for (auto* s : { &sIn, &sDrive, &sBoost, &sSub, &sCrunch, &sWidth, &sSat, &sDist, &sOvl }) s->reset (osr, 0.02);
    for (auto* s : { &sMix, &sOut, &sBypass }) s->reset (sr, 0.03);
    updateTargets (true);
    for (int c = 0; c < 2; ++c) { subEnv[c] = subL1[c] = subL2[c] = tz1[c] = tz2[c] = held[c] = revLp[c] = echoLp[c] = 0.f; subSgn[c] = 1; }
    gateGain = 1.f; gateEnv = 0.f; pumpG = 1.f;
}

void GazyavaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0) return;

    // темп / позиция из хоста (для вобла и памп-компрессии)
    double bpm = 120.0, ppq = 0.0; bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { ppq = *p; havePpq = true; }
        }
    const double beatInc = juce::jmax (1.0, bpm) / 60.0 / sampleRate;
    const double beat0 = (playing && havePpq) ? ppq : freeBeats;
    freeBeats += n * beatInc;

    updateTargets (false);
    const int  mode     = juce::jlimit (0, 4, juce::roundToInt (getP ("mode")));
    const int  satT     = juce::jlimit (0, 2, juce::roundToInt (getP ("sattype")));
    const int  distT    = juce::jlimit (0, 3, juce::roundToInt (getP ("disttype")));
    const bool monoBass = getP ("monobass") > 0.5f;
    const bool limOn    = getP ("limiter") > 0.5f;
    const float ice = getP ("ice"), tight = getP ("tight"), distP = getP ("dist");
    xover.setCutoffFrequency (getP ("freq"));

    const float inPk = juce::jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n));
    inLevel.store (juce::jmax (inLevel.load(), inPk));

    dryBuf.setSize (2, n, false, false, true);
    for (int c = 0; c < 2; ++c)
    {
        auto* src = buffer.getReadPointer (c);
        auto* d = dryBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i) { dryDelay.pushSample (c, src[i]); d[i] = dryDelay.popSample (c); }
    }

    // ---------- нелинейная часть на 4x oversampling ----------
    const float osr = (float) (sampleRate * 4.0), twoPi = juce::MathConstants<float>::twoPi;
    const float aSub  = 1.f - std::exp (-twoPi * 70.f / osr);
    const float aEnvA = 1.f - std::exp (-1.f / (0.002f * osr)), aEnvR = 1.f - std::exp (-1.f / (0.08f * osr));
    const float aTight = 1.f - std::exp (-twoPi * (18.f + tight * 55.f) / osr);
    const int   crushN = 1 + (int) (distP * 14.f);
    const float crushLv = std::pow (2.f, 12.f - distP * 9.f);
    const int   mShape = kModeShape[mode], sShape = kSatShape[satT];

    juce::dsp::AudioBlock<float> blk (buffer);
    auto sub = blk.getSubsetChannelBlock (0, 2).getSubBlock (0, (size_t) n);
    auto up = oversampling.processSamplesUp (sub);
    float* ch[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };
    const int nu = (int) up.getNumSamples();
    float bassPk = 0.f;

    for (int i = 0; i < nu; ++i)
    {
        const float inG = sIn.getNextValue(), dr = sDrive.getNextValue(), bg = sBoost.getNextValue(), sb = sSub.getNextValue(),
                    cr = sCrunch.getNextValue(), wd = sWidth.getNextValue(), sa = sSat.getNextValue(), di = sDist.getNextValue(),
                    ov = sOvl.getNextValue();
        const float k = (1.f + dr * 14.f) * (mode == 2 ? 1.2f : 1.f);
        const float subAmt = juce::jmin (1.f, sb + (mode == 3 ? 0.45f : 0.f));
        float lo[2], hi[2];
        for (int c = 0; c < 2; ++c) xover.processSample (c, ch[c][i] * inG, lo[c], hi[c]);
        if (monoBass) lo[0] = lo[1] = 0.5f * (lo[0] + lo[1]);
        { const float m = 0.5f * (hi[0] + hi[1]), s = 0.5f * (hi[0] - hi[1]) * wd; hi[0] = m + s; hi[1] = m - s; }

        for (int c = 0; c < 2; ++c)
        {
            const float x = lo[c];
            bassPk = juce::jmax (bassPk, std::abs (x));                      // реальная громкость баса -> тряска динамиков
            const float ax = std::abs (x);
            subEnv[c] += (ax - subEnv[c]) * (ax > subEnv[c] ? aEnvA : aEnvR);
            const int ns = x > 0.002f ? 1 : (x < -0.002f ? -1 : subSgn[c]);
            if (ns == 1 && subSgn[c] == -1) subFlip[c] = ! subFlip[c];
            subSgn[c] = ns;
            subL1[c] += aSub * ((subFlip[c] ? 1.f : -1.f) * subEnv[c] - subL1[c]);
            subL2[c] += aSub * (subL1[c] - subL2[c]);

            float s = shape (x * bg, k, mShape) + subL2[c] * subAmt * 1.2f;
            tz1[c] += aTight * (s - tz1[c]); s -= tz1[c];
            tz2[c] += aTight * (s - tz2[c]); s -= tz2[c];

            float h = hi[c];
            if (cr > 0.001f) { const float hs = shape (h * (1.f + cr * 3.f), 1.f + cr * 10.f, 0); h += (hs - h) * cr; }
            float y = s + h;

            if (sa > 0.001f) { const float ws = shape (y * (1.f + sa * 1.5f), 1.f + sa * 6.f, sShape); y += (ws - y) * sa; }
            if (di > 0.001f)
            {
                float d;
                switch (distT)
                {
                    case 0:  d = shape (y, 1.f + di * 30.f, 5); break;
                    case 1:  d = shape (y * (1.f + di * 4.f), 1.f + di * 18.f, 2); break;
                    case 2:  d = shape (y, 1.f + di * 18.f, 3); break;
                    default: if (++crushCnt[c] >= crushN) { crushCnt[c] = 0; held[c] = y; }
                             d = std::round (held[c] * crushLv) / crushLv; break;
                }
                y += (d - y) * juce::jmin (1.f, di * 2.5f);
            }
            if (ov > 0.001f)
            {
                const float z = juce::jlimit (-1.f, 1.f, y * (1.f + ov * 18.f));
                y += (z * 0.85f - y) * juce::jmin (1.f, ov * 3.f);
            }
            ch[c][i] = y;
        }
    }
    bassLevel.store (juce::jmax (bassLevel.load(), bassPk));
    oversampling.processSamplesDown (sub);

    // ---------- тон ----------
    if (ice > 0.01f)
    {
        tone.setCutoffFrequency (juce::jmin (20000.f * std::pow (0.075f, ice), 0.45f * (float) sampleRate));
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        tone.process (ctx);
    }

    // ---------- гейт ----------
    const float gthr = getP ("gthr");
    if (gthr > -79.5f)
    {
        const float thr = juce::Decibels::decibelsToGain (gthr), sr = (float) sampleRate;
        const float aA = 1.f - std::exp (-1.f / (0.002f * sr)), aR = 1.f - std::exp (-1.f / (getP ("grel") * 0.001f * sr));
        for (int i = 0; i < n; ++i)
        {
            const float a = juce::jmax (std::abs (buffer.getSample (0, i)), std::abs (buffer.getSample (1, i)));
            gateEnv = juce::jmax (a, gateEnv * 0.9993f);
            const float tgt = gateEnv > thr ? 1.f : 0.f;
            gateGain += (tgt - gateGain) * (tgt > gateGain ? aA : aR);
            buffer.applyGain (i, 1, gateGain);
        }
    }

    // ---------- вобл-фильтр (синхронно с темпом) ----------
    const float wmix = getP ("wmix");
    if (wmix > 0.001f)
    {
        const int wt = juce::jlimit (0, 2, juce::roundToInt (getP ("wtype")));
        wob.setType (wt == 0 ? juce::dsp::StateVariableTPTFilterType::lowpass : (wt == 1 ? juce::dsp::StateVariableTPTFilterType::bandpass : juce::dsp::StateVariableTPTFilterType::highpass));
        wob.setResonance (0.7f + getP ("wres") * 9.f);
        const double bpc = kBeats[juce::jlimit (0, 4, juce::roundToInt (getP ("wdiv")))];
        const float base = 40.f * std::pow (200.f, getP ("wcut")), depth = getP ("wdepth");
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0)
            {
                const float lfo = std::sin (twoPi * (float) std::fmod ((beat0 + i * beatInc) / bpc, 1.0));
                wob.setCutoffFrequency (juce::jlimit (25.f, 0.45f * (float) sampleRate, base * std::pow (2.f, depth * 3.f * lfo)));
            }
            for (int c = 0; c < 2; ++c)
            { const float x = buffer.getSample (c, i); buffer.setSample (c, i, x + (wob.processSample (c, x) - x) * wmix); }
        }
    }

    // ---------- эквалайзер ----------
    {
        const float eL = getP ("eqlow"), eM = getP ("eqmid"), eF = getP ("eqmidf"), eH = getP ("eqhigh");
        if (eL != lastEq[0] || eM != lastEq[1] || eF != lastEq[2] || eH != lastEq[3])
        {
            using Co = juce::dsp::IIR::Coefficients<float>;
            *eqL.state = *Co::makeLowShelf (sampleRate, 90.f, 0.7f, juce::Decibels::decibelsToGain (eL));
            *eqM.state = *Co::makePeakFilter (sampleRate, eF, 0.9f, juce::Decibels::decibelsToGain (eM));
            *eqH.state = *Co::makeHighShelf (sampleRate, 8000.f, 0.7f, juce::Decibels::decibelsToGain (eH));
            lastEq[0] = eL; lastEq[1] = eM; lastEq[2] = eF; lastEq[3] = eH;
        }
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        if (std::abs (eL) > 0.05f) eqL.process (ctx);
        if (std::abs (eM) > 0.05f) eqM.process (ctx);
        if (std::abs (eH) > 0.05f) eqH.process (ctx);
    }

    // ---------- хорус ----------
    const float chmix = getP ("chmix");
    if (chmix > 0.001f)
    {
        chorus.setRate (getP ("chrate")); chorus.setDepth (getP ("chdepth")); chorus.setMix (chmix);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        chorus.process (ctx);
    }

    // ---------- тремоло ----------
    const float td = getP ("tdepth");
    if (td > 0.001f)
    {
        const float inc = twoPi * getP ("trate") / (float) sampleRate;
        for (int i = 0; i < n; ++i)
        {
            trPhase += inc; if (trPhase > twoPi) trPhase -= twoPi;
            buffer.applyGain (i, 1, 1.f - td * (0.5f + 0.5f * std::sin (trPhase)));
        }
    }

    // ---------- компрессор (параллельный) ----------
    const float cmix = getP ("cmix");
    if (cmix > 0.001f)
    {
        comp.setThreshold (getP ("cthr")); comp.setRatio (getP ("cratio"));
        comp.setAttack (getP ("catk"));    comp.setRelease (getP ("crel"));
        compBuf.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c) compBuf.copyFrom (c, 0, buffer, c, 0, n);
        juce::dsp::AudioBlock<float> cb (compBuf);
        juce::dsp::ProcessContextReplacing<float> cctx (cb);
        comp.process (cctx);
        const float mk = juce::Decibels::decibelsToGain (getP ("cgain"));
        for (int c = 0; c < 2; ++c)
        {
            auto* o = buffer.getWritePointer (c); auto* cp = compBuf.getReadPointer (c);
            for (int i = 0; i < n; ++i) o[i] = o[i] * (1.f - cmix) + cp[i] * mk * cmix;
        }
    }

    // ---------- эхо (пинг-понг) ----------
    const float dmix = getP ("dmix");
    if (dmix > 0.001f)
    {
        const float dsamp = juce::jmin (getP ("dtime") * 0.001f * (float) sampleRate, 130000.f);
        const float fb = getP ("dfb"), aTone = 1.f - std::exp (-twoPi * (1500.f + getP ("dtone") * 14500.f) / (float) sampleRate);
        const bool pp = getP ("dpp") > 0.5f;
        echo.setDelay (dsamp);
        for (int i = 0; i < n; ++i)
        {
            const float inL = buffer.getSample (0, i), inR = buffer.getSample (1, i);
            if (pp) { echo.pushSample (0, 0.5f * (inL + inR) + fb * echoLp[1]); echo.pushSample (1, fb * echoLp[0]); }
            else    { echo.pushSample (0, inL + fb * echoLp[0]); echo.pushSample (1, inR + fb * echoLp[1]); }
            const float y0 = echo.popSample (0), y1 = echo.popSample (1);
            echoLp[0] += aTone * (y0 - echoLp[0]); echoLp[1] += aTone * (y1 - echoLp[1]);
            buffer.addSample (0, i, y0 * dmix); buffer.addSample (1, i, y1 * dmix);
        }
    }

    // ---------- реверб "физз": газированный хвост + дакинг ----------
    const float rmix = getP ("rmix");
    if (rmix > 0.001f)
    {
        revBuf.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c) revBuf.copyFrom (c, 0, buffer, c, 0, n);
        juce::dsp::AudioBlock<float> rb (revBuf);
        juce::dsp::ProcessContextReplacing<float> rctx (rb);
        revHp.process (rctx);
        revDelay.setDelay (getP ("rpre") * 0.001f * (float) sampleRate);
        for (int c = 0; c < 2; ++c)
        {
            auto* r = revBuf.getWritePointer (c);
            for (int i = 0; i < n; ++i) { revDelay.pushSample (c, r[i]); r[i] = revDelay.popSample (c); }
        }
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.3f + getP ("rsize") * 0.7f; rp.damping = getP ("rdamp");
        rp.wetLevel = 0.4f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
        reverb.setParameters (rp);
        reverb.process (rctx);

        const float fz = getP ("rfizz"), dk = getP ("rduck");
        for (int i = 0; i < n; ++i)
        {
            if (--fizzCnt <= 0) { fizzCnt = (int) ((float) sampleRate / (900.f + rnd.nextFloat() * 1800.f)); fizzTarget = rnd.nextFloat(); }
            fizzSm += 0.2f * (fizzTarget - fizzSm);
            const float gate = 1.f - fz * 0.85f * fizzSm;
            const float ain = std::abs (buffer.getSample (0, i)) + std::abs (buffer.getSample (1, i));
            duckEnv += (ain - duckEnv) * (ain > duckEnv ? 0.02f : 0.0008f);
            const float dg = 1.f - dk * juce::jlimit (0.f, 1.f, duckEnv * 1.5f);
            for (int c = 0; c < 2; ++c)
            {
                float r = revBuf.getSample (c, i);
                revLp[c] += 0.15f * (r - revLp[c]);
                r = (r + fz * 0.8f * (r - revLp[c])) * gate;
                buffer.addSample (c, i, r * rmix * dg * 1.4f);
            }
        }
    }

    // ---------- памп (сайдчейн-эффект в такт темпу) ----------
    const float pamt = getP ("pamt");
    if (pamt > 0.001f)
    {
        const int pd = juce::jlimit (0, 2, juce::roundToInt (getP ("pdiv")));
        const double bpc = pd == 0 ? 1.0 : (pd == 1 ? 0.5 : 0.25);
        const float pw = 1.f + getP ("pcurve") * 5.f;
        for (int i = 0; i < n; ++i)
        {
            const float ph = (float) std::fmod ((beat0 + i * beatInc) / bpc, 1.0);
            pumpG += 0.08f * ((1.f - pamt * std::pow (1.f - ph, pw)) - pumpG);
            buffer.applyGain (i, 1, pumpG);
        }
    }

    // ---------- микс / выход / байпас ----------
    for (int i = 0; i < n; ++i)
    {
        const float m = sMix.getNextValue(), og = sOut.getNextValue(), bp = sBypass.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            auto* o = buffer.getWritePointer (c);
            const float d = dryBuf.getSample (c, i);
            o[i] = ((d * (1.f - m) + o[i] * m) * og) * (1.f - bp) + d * bp;
        }
    }
    if (limOn) { juce::dsp::ProcessContextReplacing<float> ctx (sub); limiter.process (ctx); }

    for (int i = 0; i < n; ++i) scope[(size_t) ((scopeW + i) & 4095)] = 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i));
    scopeW += n; scopePos.store (scopeW);

    // ---------- мяуканье манулов ----------
    const int req = meowReq.exchange (-1);
    if (req >= 0 && req < 3 && meow[req].getNumSamples() > 2)
    { voice = req; voicePos = 0.0; voiceStep = meowRate[req] / sampleRate * (double) meowPitch.load(); }
    if (voice >= 0)
    {
        const int len = meow[voice].getNumSamples();
        const float* src = meow[voice].getReadPointer (0);
        for (int i = 0; i < n; ++i)
        {
            const int ip = (int) voicePos;
            if (ip >= len - 1) { voice = -1; break; }
            const float fr = (float) (voicePos - ip), v = (src[ip] * (1.f - fr) + src[ip + 1] * fr) * 0.9f;
            buffer.addSample (0, i, v); buffer.addSample (1, i, v);
            voicePos += voiceStep;
        }
    }

    const float outPk = juce::jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n));
    outLevel.store (juce::jmax (outLevel.load(), outPk));
}

juce::AudioProcessorEditor* GazyavaProcessor::createEditor() { return new GazyavaEditor (*this); }

void GazyavaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void GazyavaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GazyavaProcessor(); }
