// dllmain.cpp : Definiuje punkt wejścia dla aplikacji DLL.
#include "pch.h"
#include "fltuser.h"
#include "strsafe.h"

#define MAX_MESSAGE_CHARS 2048

typedef struct _MESSAGE {
    FILTER_MESSAGE_HEADER header;
    WCHAR string[MAX_MESSAGE_CHARS];
} MESSAGE, * PMESSAGE;

HANDLE      port = NULL;
MESSAGE     message;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    HRESULT result;
    ZeroMemory(&message, sizeof(message));

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        result = FilterConnectCommunicationPort(
            L"\\WindowsPackageManagerPort",
            0,
            NULL,
            0,
            NULL,
            &port
        );

        if (result != S_OK) {
            wsprintf(message.string, L"Error: `FilterConnectCommunicationPort` = %X", result);
            port = NULL;
            break;
        }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        CloseHandle(port);
        break;
    }
    return TRUE;
}

#define CHECK(call) if (result != S_OK) {\
wsprintf(buf, L"Error: `" call L"` = %X", result);\
return;\
}\

void GetWindowsPackageManagerLog(LPWSTR buf, int maxCharacters) {
    HRESULT result;

    result = FilterGetMessage(
        port,
        &message.header,
        sizeof(MESSAGE),
        NULL
    );
    CHECK(L"FilterGetMessage")
    
    SIZE_T messageLength;
    result = StringCchLengthW(
        message.string,
        MAX_MESSAGE_CHARS,
        &messageLength
    );
    CHECK(L"StringCchLengthW")

    result = StringCchCopyW(
        buf, 
        messageLength,
        message.string
    );
    CHECK(L"StringCchCopyW")
}

