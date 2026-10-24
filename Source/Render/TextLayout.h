#pragma once
#include "ParticleSims.h"
#include <string>
#include <vector>

namespace au
{
struct TextLayoutResult
{
    std::vector<TextTarget> targets;
    float box[4] { 0, 0, 0, 0 };   // minX, minY, maxX, maxY (short-side units, y up)
    float dotSize = 0.0035f;
};

// Lays the words out (wrapping, right-to-left for Arabic), rasterises each word with the bundled
// Noto fonts and samples it into particle targets tagged with the word index.
TextLayoutResult layoutWords (const std::vector<std::string>& words, float frameW, float frameH,
                              float sizeUnits, float posY, int maxTargets);
}
