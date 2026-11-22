#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const float kPi = juce::MathConstants<float>::pi;
    const int W = 1360, H = 800;

    // темы под каждую банку: акцент, 2-й акцент, жидкость в колбах, фон сверху/снизу
    const ThemeColors kThemes[5] = {
        { juce::Colour (0xffb6f500), juce::Colour (0xff3fe05a), juce::Colour (0xff4cc418), juce::Colour (0xff17802a), juce::Colour (0xff0a2a10), juce::Colour (0xff010502) },
        { juce::Colour (0xff2de0c8), juce::Colour (0xff2f9bff), juce::Colour (0xff3fe0cf), juce::Colour (0xff0b7f86), juce::Colour (0xff062c2e), juce::Colour (0xff010a0b) },
        { juce::Colour (0xffe0309a), juce::Colour (0xff9a4dff), juce::Colour (0xffd24cff), juce::Colour (0xff5a1aa8), juce::Colour (0xff220a30), juce::Colour (0xff070109) },
        { juce::Colour (0xff39b6ff), juce::Colour (0xff9af0ff), juce::Colour (0xff4cc4ff), juce::Colour (0xff1568b8), juce::Colour (0xff08203c), juce::Colour (0xff010510) },
        { juce::Colour (0xffff3b2a), juce::Colour (0xffffa020), juce::Colour (0xffff5a2a), juce::Colour (0xff991008), juce::Colour (0xff280808), juce::Colour (0xff060000) } };

    const char* kModeDesc[5] = {
        "ЛАЙМ — классический ламповый буст: тёплый и плотный бас",
        "ТРОПИК — мягкая лента: округлый, тёплый, сочный низ",
        "ГРОМ — тяжёлый фазз: грязный жирный бас. Молнии на ударах!",
        "СИНИЙ — чистый холодный буст + саб-октава",
        "ЧЁРНЫЙ — жёсткий клип: агрессивный перегруз баса" };

    juce::String pct (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; }
    juce::String db  (double v) { return juce::String (v, 1) + " dB"; }
    juce::String hz  (double v) { return juce::String (juce::roundToInt (v)) + " Hz"; }
    juce::String ms0 (double v) { return juce::String (juce::roundToInt (v)) + " ms"; }
    juce::Image load (const void* d, int n) { return juce::ImageCache::getFromMemory (d, n); }

    struct GazLnf : juce::LookAndFeel_V4
    {
        GazLnf()
        {
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colours::black.withAlpha (0.92f));
            setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
            setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xffb6f500));
        }
        juce::Font getTextButtonFont (juce::TextButton&, int h) override { return Fonts::ui (juce::jmin (13.f, (float) h * 0.52f)); }
    };
}

namespace Fonts
{
    static juce::Typeface::Ptr uiTf, titleTf;
    static void init()
    {
        if (uiTf == nullptr)    uiTf    = juce::Typeface::createSystemTypefaceFor (BinaryData::font_ui_ttf, (size_t) BinaryData::font_ui_ttfSize);
        if (titleTf == nullptr) titleTf = juce::Typeface::createSystemTypefaceFor (BinaryData::font_title_ttf, (size_t) BinaryData::font_title_ttfSize);
    }
    juce::Font ui (float h)    { init(); return juce::Font (uiTf).withHeight (h); }
    juce::Font title (float h) { init(); return juce::Font (titleTf).withHeight (h); }
}

// ---------------------------------------------------------------- knobs
LidKnob::LidKnob (juce::Image sprite, Fmt f, const ThemeColors* t) : src (std::move (sprite)), fmt (std::move (f)), th (t)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (220);
}

void LidKnob::resized()
{
    const int d = juce::jmin (getWidth(), getHeight()) - 82;
    if (src.isValid() && d > 10) scaled = src.rescaled (d, d, juce::Graphics::highResamplingQuality);
}

void LidKnob::mouseUp (const juce::MouseEvent& e)
{
    juce::Slider::mouseUp (e);
    if (! e.mouseWasDraggedSinceMouseDown() && onClick) onClick();
}

void LidKnob::paint (juce::Graphics& g)
{
    auto c = getLocalBounds().toFloat().getCentre();
    const float rad = scaled.isValid() ? scaled.getWidth() * 0.5f : 100.f;
    const float p = (float) valueToProportionOfLength (getValue());
    const float a0 = juce::degreesToRadians (-135.f), a1 = juce::degreesToRadians (135.f), a = a0 + p * (a1 - a0);
    const float rr = rad + 14.f;

    g.setGradientFill (juce::ColourGradient (th->a.withAlpha (0.2f + 0.3f * p), c.x, c.y, th->a.withAlpha (0.f), c.x + rr + 30, c.y, true));
    g.fillEllipse (c.x - rr - 30, c.y - rr - 30, (rr + 30) * 2, (rr + 30) * 2);

    // круговой спектр вокруг крышки
    for (int j = 0; j < 32; ++j)
    {
        const float tt = j / 31.f, v = bands[j], len = 2.f + v * 17.f;
        for (int side = 0; side < 2; ++side)
        {
            const float ang = side == 0 ? kPi - tt * kPi : kPi + tt * kPi;
            const float s = std::sin (ang), co = -std::cos (ang), r0 = rad + 26.f;
            g.setColour (th->a.interpolatedWith (th->a2, tt).withAlpha (0.35f + 0.65f * v));
            g.drawLine (c.x + r0 * s, c.y + r0 * co, c.x + (r0 + len) * s, c.y + (r0 + len) * co, 3.4f);
        }
    }
    juce::Path track, val;
    track.addCentredArc (c.x, c.y, rr, rr, 0, a0, a1, true);
    val.addCentredArc   (c.x, c.y, rr, rr, 0, a0, a,  true);
    g.setColour (juce::Colour (0xff04100a));
    g.strokePath (track, juce::PathStrokeType (10.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (th->a2, c.x - rr, c.y, th->a, c.x + rr, c.y, false));
    g.strokePath (val, juce::PathStrokeType (7.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (scaled.isValid())
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - rad + 4, c.y - rad + 8, rad * 2, rad * 2);
        g.setColour (juce::Colours::white);
        g.drawImageTransformed (scaled, juce::AffineTransform::translation (c.x - rad, c.y - rad).followedBy (juce::AffineTransform::rotation (a, c.x, c.y)));
    }
    juce::Rectangle<float> pill (c.x - 54, c.y + rad * 0.55f, 108, 24);
    g.setColour (juce::Colours::black.withAlpha (0.75f)); g.fillRoundedRectangle (pill, 12.f);
    g.setColour (th->a); g.drawRoundedRectangle (pill, 12.f, 1.5f);
    g.setColour (juce::Colours::white); g.setFont (Fonts::ui (14.f));
    g.drawFittedText (RU ("ГАЗ ") + fmt (getValue()), pill.toNearestInt(), juce::Justification::centred, 1);
}

MiniKnob::MiniKnob (juce::String l, juce::Image sprite, Fmt f, const ThemeColors* t)
    : label (std::move (l)), src (std::move (sprite)), fmt (std::move (f)), th (t)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setMouseDragSensitivity (150);
}

