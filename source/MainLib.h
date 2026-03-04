#pragma once
#include <windows.h>

#define PI 3.14159265358979323846f

// SAL Annotations
#ifndef _In_
#define _In_
#endif
#ifndef _In_opt_
#define _In_opt_
#endif
#ifndef _Inout_
#define _Inout_
#endif

VOID SeedXorshift32(_In_ DWORD dwSeed);
DWORD Xorshift32(VOID);
VOID Reflect2D(_Inout_ PINT x, _Inout_ PINT y, _In_ INT w, _In_ INT h);
VOID InitializeSine(VOID);

#include <accctrl.h>
#include <aclapi.h>
#include <stdio.h>
#include <math.h>
#include "def_copy.h"

#pragma region Public Variables
extern HWND hwndDesktop;
extern HDC hdcDesktop;
extern RECT rcScrBounds;
extern HHOOK hMsgHook;
extern INT nCounter;
#pragma endregion Public Variables


FLOAT WINAPI FastSine(_In_ FLOAT f);
FLOAT WINAPI FastCosine(_In_ FLOAT f);


