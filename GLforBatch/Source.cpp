#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>

#include <stdio.h>
#include <string>
#include <sstream>
#include <unordered_map>

#include <iostream>

//#include "glad.h"
#include <gl/GL.h>

typedef const GLubyte*(* PFNGLGETSTRINGPROC)(GLenum name);

//using PFNGLGETSTRINGPROC = const GLubyte(*APIENTRYP)(GLenum name);

#pragma comment (lib, "opengl32")

#define MAXLEN 512

#define WM_GLCOMMAND (WM_USER+0)

struct WindowData {
    HWND hWnd;
    HDC hDC;
    HGLRC hGLRC;
};

struct GlData {
    void* ptr;
    char* arguments;
};

[[gnu::noinline]] uint32_t fnvhash(char const* str, bool lowercase = false) {
    uint32_t hash = 0;
    char c, i = 0; // lets not hash strings > 256 chars
    while (c = str[i++]) {
        hash = (hash * 0x811C9DC5) ^ ((lowercase) ? c | 0x20 : c);
    }
    return hash;
}

consteval uint32_t consthash(char const* str, bool removeCases = false) {
    uint32_t hash = 0, i = 0;
    char c = 0;
    while (c = str[i]) {
        if (removeCases && 'A' <= c && c <= 'Z') c += 'a' - 'A';
        hash = (hash * 0x811C9DC5) ^ c;
        i++;
    }
    return hash;
}

char* itoa_(int i) {
    static char buffer[21] = { 0 };

    char* c = buffer + 19; // buffer[20] must be \0
    int x = abs(i);

    do {
        *--c = 48 + x % 10;
    } while (x && (x /= 10));

    if (i < 0) *--c = 45;
    return c;
}

std::unordered_map<uint32_t, void*> funcMap = {
    {consthash("glGetString"), glGetString}
};

WindowData wd;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uint, WPARAM wParam, LPARAM lParam) {
    if (uint == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
        EndPaint(hwnd, &ps);
    }
    else if (uint == WM_GLCOMMAND) {
        GlData* d = (GlData*)lParam;
        MessageBox(wd.hWnd, (LPCSTR)((PFNGLGETSTRINGPROC)d->ptr)(atoi(d->arguments)), "version", 0);
    }
    else if (uint == WM_PAINT) {

    }
    else {
        return DefWindowProc(hwnd, uint, wParam, lParam);
    }

}

DWORD CALLBACK InitGL(void *unused)
{
    /* REGISTER WINDOW */
    WNDCLASS window_class;

    // Clear all structure fields to zero first
    ZeroMemory(&window_class, sizeof(window_class));

    // Define fields we need (others will be zero)
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = WindowProc; // To be introduced later
    window_class.hInstance = GetModuleHandle(0);
    window_class.lpszClassName = TEXT("OPENGL_WINDOW");

    // Give our class to Windows
    RegisterClass(&window_class);
    /* CREATE WINDOW */
    wd.hWnd = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW,
        TEXT("OPENGL_WINDOW"),
        TEXT("OpenGL window"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL,
        NULL,
        GetModuleHandle(0),
        NULL);

    wd.hDC = GetDC(wd.hWnd);

    ShowWindow(wd.hWnd, SW_SHOW);

    /* PIXEL FORMAT */
    PIXELFORMATDESCRIPTOR descriptor;

    // Clear all structure fields to zero first
    ZeroMemory(&descriptor, sizeof(descriptor));

    // Describe our pixel format
    descriptor.nSize = sizeof(descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL | PFD_GENERIC_ACCELERATED | PFD_DOUBLEBUFFER | PFD_SWAP_LAYER_BUFFERS;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = 32;
    descriptor.cRedBits = 8;
    descriptor.cGreenBits = 8;
    descriptor.cBlueBits = 8;
    descriptor.cAlphaBits = 8;
    descriptor.cDepthBits = 32;
    descriptor.cStencilBits = 8;

    // Ask for a similar supported format and set 
    SetPixelFormat(wd.hDC, ChoosePixelFormat(wd.hDC, &descriptor), &descriptor);

    wd.hGLRC = wglCreateContext(wd.hDC);
    wglMakeCurrent(wd.hDC, wd.hGLRC);

    MSG msg;
    while (GetMessage(&msg, wd.hWnd, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

int main(void) {
    CreateThread(NULL, 0, InitGL, NULL, 0, NULL);

    HANDLE hPipe;
    DWORD dwRead;

    std::string s, word;
    s.reserve(MAXLEN);
    word.reserve(128);
    char word1[128];
    char line[512];

    char *buffer = s.data();

    hPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\GLpipe"),
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, // | FILE_FLAG_FIRST_PIPE_INSTANCE,
        1,
        MAXLEN,
        MAXLEN,
        NMPWAIT_USE_DEFAULT_WAIT,
        NULL);

    if (hPipe == INVALID_HANDLE_VALUE) {
        return -1;
    }

    while (hPipe != INVALID_HANDLE_VALUE) {
        if (ConnectNamedPipe(hPipe, NULL) != FALSE) {   // wait for someone to connect to the pipe
            while (ReadFile(hPipe, buffer, MAXLEN - 1, &dwRead, NULL) != FALSE) {
                /* add terminating zero */
                buffer[dwRead] = '\0';

                sscanf(buffer, "%s ", word1);

                //std::cout << std::string_view(s).substr(0, s.find(' ')) << std::endl;
                printf("read: %d ;; buffer:%s ;; calling function: %s\n", dwRead, buffer, word1);

                void* ptr;

                try {
                    ptr = funcMap.at(fnvhash(word1));
                } catch (...) {
                    break;
                }

                GlData gd = {
                    .ptr = ptr,
                    .arguments = itoa_(GL_VERSION)
                };
                SendMessage(wd.hWnd, WM_GLCOMMAND, 0, (LPARAM)&gd);
            }
        }

        DisconnectNamedPipe(hPipe);
    }

    return 0;
}