void MiniKnob::resized()
{
    const int d = juce::jmin (getWidth(), getHeight() - 30) - 10;
    if (src.isValid() && d > 8) scaled = src.rescaled (d, d, juce::Graphics::highResamplingQuality);
}

void MiniKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto tx = b.removeFromBottom (30.f);
    auto c = b.getCentre();
    const float rad = scaled.isValid() ? scaled.getWidth() * 0.5f : 20.f;
    const float p = (float) valueToProportionOfLength (getValue());
    const float a0 = juce::degreesToRadians (-135.f), a1 = juce::degreesToRadians (135.f), a = a0 + p * (a1 - a0);
    juce::Path track, val;
    track.addCentredArc (c.x, c.y, rad + 4, rad + 4, 0, a0, a1, true);
    val.addCentredArc   (c.x, c.y, rad + 4, rad + 4, 0, a0, a,  true);
    g.setColour (juce::Colour (0xff0a1a0f)); g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (th->a); g.strokePath (val, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (scaled.isValid())
    {
        g.setColour (juce::Colours::white);
        g.drawImageTransformed (scaled, juce::AffineTransform::translation (c.x - rad, c.y - rad).followedBy (juce::AffineTransform::rotation (a, c.x, c.y)));
    }
    g.setFont (Fonts::ui (10.5f));
    g.setColour (juce::Colours::white); g.drawFittedText (label, tx.removeFromTop (15.f).toNearestInt(), juce::Justification::centred, 1);
    g.setColour (th->a); g.drawFittedText (fmt (getValue()), tx.toNearestInt(), juce::Justification::centred, 1);
}

ModeCan::ModeCan (juce::String n, juce::Image sprite, juce::Colour glow) : label (std::move (n)), src (std::move (sprite)), glowC (glow)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ModeCan::resized()
{
    if (! src.isValid()) return;
    const int h = juce::jmax (10, getHeight() - 28);
    const int w = juce::roundToInt (src.getWidth() * (float) h / (float) src.getHeight());
    scaled = src.rescaled (w, h, juce::Graphics::highResamplingQuality);
}

void ModeCan::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto txt = b.removeFromBottom (28.f);
    if (scaled.isValid())
    {
        const int w = scaled.getWidth(), h = scaled.getHeight();
        const int x = (int) (b.getCentreX() - w * 0.5f), y = (int) (b.getBottom() - h);
        if (selected)
        {
            g.setGradientFill (juce::ColourGradient (glowC.withAlpha (0.8f), b.getCentreX(), y + h * 0.5f, glowC.withAlpha (0.f), b.getCentreX() + w * 1.3f, y + h * 0.5f, true));
            g.fillEllipse (b.getCentreX() - w * 1.3f, y + h * 0.5f - w * 1.3f, w * 2.6f, w * 2.6f);
        }
        g.setColour (juce::Colours::white.withAlpha (selected ? 1.f : 0.45f));
        g.drawImageAt (scaled, x, y);
    }
    g.setFont (Fonts::ui (14.f));
    g.setColour (selected ? glowC.brighter (0.5f) : juce::Colours::white.withAlpha (0.55f));
    g.drawFittedText (label, txt.toNearestInt(), juce::Justification::centred, 1);
}

void TubeMeter::tick (float rate)
{
    auto b = getLocalBounds().toFloat().reduced (4.f);
    const float top = b.getBottom() - b.getHeight() * level;
    const float n = rate / 30.f; int c = (int) n;
    if (rng.nextFloat() < n - (float) c) ++c;
    for (int i = 0; i < c && bubbles.size() < 260; ++i)
        bubbles.push_back ({ b.getX() + 5.f + rng.nextFloat() * (b.getWidth() - 10.f), b.getBottom() - 3.f,
                             1.5f + rng.nextFloat() * 3.8f, 1.2f + rng.nextFloat() * 2.2f + rate * 0.01f, rng.nextFloat() * 6.28f });
    for (auto& bb : bubbles) { bb.y -= bb.vy; bb.ph += 0.2f; bb.x += std::sin (bb.ph) * 0.35f; }
    bubbles.erase (std::remove_if (bubbles.begin(), bubbles.end(), [top] (const Bubble& bb) { return bb.y < top + 2.f; }), bubbles.end());
    repaint();
}

void TubeMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (4.f);
    juce::Path pill; pill.addRoundedRectangle (b, b.getWidth() * 0.5f);
    g.setColour (th->a.withAlpha (0.3f)); g.strokePath (pill, juce::PathStrokeType (9.f));
    g.setColour (juce::Colours::white); g.fillPath (pill);
    g.saveState(); g.reduceClipRegion (pill);
    const float fh = b.getHeight() * level, top = b.getBottom() - fh;
    g.setGradientFill (juce::ColourGradient (th->liq1, 0, top, th->liq2, 0, b.getBottom(), false));
    g.fillRect (b.getX(), top, b.getWidth(), fh);
    g.setColour (juce::Colours::white.withAlpha (0.55f)); g.fillRect (b.getX(), top, b.getWidth(), 3.f);
    for (auto& bb : bubbles)
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillEllipse (bb.x - bb.r, bb.y - bb.r, bb.r * 2, bb.r * 2);
        g.setColour (juce::Colours::white.withAlpha (0.85f)); g.drawEllipse (bb.x - bb.r, bb.y - bb.r, bb.r * 2, bb.r * 2, 1.2f);
    }
    g.restoreState();
    g.setColour (juce::Colours::black); g.strokePath (pill, juce::PathStrokeType (2.5f));
}

// ---------------------------------------------------------------- editor
MiniKnob* GazyavaEditor::addKnob (const char* id, const char* label, Fmt f, juce::Rectangle<int> r, int pg, const char* tip)
{
    knobs.push_back (std::make_unique<MiniKnob> (RU (label), imgLid, std::move (f), &cur));
    auto* k = knobs.back().get();
    k->setBufferedToImage (true);
    k->setTooltip (RU (tip));
    addAndMakeVisible (k);
    k->setBounds (r);
    sliderAtts.push_back (std::make_unique<SA> (proc.apvts, id, *k));
    if (pg >= 0) pageComps[pg].push_back (k);
    return k;
}

