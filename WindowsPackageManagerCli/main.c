#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <fltUser.h>

typedef struct _MESSAGE {
    FILTER_MESSAGE_HEADER header;
    WCHAR bytes[2048];
} MESSAGE, *PMESSAGE;

int _tmain(int argc, TCHAR *argv[]) {
	if (argc != 2) {
        printf("Usage: WindowsPackageManagerCli EXEC_NAME");
		return 1;
	}

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    si.cb = sizeof(si);

    // Start the child process. 
    if (!CreateProcess(NULL,   // No module name (use command line)
        argv[1],        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi)           // Pointer to PROCESS_INFORMATION structure
        )
    {
        printf("CreateProcess failed (%d).\n", GetLastError());
        return 1;
    }

    printf("PID = %d\n", pi.dwProcessId);

    HANDLE      port;
    HRESULT     result;

    result = FilterConnectCommunicationPort(
        L"\\WindowsPackageManagerPort",
        0,
        NULL,
        0,
        NULL,
        &port
    );

    printf("FilterConnectCommunicationPort = %d\n", result);

    HANDLE      PID = pi.dwProcessId;
    DWORD       bytesReturned = 0;

    result = FilterSendMessage(
        port,
        &PID,
        sizeof(HANDLE),
        NULL,
        0,
        &bytesReturned
    );

    printf("FilterSendMessage = %d\n", result);

    MESSAGE     message;
    ZeroMemory(&message, sizeof(message));

    while (TRUE) {
        result = FilterGetMessage(
            port,
            &message,
            sizeof(MESSAGE),
            NULL
        );

        if (result != S_OK) {
            printf("FilterGetMessage = %d\n", result);
        }
        else {
            wprintf(L"%s\n", message.bytes);
        }
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles. 
    CloseHandle(port);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

	return 0;
}