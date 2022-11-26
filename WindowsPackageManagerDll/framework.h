#pragma once

#define DllExport   __declspec( dllexport )
#define WIN32_LEAN_AND_MEAN             // Wyklucz rzadko używane rzeczy z nagłówków systemu Windows
// Pliki nagłówkowe systemu Windows
#include <windows.h>

DllExport HRESULT __cdecl StartWindowsPackageManagerLogging();
DllExport HRESULT __cdecl StopWindowsPackageManagerLogging();
DllExport HRESULT __cdecl GetWindowsPackageManagerLog(LPWSTR buf, int maxCharacters);