void GazyavaEditor::addChoice (const char* id, std::vector<const char*> names, int cols, juce::Rectangle<int> area, int bw, int bh, int gap, int radio, int pg)
{
    auto grp = std::make_unique<ChoiceGroup>();
    auto* gp = grp.get();
    for (size_t i = 0; i < names.size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (RU (names[i]));
        b->setClickingTogglesState (true); b->setRadioGroupId (radio);
        b->setBounds (area.getX() + (int) (i % (size_t) cols) * (bw + gap), area.getY() + (int) (i / (size_t) cols) * (bh + gap), bw, bh);
        b->onClick = [gp, i] { if (gp->btns[i]->getToggleState()) gp->att->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (*b);
        allButtons.push_back (b.get());
        if (pg >= 0) pageComps[pg].push_back (b.get());
        gp->btns.push_back (std::move (b));
    }
    gp->att = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (id), [gp] (float v)
    { const int idx = juce::roundToInt (v); for (size_t j = 0; j < gp->btns.size(); ++j) gp->btns[j]->setToggleState ((int) j == idx, juce::dontSendNotification); });
    gp->att->sendInitialUpdate();
    groups.push_back (std::move (grp));
}

juce::TextButton* GazyavaEditor::addToggle (const char* id, const char* text, juce::Rectangle<int> r, int pg, const char* tip)
{
    miscButtons.push_back (std::make_unique<juce::TextButton> (RU (text)));
    auto* b = miscButtons.back().get();
    b->setClickingTogglesState (true); b->setBounds (r); b->setTooltip (RU (tip));
    addAndMakeVisible (b); allButtons.push_back (b);
    if (pg >= 0) pageComps[pg].push_back (b);
    buttonAtts.push_back (std::make_unique<BA> (proc.apvts, id, *b));
    return b;
}

