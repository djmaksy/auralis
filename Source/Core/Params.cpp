#include "Params.h"

namespace au
{
namespace
{
#define SL(id, key, grp, en, ru, mn, mx, def)  { P::id, key, grp, en, ru, mn, mx, def, Kind::Slider, "", "" }
#define TG(id, key, grp, en, ru, def)          { P::id, key, grp, en, ru, 0.f, 1.f, def, Kind::Toggle, "", "" }
#define CH(id, key, grp, en, ru, n, def, ce, cr) { P::id, key, grp, en, ru, 0.f, (float) (n - 1), def, Kind::Choice, ce, cr }

const ParamInfo table[] =
{
    TG (bgOn,        "bg.on",        "BACKGROUND", "On", "Вкл", 1.f),
    CH (bgMode,      "bg.mode",      "BACKGROUND", "Style", "Стиль", 5, 0.f,
        "Liquid lake|Nebulae|Flight|Northern lights|Retro grid",
        "Озеро-жидкость|Туманности|Полёт|Северное сияние|Ретро-сетка"),
    SL (bgIntensity, "bg.intensity", "BACKGROUND", "Intensity", "Яркость", 0.f, 1.f, 0.8f),
    SL (bgSpeed,     "bg.speed",     "BACKGROUND", "Speed", "Скорость", 0.f, 2.f, 0.5f),
    SL (bgHue,       "bg.hue",       "BACKGROUND", "Hue", "Тон", 0.f, 1.f, 0.6f),
    SL (bgSat,       "bg.sat",       "BACKGROUND", "Saturation", "Насыщенность", 0.f, 1.f, 0.7f),
    SL (bgReact,     "bg.react",     "BACKGROUND", "Reactivity", "Реакция", 0.f, 1.f, 0.6f),

    TG (spOn,        "sp.on",        "SPECTRUM", "On", "Вкл", 1.f),
    CH (spMode,      "sp.mode",      "SPECTRUM", "Shape", "Форма", 3, 0.f,
        "Polar halo|Neon line|Hex cells", "Полярное кольцо|Неоновая линия|Соты"),
    SL (spSize,      "sp.size",      "SPECTRUM", "Size", "Размер", 0.2f, 1.5f, 0.8f),
    SL (spThick,     "sp.thick",     "SPECTRUM", "Thickness", "Толщина", 0.f, 1.f, 0.5f),
    SL (spHue,       "sp.hue",       "SPECTRUM", "Hue", "Тон", 0.f, 1.f, 0.08f),
    SL (spSat,       "sp.sat",       "SPECTRUM", "Saturation", "Насыщенность", 0.f, 1.f, 0.85f),
    SL (spGain,      "sp.gain",      "SPECTRUM", "Gain", "Усиление", 0.f, 2.f, 1.f),

    TG (d3On,        "d3.on",        "3D", "On", "Вкл", 0.f),
    CH (d3Mode,      "d3.mode",      "3D", "Mode", "Режим", 5, 0.f,
        "Particle morph|Curl flow|Strange attractor|Spectrum terrain|Shattering sphere",
        "Морфинг частиц|Curl-поток|Странный аттрактор|Рельеф спектра|Разлом сферы"),
    SL (d3Count,     "d3.count",     "3D", "Density", "Плотность", 0.1f, 1.f, 0.5f),
    SL (d3Size,      "d3.size",      "3D", "Particle size", "Размер частиц", 0.2f, 3.f, 1.f),
    SL (d3Speed,     "d3.speed",     "3D", "Speed", "Скорость", 0.f, 2.f, 0.7f),
    SL (d3Hue,       "d3.hue",       "3D", "Hue", "Тон", 0.f, 1.f, 0.5f),
    SL (d3Sat,       "d3.sat",       "3D", "Saturation", "Насыщенность", 0.f, 1.f, 0.8f),
    SL (d3React,     "d3.react",     "3D", "Reactivity", "Реакция", 0.f, 1.f, 0.7f),
    SL (d3Cam,       "d3.cam",       "3D", "Camera orbit", "Облёт камеры", 0.f, 2.f, 0.5f),
    CH (d3Attr,      "d3.attr",      "3D", "Attractor", "Аттрактор", 3, 0.f, "Lorenz|Aizawa|Thomas", "Лоренц|Айзава|Томас"),

    TG (txOn,        "tx.on",        "TEXT", "On", "Вкл", 1.f),
    SL (txSize,      "tx.size",      "TEXT", "Size", "Размер", 0.2f, 1.5f, 0.8f),
    SL (txPosY,      "tx.posY",      "TEXT", "Vertical position", "Положение по вертикали", -0.45f, 0.45f, 0.f),
    SL (txHue,       "tx.hue",       "TEXT", "Word colour", "Цвет слов", 0.f, 1.f, 0.08f),
    SL (txSat,       "tx.sat",       "TEXT", "Word saturation", "Насыщ. слов", 0.f, 1.f, 0.15f),
    SL (txSungHue,   "tx.sungHue",   "TEXT", "Sung-word colour", "Цвет спетого", 0.f, 1.f, 0.08f),
    TG (txShowLyrics,"tx.lyrics",    "TEXT", "Show lyrics", "Текст песни", 1.f),
    SL (txBright,    "tx.bright",    "TEXT", "Brightness", "Яркость", 0.2f, 2.f, 1.f),
    SL (txPanel,     "tx.panel",     "TEXT", "Backing panel", "Подложка", 0.f, 1.f, 0.6f),
    CH (txAnim,      "tx.anim",      "TEXT", "Appearance", "Появление", 3, 0.f,
        "Particles fly in|Fade in|Instant", "Частицы слетаются|Плавно|Сразу"),

    TG (orOn,        "or.on",        "ORNAMENT", "On", "Вкл", 0.f),
    CH (orMode,      "or.mode",      "ORNAMENT", "Motif", "Мотив", 3, 0.f,
        "Arevakhach|Khachkar|Borjgali", "Аревахач|Хачкар|Борджгали"),
    SL (orSize,      "or.size",      "ORNAMENT", "Size", "Размер", 0.2f, 1.5f, 0.8f),
    SL (orHue,       "or.hue",       "ORNAMENT", "Hue", "Тон", 0.f, 1.f, 0.1f),
    SL (orSat,       "or.sat",       "ORNAMENT", "Saturation", "Насыщенность", 0.f, 1.f, 0.7f),
    SL (orReact,     "or.react",     "ORNAMENT", "Reactivity", "Реакция", 0.f, 1.f, 0.6f),
    SL (orSpin,      "or.spin",      "ORNAMENT", "Rotation", "Вращение", -1.f, 1.f, 0.2f),

    SL (opBloom,     "op.bloom",     "OPTICS", "Bloom", "Свечение", 0.f, 1.f, 0.5f),
    SL (opBokeh,     "op.bokeh",     "OPTICS", "Bokeh", "Боке", 0.f, 1.f, 0.2f),
    SL (opChroma,    "op.chroma",    "OPTICS", "Chromatic aberration", "Хроматизм", 0.f, 1.f, 0.1f),
    SL (opVignette,  "op.vignette",  "OPTICS", "Vignette", "Виньетка", 0.f, 1.f, 0.4f),
    SL (opExposure,  "op.exposure",  "OPTICS", "Exposure", "Экспозиция", 0.3f, 2.f, 1.f),

    TG (vdOn,        "vd.on",        "VIDEO", "On", "Вкл", 0.f),
    CH (vdMode,      "vd.mode",      "VIDEO", "Placement", "Положение", 2, 0.f, "Full frame|Framed", "Во весь кадр|В рамке"),
    SL (vdOpacity,   "vd.opacity",   "VIDEO", "Opacity", "Непрозрачность", 0.f, 1.f, 1.f),
    SL (vdBright,    "vd.bright",    "VIDEO", "Brightness", "Яркость", 0.f, 2.f, 1.f),
    SL (vdContrast,  "vd.contrast",  "VIDEO", "Contrast", "Контраст", 0.f, 2.f, 1.f),
    SL (vdSat,       "vd.sat",       "VIDEO", "Saturation", "Насыщенность", 0.f, 2.f, 1.f),
    SL (vdGrain,     "vd.grain",     "VIDEO", "Grain", "Зерно", 0.f, 1.f, 0.f),
    SL (vdScan,      "vd.scan",      "VIDEO", "Scanlines", "Строки", 0.f, 1.f, 0.f),
    SL (vdAberr,     "vd.aberr",     "VIDEO", "Aberration", "Аберрация", 0.f, 1.f, 0.f),
    SL (vdSlow,      "vd.slow",      "VIDEO", "Speed", "Скорость", 0.1f, 1.f, 1.f),
    SL (vdBeatJump,  "vd.beatjump",  "VIDEO", "Beat jumps", "Прыжки по битам", 0.f, 1.f, 0.f),
};
static_assert (sizeof (table) / sizeof (table[0]) == (size_t) kParamCount, "param table out of sync");
}

const ParamInfo& info (P p) { return table[(int) p]; }

const std::vector<std::string>& groups()
{
    static const std::vector<std::string> g { "BACKGROUND", "SPECTRUM", "3D", "TEXT", "ORNAMENT", "OPTICS", "VIDEO" };
    return g;
}

const char* groupLabel (const std::string& g, bool ru)
{
    if (g == "BACKGROUND") return ru ? "ФОН" : "BACKGROUND";
    if (g == "SPECTRUM")   return ru ? "СПЕКТР" : "SPECTRUM";
    if (g == "3D")         return "3D";
    if (g == "TEXT")       return ru ? "ТЕКСТ" : "TEXT";
    if (g == "ORNAMENT")   return ru ? "ОРНАМЕНТ" : "ORNAMENT";
    if (g == "OPTICS")     return ru ? "ОПТИКА" : "OPTICS";
    if (g == "VIDEO")      return ru ? "ВИДЕО" : "VIDEO";
    return g.c_str();
}

std::vector<std::string> splitChoices (const char* s)
{
    std::vector<std::string> out; std::string cur;
    for (const char* p = s; *p; ++p) { if (*p == '|') { out.push_back (cur); cur.clear(); } else cur += *p; }
    if (! cur.empty()) out.push_back (cur);
    return out;
}

int indexOfKey (const std::string& key)
{
    for (int i = 0; i < kParamCount; ++i) if (key == table[i].key) return i;
    return -1;
}

void Scene::resetToDefaults()
{
    for (int i = 0; i < kParamCount; ++i) v[(size_t) i] = table[i].def;
}
}
