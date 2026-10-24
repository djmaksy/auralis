#include "Params.h"

namespace au
{
namespace
{
struct Row
{
    const char* en; const char* ru;
    int bg; float bgH;      // bg: -1 = off, 0..4 = mode
    int sp; float spH;      // sp: -1 = off, 0..2
    int d3; float d3H;      // d3: -1 = off, 0..4
    int orn; float orH;     // orn: -1 = off, 0..2
};

// 62 factory looks. Bg: 0 lake, 1 nebulae, 2 flight, 3 aurora, 4 retro grid.
const Row rows[] =
{
    // --- lakes ---
    { "Still Water",      "Тихая вода",          0, .58f, 0, .08f, -1, 0, -1, 0 },
    { "Moonlit Lake",     "Лунное озеро",         0, .62f, 1, .55f, -1, 0, -1, 0 },
    { "Amber Lake",       "Янтарное озеро",       0, .09f, 0, .11f, -1, 0, -1, 0 },
    { "Deep Water",       "Глубина",              0, .55f, 2, .50f, -1, 0, -1, 0 },
    { "Mist at Dawn",     "Туман на рассвете",    0, .95f, 0, .93f, -1, 0, -1, 0 },
    { "Glass Pool",       "Стеклянный пруд",      0, .50f, 0, .45f, -1, 0, -1, 0 },
    { "Storm Lake",       "Грозовое озеро",       0, .70f, 1, .75f, -1, 0, -1, 0 },
    { "Lake and Cross",   "Озеро и крест",        0, .58f, 0, .10f, -1, 0,  1, .09f },
    // --- nebulae ---
    { "Nebula Bloom",     "Цветение туманности",  1, .80f, 0, .90f, -1, 0, -1, 0 },
    { "Violet Cloud",     "Фиолетовое облако",    1, .76f, 0, .60f, -1, 0, -1, 0 },
    { "Crimson Nebula",   "Багровая туманность",  1, .00f, 1, .08f, -1, 0, -1, 0 },
    { "Cosmic Dust",      "Космическая пыль",     1, .10f, 2, .50f, -1, 0, -1, 0 },
    { "Blue Giant",       "Голубой гигант",       1, .60f, 0, .50f, -1, 0, -1, 0 },
    { "Emerald Veil",     "Изумрудная вуаль",     1, .38f, 1, .45f, -1, 0, -1, 0 },
    { "Nebula Core",      "Ядро туманности",      1, .85f, 0, .85f,  0, .85f, -1, 0 },
    { "Star Nursery",     "Звёздные ясли",        1, .05f, 0, .08f,  1, .10f, -1, 0 },
    // --- flight ---
    { "Hyperspace",       "Гиперпространство",    2, .58f, 0, .50f, -1, 0, -1, 0 },
    { "Warp Tunnel",      "Варп-туннель",         2, .75f, 2, .80f, -1, 0, -1, 0 },
    { "Star Run",         "Звёздный бег",         2, .10f, 1, .10f, -1, 0, -1, 0 },
    { "Light Speed",      "Скорость света",       2, .50f, 1, .55f, -1, 0, -1, 0 },
    { "Cosmic Drift",     "Космический дрейф",    2, .60f, -1, 0,    1, .60f, -1, 0 },
    { "Red Comet",        "Красная комета",       2, .00f, 0, .04f, -1, 0, -1, 0 },
    // --- aurora ---
    { "Northern Lights",  "Северное сияние",      3, .38f, 1, .33f, -1, 0, -1, 0 },
    { "Polar Night",      "Полярная ночь",        3, .50f, 0, .55f, -1, 0, -1, 0 },
    { "Violet Aurora",    "Фиолетовое сияние",    3, .78f, 0, .88f, -1, 0, -1, 0 },
    { "Emerald Sky",      "Изумрудное небо",      3, .33f, 2, .30f, -1, 0, -1, 0 },
    { "Aurora Ring",      "Кольцо сияния",        3, .42f, 0, .45f, -1, 0, -1, 0 },
    { "Frozen Sky",       "Ледяное небо",         3, .55f, -1, 0,    0, .55f, -1, 0 },
    // --- retro ---
    { "Sunset Drive",     "Закатная трасса",      4, .88f, 1, .08f, -1, 0, -1, 0 },
    { "Neon Horizon",     "Неоновый горизонт",    4, .80f, 2, .50f, -1, 0, -1, 0 },
    { "Synth Grid",       "Синт-сетка",           4, .76f, 1, .88f, -1, 0, -1, 0 },
    { "Night Boulevard",  "Ночной бульвар",       4, .90f, 0, .50f, -1, 0, -1, 0 },
    { "Cyan Grid",        "Бирюзовая сетка",      4, .50f, 1, .93f, -1, 0, -1, 0 },
    { "Amber Grid",       "Янтарная сетка",       4, .09f, 2, .12f, -1, 0, -1, 0 },
    // --- 3D ---
    { "Neon Blocks",      "Неоновые кварталы",       4, .80f, -1, 0,    3, .85f, -1, 0 },
    { "Sound Terrain",    "Ландшафт звука",       1, .60f, -1, 0,    3, .55f, -1, 0 },
    { "Lava Terrain",     "Лавовый рельеф",       1, .00f, -1, 0,    3, .06f, -1, 0 },
    { "Butterfly Orbit",  "Орбита бабочки",       1, .72f, 0, .05f,  2, .02f, -1, 0 },
    { "Strange Bloom",    "Странный цветок",      1, .85f, -1, 0,    2, .90f, -1, 0 },
    { "Teal Attractor",   "Бирюзовый аттрактор",  2, .50f, -1, 0,    2, .48f, -1, 0 },
    { "Golden Orbit",     "Золотая орбита",       1, .10f, -1, 0,    2, .12f, -1, 0 },
    { "Curl Storm",       "Вихревой шторм",       1, .60f, -1, 0,    1, .55f, -1, 0 },
    { "Silk Flow",        "Шёлковый поток",       0, .70f, -1, 0,    1, .80f, -1, 0 },
    { "Fire Flow",        "Огненный поток",       1, .02f, -1, 0,    1, .06f, -1, 0 },
    { "Ice Flow",         "Ледяной поток",        3, .50f, -1, 0,    1, .52f, -1, 0 },
    { "Sphere Fracture",  "Разлом сферы",         1, .60f, -1, 0,    4, .60f, -1, 0 },
    { "Crystal Burst",    "Кристальный взрыв",    2, .50f, -1, 0,    4, .50f, -1, 0 },
    { "Solar Shatter",    "Солнечный разлом",     1, .08f, -1, 0,    4, .10f, -1, 0 },
    { "Morph Garden",     "Сад морфинга",         1, .80f, -1, 0,    0, .88f, -1, 0 },
    { "Particle Dream",   "Сон частиц",           0, .60f, -1, 0,    0, .60f, -1, 0 },
    { "Gold Dust",        "Золотая пыль",         1, .10f, -1, 0,    0, .12f, -1, 0 },
    { "Plasma Heart",     "Сердце плазмы",        1, .95f, 0, .90f,  0, .95f, -1, 0 },
    // --- ornaments ---
    { "Eternity Wheel",   "Колесо вечности",      1, .06f, 0, .10f, -1, 0,  0, .09f },
    { "Eternity Sun",     "Солнце вечности",      0, .08f, -1, 0,   -1, 0,  0, .10f },
    { "Stone Cross",      "Каменный крест",       1, .08f, -1, 0,   -1, 0,  1, .09f },
    { "Carved Light",     "Резной свет",          1, .07f, 1, .08f, -1, 0,  1, .08f },
    { "Seven Rays",       "Семь лучей",           1, .02f, -1, 0,   -1, 0,  2, .05f },
    { "Seven Suns",       "Семь солнц",           0, .09f, 0, .10f, -1, 0,  2, .10f },
    { "Highland Dawn",    "Горный рассвет",       0, .06f, -1, 0,   -1, 0,  2, .06f },
    { "Silk Road",        "Шёлковый путь",        1, .85f, -1, 0,    0, .13f, 0, .13f },
    // --- misc ---
    { "Bokeh Lights",     "Огни боке",            1, .90f, 0, .93f, -1, 0, -1, 0 },
    { "Quiet Pulse",      "Тихий пульс",         -1, 0,    0, .00f, -1, 0, -1, 0 },
};
static_assert (sizeof (rows) / sizeof (rows[0]) == 62, "need exactly 62 presets");
}

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> all = []
    {
        std::vector<Preset> out;
        int i = 0;
        for (const auto& r : rows)
        {
            Preset p; p.en = r.en; p.ru = r.ru;
            Scene& s = p.scene;

            s[P::bgOn] = r.bg >= 0 ? 1.f : 0.f;
            if (r.bg >= 0) { s[P::bgMode] = (float) r.bg; s[P::bgHue] = r.bgH; }

            s[P::spOn] = r.sp >= 0 ? 1.f : 0.f;
            if (r.sp >= 0) { s[P::spMode] = (float) r.sp; s[P::spHue] = r.spH; }
            if (r.sp == 0 && r.d3 >= 0) s[P::spSize] = 1.1f;

            s[P::d3On] = r.d3 >= 0 ? 1.f : 0.f;
            if (r.d3 >= 0)
            {
                s[P::d3Mode] = (float) r.d3; s[P::d3Hue] = r.d3H;
                s[P::d3Count] = r.d3 == 2 ? 0.7f : 0.55f;
                s[P::d3Size] = r.d3 == 4 ? 1.4f : 1.f;
            }

            s[P::orOn] = r.orn >= 0 ? 1.f : 0.f;
            if (r.orn >= 0) { s[P::orMode] = (float) r.orn; s[P::orHue] = r.orH; }

            const float textHue = r.sp >= 0 ? r.spH : (r.d3 >= 0 ? r.d3H : (r.orn >= 0 ? r.orH : r.bgH));
            s[P::txHue] = textHue; s[P::txSungHue] = textHue;
            s[P::txSize] = (r.orn >= 0 || r.d3 == 3) ? 0.55f : 0.8f;

            s[P::bgSpeed]  = 0.35f + 0.08f * (float) (i % 6);
            s[P::opBloom]  = 0.35f + 0.08f * (float) (i % 5);
            s[P::opBokeh]  = (i % 4 == 0) ? 0.5f : 0.15f;
            s[P::opChroma] = 0.05f + 0.04f * (float) (i % 3);
            s[P::d3Speed]  = 0.5f + 0.1f * (float) (i % 5);

            if (std::string (r.en) == "Bokeh Lights") { s[P::opBokeh] = 1.f; s[P::opBloom] = 0.7f; }
            if (std::string (r.en) == "Quiet Pulse")  { s[P::spSat] = 0.f; s[P::spThick] = 0.3f; s[P::opBloom] = 0.25f; s[P::txSat] = 0.f; }
            out.push_back (std::move (p));
            ++i;
        }
        return out;
    }();
    return all;
}
}