GazyavaEditor::GazyavaEditor (GazyavaProcessor& p)
    : AudioProcessorEditor (&p), lnf (std::make_unique<GazLnf>()), proc (p), cur (kThemes[0]),
      imgBig (load (BinaryData::can_big_png, BinaryData::can_big_pngSize)),
      imgSpk (load (BinaryData::speaker_png, BinaryData::speaker_pngSize)),
      imgCatL (load (BinaryData::manul_left_jpg, BinaryData::manul_left_jpgSize)),
      imgCatR (load (BinaryData::manul_right_jpg, BinaryData::manul_right_jpgSize)),
      imgLid (load (BinaryData::lid_png, BinaryData::lid_pngSize)),
      lid (load (BinaryData::lid_png, BinaryData::lid_pngSize), [] (double v) { return pct (v); }, &cur),
      tubeL (&cur), tubeR (&cur)
{
    setLookAndFeel (lnf.get());
    const int cx = W / 2;
    addAndMakeVisible (lid); lid.setBounds (cx - 148, 82, 296, 296);
    lid.setTooltip (RU ("ГАЗ — главная ручка сатурации бас-бустера. Клик по крышке — открыть банку!"));
    aLid = std::make_unique<SA> (proc.apvts, "drive", lid);
    lid.onClick = [this]
    {
        for (int i = 0; i < 46; ++i)
        {
            const float ang = rng.nextFloat() * 2.f * kPi, sp = 2.f + rng.nextFloat() * 6.f;
            spawnFx (5, (float) W * 0.5f, 230.f, std::sin (ang) * sp, -std::cos (ang) * sp, 2.f + rng.nextFloat() * 4.f, 1.f);
        }
        rings.push_back ({ { (float) W * 0.5f, 230.f }, 40.f, 1.f });
    };
    addAndMakeVisible (tubeL); addAndMakeVisible (tubeR);
    tubeL.setBounds (284, 44, 60, 466); tubeR.setBounds (W - 344, 44, 60, 466);

    // левый блок: бас-бустер
    auto L = [] (int col, int row) { return juce::Rectangle<int> (360 + col * 90, 114 + row * 86, 90, 86); };
    addKnob ("boost",  "БУСТ",    [] (double v) { return db (v * 18.0); }, L (0, 0), -1, "Сила бас-буста (до +18 дБ): чем больше, тем жирнее и громче низ");
    addKnob ("sub",    "САБ",     pct, L (1, 0), -1, "Добавляет саб-октаву ниже баса");
    addKnob ("freq",   "ЧАСТОТА", hz, L (0, 1), -1, "Граница между басом и верхами: бас обрабатывается отдельно");
    addKnob ("tight",  "ТАЙТ",    [] (double v) { return hz (18.0 + v * 55.0); }, L (1, 1), -1, "Срезает инфразвук: бас становится плотнее и чище");
    addKnob ("crunch", "ХРУСТ",   pct, L (0, 2), -1, "Добавляет гармоники на верхах");
    addKnob ("ice",    "ЛЁД",     pct, L (1, 2), -1, "Тёмный холодный тон: срезает верх");
    // правый блок: выход
    auto R = [] (int col, int row) { return juce::Rectangle<int> (820 + col * 90, 114 + row * 86, 90, 86); };
    addKnob ("in",       "ВХОД",     db, R (0, 0), -1, "Входной уровень");
    addKnob ("width",    "ШИРИНА",   [] (double v) { return juce::String (juce::roundToInt (v * 200.0)) + "%"; }, R (1, 0), -1, "Ширина стерео для верхов (бас остаётся по центру)");
    addKnob ("overload", "ПЕРЕГРУЗ", pct, R (0, 1), -1, "Жёсткий клиппер: грязный перегруз");
    addKnob ("mix",      "МИКС",     pct, R (1, 1), -1, "Баланс чистого и обработанного сигнала");
    addKnob ("out",      "ВЫХОД",    db, R (0, 2), -1, "Выходная громкость");
    limBtn.setButtonText (RU ("ЛИМИТЕР")); monoBtn.setButtonText (RU ("МОНО-БАС"));
    limBtn.setBounds (913, 294, 84, 26); monoBtn.setBounds (913, 326, 84, 26);
    limBtn.setTooltip (RU ("Защитный лимитер на выходе")); monoBtn.setTooltip (RU ("Сводит бас в моно: чище и мощнее в клубе"));
    for (auto* bt : { &limBtn, &monoBtn }) { bt->setClickingTogglesState (true); addAndMakeVisible (bt); allButtons.push_back (bt); }
    buttonAtts.push_back (std::make_unique<BA> (proc.apvts, "limiter", limBtn)); buttonAtts.push_back (std::make_unique<BA> (proc.apvts, "monobass", monoBtn));

    // страницы
    auto C = [] (int col, int row) { return juce::Rectangle<int> (360 + col * 64, 600 + row * 98, 64, 96); };
    // 0: динамика
    addKnob ("cthr",   "ПОРОГ",    db, C (0, 0), 0, "Компрессор: порог срабатывания");
    addKnob ("cratio", "СТЕПЕНЬ",  [] (double v) { return juce::String (v, 1) + ":1"; }, C (1, 0), 0, "Компрессор: степень сжатия");
    addKnob ("catk",   "АТАКА",    [] (double v) { return juce::String (v, 1) + " ms"; }, C (2, 0), 0, "Компрессор: атака");
    addKnob ("crel",   "РЕЛИЗ",    ms0, C (3, 0), 0, "Компрессор: восстановление");
    addKnob ("cgain",  "МЕЙКАП",   db, C (4, 0), 0, "Компенсация громкости после сжатия");
    addKnob ("cmix",   "КОМП МИКС", pct, C (5, 0), 0, "Параллельное сжатие: 0 = компрессор выключен");
    addKnob ("sat",    "САТУР.",   pct, C (0, 1), 0, "Сатурация всего сигнала: тепло и плотность");
    addKnob ("dist",   "ДИСТОРШН", pct, C (1, 1), 0, "Дисторшн всего сигнала");
    addChoice ("sattype",  { "ЛАМПА", "ЛЕНТА", "ТРАНС" }, 3, { 500, 708, 0, 0 }, 78, 28, 6, 81, 0);
    addChoice ("disttype", { "КЛИП", "ФАЗЗ", "ФОЛД", "КРАШ" }, 4, { 500, 748, 0, 0 }, 78, 28, 6, 82, 0);
    // 1: пространство
    addKnob ("rsize",  "РАЗМЕР",   pct, C (0, 0), 1, "Реверб: размер комнаты");
    addKnob ("rdamp",  "ЗАТУХ.",   pct, C (1, 0), 1, "Реверб: затухание верха");
    addKnob ("rpre",   "ПРЕДЗАД.", ms0, C (2, 0), 1, "Реверб: предзадержка");
    addKnob ("rfizz",  "ФИЗЗ",     pct, C (3, 0), 1, "Газированный хвост: пузыри и шипение");
    addKnob ("rduck",  "ДАКИНГ",   pct, C (4, 0), 1, "Реверб приглушается, пока играет бас");
    addKnob ("rmix",   "РЕВ МИКС", pct, C (5, 0), 1, "Количество реверба: 0 = выключен");
    addKnob ("chrate", "ХОР РЕЙТ", [] (double v) { return juce::String (v, 1) + " Hz"; }, C (6, 0), 1, "Хорус: скорость");
    addKnob ("chdepth", "ХОР ГЛУБ.", pct, C (7, 0), 1, "Хорус: глубина");
    addKnob ("chmix",  "ХОР МИКС", pct, C (8, 0), 1, "Хорус: количество, 0 = выключен");
    addKnob ("dtime",  "ВРЕМЯ",    ms0, C (0, 1), 1, "Эхо: время задержки");
    addKnob ("dfb",    "ФИДБЭК",   pct, C (1, 1), 1, "Эхо: количество повторов");
    addKnob ("dtone",  "ТОН ЭХА",  pct, C (2, 1), 1, "Эхо: яркость повторов");
    addKnob ("dmix",   "ЭХО МИКС", pct, C (3, 1), 1, "Эхо: количество, 0 = выключено");
    addToggle ("dpp", "ПИНГ-ПОНГ", { 624, 732, 110, 28 }, 1, "Повторы прыгают между левым и правым каналом");
    // 2: тон + вобл
    addKnob ("eqlow",  "НИЗ",      db, C (0, 0), 2, "Эквалайзер: низ (полка на 90 Гц)");
    addKnob ("eqmid",  "СРЕДА",    db, C (1, 0), 2, "Эквалайзер: середина");
    addKnob ("eqmidf", "ЧАСТ. СР.", hz, C (2, 0), 2, "Эквалайзер: частота середины");
    addKnob ("eqhigh", "ВЕРХ",     db, C (3, 0), 2, "Эквалайзер: верх (полка на 8 кГц)");
    const char* divs[5] = { "1/1", "1/2", "1/4", "1/8", "1/16" };
    addKnob ("wcut",   "СРЕЗ",     [] (double v) { return hz (40.0 * std::pow (200.0, v)); }, C (0, 1), 2, "Вобл-фильтр: базовая частота среза");
    addKnob ("wres",   "РЕЗО",     pct, C (1, 1), 2, "Вобл-фильтр: резонанс");
    addKnob ("wdiv",   "ТЕМП ВОБЛ", [divs] (double v) { return juce::String (divs[juce::jlimit (0, 4, juce::roundToInt (v))]); }, C (2, 1), 2, "Скорость вобла в долях такта (синхрон с темпом проекта)");
    addKnob ("wdepth", "ГЛУБИНА",  pct, C (3, 1), 2, "Вобл-фильтр: глубина качания");
    addKnob ("wmix",   "ВОБЛ МИКС", pct, C (4, 1), 2, "Вобл: количество, 0 = выключен");
    addChoice ("wtype", { "ФНЧ", "ПОЛОСА", "ФВЧ" }, 3, { 694, 732, 0, 0 }, 74, 28, 4, 83, 2);
    // 3: ритм
    const char* pdivs[3] = { "1/4", "1/8", "1/16" };
    addKnob ("pamt",   "ПАМП",     pct, C (0, 0), 3, "Памп: качание громкости в такт (как сайдчейн от бочки)");
    addKnob ("pdiv",   "ТЕМП ПАМП", [pdivs] (double v) { return juce::String (pdivs[juce::jlimit (0, 2, juce::roundToInt (v))]); }, C (1, 0), 3, "Скорость пампа в долях такта");
    addKnob ("pcurve", "КРИВАЯ",   pct, C (2, 0), 3, "Форма пампа: от мягкой до резкой");
    addKnob ("trate",  "ТРЕМ. РЕЙТ", [] (double v) { return juce::String (v, 1) + " Hz"; }, C (4, 0), 3, "Тремоло: скорость");
    addKnob ("tdepth", "ТРЕМ. ГЛУБ.", pct, C (5, 0), 3, "Тремоло: глубина, 0 = выключено");
    addKnob ("gthr",   "ГЕЙТ",     [] (double v) { return v <= -79.5 ? RU ("выкл") : db (v); }, C (0, 1), 3, "Гейт: режет тишину ниже порога (минимум = выключен)");
    addKnob ("grel",   "ГЕЙТ РЕЛ.", ms0, C (1, 1), 3, "Гейт: время закрытия");

    // вкладки
    const char* tabs[4] = { "ДИНАМИКА", "ПРОСТРАНСТВО", "ТОН + ВОБЛ", "РИТМ" };
    for (int i = 0; i < 4; ++i)
    {
        pageBtn[i].setButtonText (RU (tabs[i])); pageBtn[i].setClickingTogglesState (true); pageBtn[i].setRadioGroupId (90);
        pageBtn[i].setBounds (360 + i * 160, 566, 156, 26);
        pageBtn[i].onClick = [this, i] { if (pageBtn[i].getToggleState()) setPage (i); };
        addAndMakeVisible (pageBtn[i]); allButtons.push_back (&pageBtn[i]);
    }
    pageBtn[0].setToggleState (true, juce::dontSendNotification);

    // пресеты
    prevBtn.setButtonText ("<"); nextBtn.setButtonText (">"); rndBtn.setButtonText (RU ("ГАЗНУТЬ")); bypBtn.setButtonText (RU ("БАЙПАС"));
    prevBtn.setBounds (360, 62, 30, 26); nextBtn.setBounds (642, 62, 30, 26); rndBtn.setBounds (686, 62, 118, 26); bypBtn.setBounds (816, 62, 100, 26);
    prevBtn.setTooltip (RU ("Предыдущий пресет")); nextBtn.setTooltip (RU ("Следующий пресет"));
    rndBtn.setTooltip (RU ("Случайный звук: бросить кости и посмотреть, что выйдет")); bypBtn.setTooltip (RU ("Выключить обработку (плавно)"));
    bypBtn.setClickingTogglesState (true);
    for (auto* bt : { &prevBtn, &nextBtn, &rndBtn, &bypBtn }) { addAndMakeVisible (bt); allButtons.push_back (bt); }
    buttonAtts.push_back (std::make_unique<BA> (proc.apvts, "bypass", bypBtn));
    prevBtn.onClick = [this] { const int n = GazyavaProcessor::getNumPresets(); presetIdx = (presetIdx + n - 1) % n; proc.applyPreset (presetIdx); repaint (360, 56, 640, 36); };
    nextBtn.onClick = [this] { presetIdx = (presetIdx + 1) % GazyavaProcessor::getNumPresets(); proc.applyPreset (presetIdx); repaint (360, 56, 640, 36); };
    rndBtn.onClick  = [this] { proc.randomize(); for (int i = 0; i < 30; ++i) spawnFx (5, 745.f, 75.f, (rng.nextFloat() - 0.5f) * 9.f, (rng.nextFloat() - 0.5f) * 9.f, 2.f + rng.nextFloat() * 3.f, 1.f); };

    // банки-режимы
    const char* names[5] = { "ЛАЙМ", "ТРОПИК", "ГРОМ", "СИНИЙ", "ЧЁРНЫЙ" };
    juce::Image sprites[5] = { load (BinaryData::can_green_png, BinaryData::can_green_pngSize), load (BinaryData::can_baja_png, BinaryData::can_baja_pngSize),
                               load (BinaryData::can_purple_png, BinaryData::can_purple_pngSize), load (BinaryData::can_liberty_png, BinaryData::can_liberty_pngSize),
                               load (BinaryData::can_black_png, BinaryData::can_black_pngSize) };
    modeAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("mode"), [this] (float v) { applyTheme (juce::jlimit (0, 4, juce::roundToInt (v))); });
    for (int i = 0; i < 5; ++i)
    {
        auto mc = std::make_unique<ModeCan> (RU (names[i]), sprites[i], kThemes[i].a);
        mc->setBufferedToImage (true);
        mc->setTooltip (RU (kModeDesc[i]));
        mc->onClick = [this, i] { modeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (*mc); mc->setBounds (380 + i * 120, 392, 120, 150);
        modeCans.push_back (std::move (mc));
    }
    modeAtt->sendInitialUpdate();
    cur = kThemes[curMode];
    for (int m = 0; m < 5; ++m) fxAlpha[m] = m == curMode ? 1.f : 0.f;
    setPage (0);
    setSize (W, H);
    startTimerHz (30);
}

