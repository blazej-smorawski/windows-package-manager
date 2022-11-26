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

HRESULT StartWindowsPackageManagerLogging() 
{
    HRESULT result = S_OK;
    ZeroMemory(&message, sizeof(message));

    result = FilterConnectCommunicationPort(
        L"\\WindowsPackageManagerPort",
        0,
        NULL,
        0,
        NULL,
        &port
    );

    if (result != S_OK) {
        wsprintf(
            message.string,
            L"Error: `FilterConnectCommunicationPort` = %X",
            result
        );
        port = NULL;
    }

    return result;
}

HRESULT StopWindowsPackageManagerLogging()
{
    CloseHandle(port);
    return S_OK;
}

#define CHECK(call, ret) if (result != S_OK) {\
wsprintf(buf, L"Error: `" call L"` = %X", result);\
return ret;\
}\

HRESULT GetWindowsPackageManagerLog(
    LPWSTR buf, 
    int maxCharacters) 
{
    HRESULT result = S_OK;

    result = FilterGetMessage(
        port,
        &message.header,
        sizeof(MESSAGE),
        NULL
    );
    CHECK(L"FilterGetMessage",1)
    
    SIZE_T messageLength;
    result = StringCchLengthW(
        message.string,
        MAX_MESSAGE_CHARS,
        &messageLength
    );
    CHECK(L"StringCchLengthW",2)

    result = StringCchCopyW(
        buf, 
        messageLength+1,
        message.string
    );
    CHECK(L"StringCchCopyW", messageLength)

    return result;
}

