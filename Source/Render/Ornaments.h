#pragma once
#include <cstdint>

namespace au
{
// Traditional motifs drawn procedurally into an 8-bit mask (size x size, row 0 = bottom for GL).
// 0 = arevakhach (eight-armed eternity wheel), 1 = khachkar (cross-stone), 2 = borjgali (seven-armed sun).
void makeOrnamentMask (int motif, int size, uint8_t* dst);
}