GazyavaEditor::~GazyavaEditor() { stopTimer(); setLookAndFeel (nullptr); }

void GazyavaEditor::setPage (int p)
{
    page = p;
    for (int i = 0; i < 4; ++i) for (auto* c : pageComps[i]) c->setVisible (i == p);
    repaint();
}

void GazyavaEditor::applyTheme (int mode)
{
    const bool changed = mode != curMode;
    curMode = mode;
    for (size_t j = 0; j < modeCans.size(); ++j) modeCans[j]->setSelected ((int) j == mode);
    for (auto* bt : allButtons)
    {
        bt->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0a140e));
        bt->setColour (juce::TextButton::buttonOnColourId, kThemes[mode].a);
        bt->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        bt->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    }
    themeAnim = 26;   // пока тема плавно перекрашивается, перерисовываем кэшированные ручки
    if (changed)   // фишка: при смене банки — салют из искр в цвет новой темы и волна от крышки
    {
        for (int i = 0; i < 60; ++i)
        {
            const float ang = rng.nextFloat() * 2.f * kPi, sp = 3.f + rng.nextFloat() * 8.f;
            spawnFx (5, 440.f + 120.f * (float) mode, 470.f, std::sin (ang) * sp, -std::cos (ang) * sp, 2.f + rng.nextFloat() * 4.f, 1.f);
        }
        rings.push_back ({ { 440.f + 120.f * (float) mode, 470.f }, 20.f, 1.f });
    }
    repaint();
}

void GazyavaEditor::spawnFx (int kind, float x, float y, float vx, float vy, float s, float life)
{
    if (fx.size() < 260) fx.push_back ({ x, y, vx, vy, s, life, rng.nextFloat() * 6.28f, kind });
}

void GazyavaEditor::mouseDown (const juce::MouseEvent& e)
{
    const int side = catRectL.contains (e.getPosition()) ? 0 : (catRectR.contains (e.getPosition()) ? 1 : -1);
    if (side < 0) return;
    const float r = rng.nextFloat();
    proc.triggerMeow (r < 0.55f ? 0 : (r < 0.8f ? 1 : 2), 0.94f + rng.nextFloat() * 0.12f);   // мяу / вопль / шипение
    meowAnim[side] = 1.f;
}

