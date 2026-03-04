#include "MainLib.h"
#include <math.h>

static FLOAT pfSinVals[4096];
static DWORD xs;

VOID SeedXorshift32(DWORD dwSeed)
{
    xs = dwSeed;
}

DWORD Xorshift32(VOID)
{
    xs ^= xs << 13;
    xs ^= xs >> 17;
    xs ^= xs << 5;
    return xs;
}

VOID Reflect2D(PINT x, PINT y, INT w, INT h)
{
#define F(v, maxv) (abs(v) / (maxv) % 2 ? (maxv) - abs(v) % (maxv) : abs(v) % (maxv))
    *x = F(*x, w - 1);
    *y = F(*y, h - 1);
#undef F
}

FLOAT WINAPI FastSine(FLOAT f)
{
    INT i = (INT)(f / (2.f * PI) * 4096.f);
    return pfSinVals[i & 4095];
}

FLOAT WINAPI FastCosine(FLOAT f)
{
    return FastSine(f + PI / 2.f);
}

VOID InitializeSine(VOID)
{
    for (INT i = 0; i < 4096; i++)
        pfSinVals[i] = sinf((FLOAT)i / 4096.f * PI * 2.f);
}