void GazyavaEditor::mouseMove (const juce::MouseEvent& e)
{
    const bool onCat = catRectL.contains (e.getPosition()) || catRectR.contains (e.getPosition());
    setMouseCursor (onCat ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void GazyavaEditor::timerCallback()
{
    t += 1.f / 30.f;
    const auto& T = kThemes[curMode];
    auto mix = [] (juce::Colour a, juce::Colour b) { return a.interpolatedWith (b, 0.18f); };
    cur.a = mix (cur.a, T.a); cur.a2 = mix (cur.a2, T.a2); cur.liq1 = mix (cur.liq1, T.liq1); cur.liq2 = mix (cur.liq2, T.liq2);
    cur.bgTop = mix (cur.bgTop, T.bgTop); cur.bgBot = mix (cur.bgBot, T.bgBot);
    for (int m = 0; m < 5; ++m) fxAlpha[m] += ((m == curMode ? 1.f : 0.f) - fxAlpha[m]) * 0.12f;
    if (themeAnim > 0) { --themeAnim; for (auto& k : knobs) if (k->isVisible()) k->repaint(); }

    auto toMeter = [] (float g) { return juce::jlimit (0.f, 1.f, (juce::Decibels::gainToDecibels (g, -48.f) + 48.f) / 48.f); };
    lvIn  = juce::jmax (toMeter (proc.inLevel.exchange (0.f)),  lvIn  * 0.88f);
    lvOut = juce::jmax (toMeter (proc.outLevel.exchange (0.f)), lvOut * 0.88f);
    const float bl = juce::jlimit (0.f, 1.f, std::sqrt (proc.bassLevel.exchange (0.f) * 1.1f));
    const float prevEnv = bassEnv;
    bassEnv = juce::jmax (bl, bassEnv * 0.80f);
    const bool onset = bl > 0.35f && bl - prevEnv > 0.15f;

    // спектр
    proc.getScope (fftData.data(), 2048);
    std::fill (fftData.begin() + 2048, fftData.end(), 0.f);
    window.multiplyWithWindowingTable (fftData.data(), 2048);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const float binHz = (float) sr / 2048.f;
    for (int j = 0; j < 32; ++j)
    {
        const float f0 = 35.f * std::pow (400.f, j / 32.f), f1 = 35.f * std::pow (400.f, (j + 1) / 32.f);
        const int b0 = juce::jmax (1, (int) (f0 / binHz)), b1 = juce::jmax (b0 + 1, (int) (f1 / binHz));
        float mag = 0.f; for (int k = b0; k < juce::jmin (b1, 1023); ++k) mag = juce::jmax (mag, fftData[(size_t) k]);
        const float dbv = juce::Decibels::gainToDecibels (mag / 512.f, -80.f) + j * 0.7f;
        const float v = juce::jlimit (0.f, 1.f, (dbv + 62.f) / 62.f);
        bands[j] = v > bands[j] ? bands[j] + (v - bands[j]) * 0.6f : bands[j] * 0.88f;
    }
    lid.setBands (bands);

    const float rate = 6.f + proc.apvts.getRawParameterValue ("boost")->load() * 45.f + bassEnv * 140.f;
    tubeL.setLevel (0.16f + 0.84f * lvIn);  tubeR.setLevel (0.16f + 0.84f * lvOut);
    tubeL.tick (rate); tubeR.tick (rate);

    // тряска: чем сильнее бас, тем жёстче
    const float amp = std::pow (bassEnv, 1.4f) * 15.f;
    for (int i = 0; i < 2; ++i) { jx[i] = (rng.nextFloat() - 0.5f) * 2.f * amp; jy[i] = (rng.nextFloat() - 0.5f) * 2.f * amp; meowAnim[i] *= 0.86f; }
    tubeL.setTopLeftPosition (284 + (int) (jx[0] * 0.25f), 44 + (int) (jy[0] * 0.25f));
    tubeR.setTopLeftPosition (W - 344 + (int) (jx[1] * 0.25f), 44 + (int) (jy[1] * 0.25f));

    if (onset)
        for (int side = 0; side < 2; ++side)
        {
            auto r = side == 0 ? canL : canR;
            rings.push_back ({ { (float) r.getCentreX(), r.getY() + r.getHeight() * 0.40f }, 30.f, 1.f });
        }
    for (auto& rg : rings) { rg.r += 11.f; rg.life -= 0.045f; }
    rings.erase (std::remove_if (rings.begin(), rings.end(), [] (const Ring& rg) { return rg.life <= 0.f; }), rings.end());

    // молнии для темы "ГРОМ"
    boltCool -= 1.f / 30.f;
    if (fxAlpha[2] > 0.6f && onset && boltCool <= 0.f)
    {
        bolt.clear(); float x = 380.f + rng.nextFloat() * 600.f, y = 0.f; bolt.push_back ({ x, y });
        const float endY = 300.f + rng.nextFloat() * 260.f;
        while (y < endY) { y += 20.f + rng.nextFloat() * 24.f; x += (rng.nextFloat() - 0.5f) * 70.f; bolt.push_back ({ x, y }); }
        boltAlpha = 1.f; flash = 1.f; boltCool = 0.35f;
    }
    boltAlpha *= 0.82f; flash *= 0.8f;

    // частицы темы
    auto count = [this] (float r) { int c = (int) r; if (rng.nextFloat() < r - (float) c) ++c; return c; };
    if (fxAlpha[0] > 0.5f) for (int i = count (0.4f + bassEnv * 2.5f); i > 0; --i) spawnFx (0, 290.f + rng.nextFloat() * 780.f, (float) H - 6.f, 0.f, 1.2f + rng.nextFloat() * 2.4f, 2.f + rng.nextFloat() * 5.f, 1.f);
    if (fxAlpha[1] > 0.5f) for (int i = count (0.3f + bassEnv * 1.5f); i > 0; --i) spawnFx (1, rng.nextFloat() * W, 80.f + rng.nextFloat() * 680.f, (rng.nextFloat() - 0.5f) * 0.6f, -0.2f - rng.nextFloat() * 0.3f, 6.f + rng.nextFloat() * 14.f, 1.f);
    if (fxAlpha[2] > 0.5f) for (int i = count (0.15f + bassEnv * 2.f); i > 0; --i) { const float a = rng.nextFloat() * 2.f * kPi, sp = 3.f + rng.nextFloat() * 5.f; spawnFx (2, 300.f + rng.nextFloat() * 760.f, 60.f + rng.nextFloat() * 700.f, std::sin (a) * sp, -std::cos (a) * sp, 5.f + rng.nextFloat() * 8.f, 0.6f); }
    if (fxAlpha[3] > 0.5f) for (int i = count (0.7f + bassEnv * 1.5f); i > 0; --i) spawnFx (3, rng.nextFloat() * W, -6.f, 0.f, 0.6f + rng.nextFloat() * 1.4f, 1.5f + rng.nextFloat() * 3.f, 1.f);
    if (fxAlpha[4] > 0.5f) for (int i = count (0.5f + bassEnv * 3.f); i > 0; --i) spawnFx (4, 290.f + rng.nextFloat() * 780.f, (float) H, 0.f, 1.f + rng.nextFloat() * 2.2f, 1.5f + rng.nextFloat() * 3.f, 1.f);
    for (auto& f : fx)
    {
        f.ph += 0.15f;
        switch (f.kind)
        {
            case 0: f.y -= f.vy; f.x += std::sin (f.ph) * 0.6f; f.life -= 0.003f; break;
            case 1: f.x += f.vx; f.y += f.vy; f.life -= 0.006f; break;
            case 2: f.x += f.vx; f.y += f.vy; f.life -= 0.12f; break;
            case 3: f.y += f.vy; f.x += std::sin (f.ph) * 0.7f; f.life -= 0.0015f; break;
            case 4: f.y -= f.vy; f.x += std::sin (f.ph * 1.3f) * 0.8f; f.life -= 0.006f; break;
            default: f.x += f.vx; f.y += f.vy; f.vx *= 0.94f; f.vy *= 0.94f; f.life -= 0.03f; break;
        }
    }
    fx.erase (std::remove_if (fx.begin(), fx.end(), [] (const Fx& f) { return f.life <= 0.f || f.y < -20.f || f.y > (float) H + 20.f; }), fx.end());
    repaint();
}

void GazyavaEditor::drawSideCan (juce::Graphics& g, juce::Rectangle<int> r, int side)
{
    const float env = bassEnv, ox = jx[side], oy = jy[side];
    const float cx = r.getCentreX() + ox, cy = r.getY() + r.getHeight() * 0.40f + oy;
    const float ang = ox * 0.0016f;
    g.setGradientFill (juce::ColourGradient (cur.a.withAlpha (0.08f + 0.55f * env), (float) r.getCentreX(), (float) r.getCentreY(),
                                             juce::Colours::transparentBlack, r.getCentreX() + r.getWidth() * 1.0f, (float) r.getCentreY(), true));
    g.fillEllipse (r.toFloat().expanded (r.getWidth() * 0.5f, r.getHeight() * 0.15f));
    const auto canT = [&] (float dx, float dy) { return juce::AffineTransform::translation ((float) r.getX(), (float) r.getY())
                                                           .followedBy (juce::AffineTransform::rotation (ang, (float) r.getCentreX(), (float) r.getCentreY()))
                                                           .translated (dx, dy); };
    if (bigS.isValid())
    {
        if (env > 0.4f) { g.setColour (juce::Colours::white.withAlpha (0.28f * env)); g.drawImageTransformed (bigS, canT (-ox * 1.9f, -oy * 1.9f)); }   // «двоение» от вибрации
        g.setColour (juce::Colours::white); g.drawImageTransformed (bigS, canT (ox, oy));
    }
    if (spkS.isValid())
    {
        const float D = (float) spkS.getWidth(), sx = (rng.nextFloat() - 0.5f) * env * 5.f, sy = (rng.nextFloat() - 0.5f) * env * 5.f;
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - D * 0.5f + 3, cy - D * 0.5f + 6, D, D);
        g.setColour (juce::Colours::white); g.drawImageAt (spkS, (int) (cx - D * 0.5f + sx), (int) (cy - D * 0.5f + sy));
        g.saveState();
        juce::Path clip; clip.addEllipse (cx - D * 0.36f + sx, cy - D * 0.36f + sy, D * 0.72f, D * 0.72f);
        g.reduceClipRegion (clip);
        g.setColour (juce::Colours::white);
        g.drawImageTransformed (spkS, juce::AffineTransform::translation (-D * 0.5f, -D * 0.5f).scaled (1.f + env * 0.5f).translated (cx + sx, cy + sy));
        g.setColour (cur.a.withAlpha (env * 0.28f)); g.fillEllipse (cx - D * 0.36f, cy - D * 0.36f, D * 0.72f, D * 0.72f);
        g.restoreState();
        g.setColour (cur.a.withAlpha (0.12f + env * 0.88f));
        g.drawEllipse (cx - D * 0.5f + sx, cy - D * 0.5f + sy, D, D, 1.5f + env * 4.5f);
    }
}

void GazyavaEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (cur.bgTop, 0, 0, cur.bgBot, 0, (float) H, false)); g.fillAll();
    g.setGradientFill (juce::ColourGradient (cur.a.withAlpha (0.26f + 0.25f * bassEnv), W * 0.5f, 230, juce::Colours::transparentBlack, W * 0.5f, 700, true)); g.fillAll();

    if (fxAlpha[1] > 0.01f)   // ТРОПИК: волны
        for (int k = 0; k < 4; ++k)
        {
            juce::Path w;
            for (int x = 0; x <= W; x += 8)
            {
                const float y = (float) H - 70.f - k * 22.f + std::sin (x * 0.011f + t * (1.2f + k * 0.3f) + k) * (7.f + bassEnv * 20.f);
                if (x == 0) w.startNewSubPath ((float) x, y); else w.lineTo ((float) x, y);
            }
            g.setColour (cur.a.withAlpha (0.22f * fxAlpha[1] * (1.f - k * 0.2f))); g.strokePath (w, juce::PathStrokeType (2.5f));
        }

    for (auto& f : fx)
    {
        const float a = juce::jlimit (0.f, 1.f, f.life);
        switch (f.kind)
        {
            case 0: g.setColour (cur.a.withAlpha (0.4f * a)); g.drawEllipse (f.x - f.s, f.y - f.s, f.s * 2, f.s * 2, 1.3f); break;
            case 1: { const float s = f.s * (0.8f + 0.2f * std::sin (f.ph));
                      g.setGradientFill (juce::ColourGradient (cur.a2.withAlpha (0.3f * a), f.x, f.y, cur.a2.withAlpha (0.f), f.x + s, f.y, true));
                      g.fillEllipse (f.x - s, f.y - s, s * 2, s * 2); break; }
            case 2: g.setColour (juce::Colours::white.withAlpha (a)); g.drawLine (f.x, f.y, f.x - f.vx * 2.f, f.y - f.vy * 2.f, 2.f);
                    g.setColour (cur.a.withAlpha (0.6f * a)); g.drawLine (f.x, f.y, f.x - f.vx * 3.5f, f.y - f.vy * 3.5f, 4.f); break;
            case 3: g.setColour (juce::Colours::white.withAlpha (0.75f * a)); g.fillEllipse (f.x - f.s * 0.5f, f.y - f.s * 0.5f, f.s, f.s);
                    if (f.s > 3.f) { g.setColour (cur.a2.withAlpha (0.5f * a)); g.drawLine (f.x - f.s * 1.6f, f.y, f.x + f.s * 1.6f, f.y, 1.f); g.drawLine (f.x, f.y - f.s * 1.6f, f.x, f.y + f.s * 1.6f, 1.f); } break;
            case 4: { const float fl = 0.6f + 0.4f * std::sin (f.ph * 3.f);
                      g.setGradientFill (juce::ColourGradient (cur.a2.withAlpha (0.85f * a * fl), f.x, f.y, cur.a.withAlpha (0.f), f.x + f.s * 3.f, f.y, true));
                      g.fillEllipse (f.x - f.s * 3.f, f.y - f.s * 3.f, f.s * 6.f, f.s * 6.f); break; }
            default: g.setColour (cur.a.interpolatedWith (juce::Colours::white, 0.4f).withAlpha (a)); g.fillEllipse (f.x - f.s * 0.5f, f.y - f.s * 0.5f, f.s, f.s); break;
        }
    }
    if (flash > 0.02f) { g.setColour (juce::Colours::white.withAlpha (0.14f * flash)); g.fillAll(); }
    if (boltAlpha > 0.03f && bolt.size() > 1)
    {
        juce::Path bp; bp.startNewSubPath (bolt[0]); for (size_t i = 1; i < bolt.size(); ++i) bp.lineTo (bolt[i]);
        g.setColour (cur.a.withAlpha (0.45f * boltAlpha)); g.strokePath (bp, juce::PathStrokeType (9.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (boltAlpha)); g.strokePath (bp, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    drawSideCan (g, canL, 0);
    drawSideCan (g, canR, 1);
    for (auto& rg : rings) { g.setColour (cur.a.withAlpha (rg.life * 0.8f)); g.drawEllipse (rg.c.x - rg.r, rg.c.y - rg.r, rg.r * 2, rg.r * 2, 1.5f + rg.life * 3.f); }

    for (int i = 0; i < 2; ++i)
    {
        auto r = (i == 0 ? catRectL : catRectR).toFloat();
        const auto& img = i == 0 ? catLS : catRS;
        const float sc = 1.f + 0.09f * meowAnim[i] + 0.04f * bassEnv;
        juce::Path clip; clip.addRoundedRectangle (r, 16.f);
        g.setColour (cur.a.withAlpha (0.25f + 0.3f * bassEnv)); g.strokePath (clip, juce::PathStrokeType (9.f));
        g.saveState(); g.reduceClipRegion (clip);
        g.setColour (juce::Colours::white);
        if (img.isValid()) g.drawImage (img, r.withSizeKeepingCentre (r.getWidth() * sc, r.getHeight() * sc).translated (0.f, -bassEnv * 4.f), juce::RectanglePlacement::fillDestination);
        g.restoreState();
        g.setColour (cur.a); g.strokePath (clip, juce::PathStrokeType (2.5f));
    }

    // логотип
    g.saveState();
    g.addTransform (juce::AffineTransform::translation (jx[0] * 0.15f, jy[0] * 0.15f));
    g.setColour (cur.a.withAlpha (0.2f)); g.strokePath (titlePath, juce::PathStrokeType (20.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cur.a2); g.strokePath (titlePath, juce::PathStrokeType (9.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (4, 4));
    g.setColour (juce::Colours::black); g.strokePath (titlePath, juce::PathStrokeType (8.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::ColourGradient grad (cur.a, 400.f, 0.f, cur.a, 960.f, 0.f, false);
    const float pos = std::fmod (t * 0.3f, 1.6f) - 0.3f;
    grad.addColour (juce::jlimit (0.0, 1.0, (double) pos - 0.08), cur.a);
    grad.addColour (juce::jlimit (0.0, 1.0, (double) pos), juce::Colours::white);
    grad.addColour (juce::jlimit (0.0, 1.0, (double) pos + 0.08), cur.a);
    g.setGradientFill (grad); g.fillPath (titlePath);
    g.restoreState();

    // подписи и панели
    g.setFont (Fonts::ui (12.f)); g.setColour (cur.a);
    g.drawFittedText (RU ("БАС-БУСТЕР"), { 360, 94, 180, 18 }, juce::Justification::centred, 1);
    g.drawFittedText (RU ("ВЫХОД"), { 820, 94, 180, 18 }, juce::Justification::centred, 1);
    g.drawFittedText (RU ("РЕЖИМ БУСТЕРА — ТЫКАЙ В БАНКУ"), { 360, 376, 640, 16 }, juce::Justification::centred, 1);
    g.setFont (Fonts::ui (12.5f));
    g.drawFittedText (RU (kModeDesc[curMode]), { 360, 544, 640, 18 }, juce::Justification::centred, 1);
    juce::Rectangle<float> pr (360.f, 596.f, 640.f, 200.f);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (pr, 10.f);
    g.setColour (cur.a.withAlpha (0.5f)); g.drawRoundedRectangle (pr, 10.f, 1.3f);
    juce::Rectangle<float> pn (392.f, 62.f, 248.f, 26.f);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (pn, 8.f);
    g.setColour (cur.a.withAlpha (0.7f)); g.drawRoundedRectangle (pn, 8.f, 1.3f);
    g.setColour (juce::Colours::white); g.setFont (Fonts::ui (14.f));
    g.drawFittedText (GazyavaProcessor::getPresetName (presetIdx), pn.toNearestInt(), juce::Justification::centred, 1);
}

void GazyavaEditor::resized()
{
    const int cx = W / 2;
    canL = { 10, 40, 262, 478 }; canR = { W - 272, 40, 262, 478 };
    catRectL = { 10, 532, 262, 258 }; catRectR = { W - 272, 532, 262, 258 };
    bigS = imgBig.isValid() ? imgBig.rescaled (262, 478, juce::Graphics::highResamplingQuality) : juce::Image();
    const int D = (int) (canL.getWidth() * 0.80f);
    spkS = imgSpk.isValid() ? imgSpk.rescaled (D, D, juce::Graphics::highResamplingQuality) : juce::Image();
    catLS = imgCatL.isValid() ? imgCatL.rescaled (262, 258, juce::Graphics::highResamplingQuality) : juce::Image();
    catRS = imgCatR.isValid() ? imgCatR.rescaled (262, 258, juce::Graphics::highResamplingQuality) : juce::Image();

    juce::GlyphArrangement ga;
    ga.addFittedText (Fonts::title (56.f), RU ("#ГАЗЯВА"), 0, 0, 600, 62, juce::Justification::centred, 1, 1.f);
    titlePath.clear(); ga.createPath (titlePath);
    titlePath.applyTransform (juce::AffineTransform::translation ((float) cx - 300.f, -2.f));
}
