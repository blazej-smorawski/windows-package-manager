#include <fltKernel.h>
#include <ntstrsafe.h>

#define     FILENAME_SIZE 1024
#define     CHECK(status) \
if (!NT_SUCCESS(status)) {\
    return FLT_PREOP_SUCCESS_NO_CALLBACK;\
}\

PFLT_FILTER         Filter;
RTL_AVL_TABLE       LogTable;
HANDLE              LogHandle;
IO_STATUS_BLOCK     IoStatusBlock;

NTSTATUS ZwQueryInformationProcess(
    _In_      HANDLE           ProcessHandle,
    _In_      PROCESSINFOCLASS ProcessInformationClass,
    _Out_     PVOID            ProcessInformation,
    _In_      ULONG            ProcessInformationLength,
    _Out_opt_ PULONG           ReturnLength
);


FLT_PREOP_CALLBACK_STATUS
PreCreateOperation(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID* CompletionContext
) {
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
        
    WCHAR       fileNameBuffer[FILENAME_SIZE];
    WCHAR       exeNameBuffer[FILENAME_SIZE];

    NTSTATUS            status;
    size_t              fileNameLength;
    HANDLE              processHandle;
    OBJECT_ATTRIBUTES   objectAttributes;
    CLIENT_ID           clientId;
    UNICODE_STRING      exeNameString;

    clientId.UniqueProcess = PsGetCurrentProcessId();
    clientId.UniqueThread = NULL;
    InitializeObjectAttributes(
        &objectAttributes,
        NULL, 0, NULL, NULL
    );

    status = ZwOpenProcess(&processHandle,
        PROCESS_ALL_ACCESS,
        &objectAttributes,
        &clientId
    );
    CHECK(status);

    exeNameString.Buffer = exeNameBuffer;
    exeNameString.Length = 0;
    exeNameString.MaximumLength = sizeof(exeNameBuffer);

    status = ZwQueryInformationProcess(
        processHandle,
        ProcessImageFileName,
        &exeNameString,
        sizeof(exeNameBuffer),
        NULL
    );
    CHECK(status)

    status = RtlStringCchPrintfW(
        fileNameBuffer,
        FILENAME_SIZE,
        L"%wZ::%wZ\n",
        &FltObjects->FileObject->FileName,
        &exeNameString
    );
    CHECK(status)

    status = RtlStringCbLengthW(
        fileNameBuffer,
        FILENAME_SIZE,
        &fileNameLength
    );
    CHECK(status)

    status = ZwWriteFile(
        LogHandle, 
        NULL, NULL, NULL, 
        &IoStatusBlock,
        fileNameBuffer, 
        (ULONG)fileNameLength, 
        NULL, NULL
    );
    CHECK(status)

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
PostCreateOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
) {
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    KdPrint(("PostCreateOperation\n"));
    return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS
FilterUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
) {
    UNREFERENCED_PARAMETER(Flags);

    FltUnregisterFilter(Filter);
    ZwClose(LogHandle);

    return STATUS_SUCCESS;
}

const FLT_OPERATION_REGISTRATION Callbacks[] = {

    { IRP_MJ_CREATE,
      0,
      PreCreateOperation,
      PostCreateOperation },
    { IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof(FLT_REGISTRATION),               //  Size
    FLT_REGISTRATION_VERSION,               //  Version   
#if MINISPY_WIN8 
    FLTFL_REGISTRATION_SUPPORT_NPFS_MSFS,   //  Flags
#else
    0,                                      //  Flags
#endif // MINISPY_WIN8

    NULL,                                   //  Context
    Callbacks,                              //  Operation callbacks

    FilterUnload,                                   //  FilterUnload

    NULL,                                   //  InstanceSetup
    NULL,                                   //  InstanceQueryTeardown
    NULL,                                   //  InstanceTeardownStart
    NULL,                                   //  InstanceTeardownComplete

    NULL,                                   //  GenerateFileName
    NULL,                                   //  GenerateDestinationFileName
    NULL                                    //  NormalizeNameComponent

#if MINISPY_VISTA

    ,
    SpyKtmNotificationCallback              //  KTM notification callback

#endif // MINISPY_VISTA

};

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) {
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS            status;
    UNICODE_STRING      uniName;
    OBJECT_ATTRIBUTES   objAttr;

    RtlInitUnicodeString(&uniName, L"\\DosDevices\\C:\\WINDOWS\\WindowsPackageManager.log");  // or L"\\SystemRoot\\example.txt"
    InitializeObjectAttributes(&objAttr, &uniName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL);

    status = ZwCreateFile(&LogHandle,
        GENERIC_WRITE,
        &objAttr, &IoStatusBlock, NULL,
        FILE_ATTRIBUTE_NORMAL,
        0,
        FILE_OVERWRITE_IF,
        FILE_SYNCHRONOUS_IO_NONALERT,
        NULL, 0);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = FltRegisterFilter(
        DriverObject,                  //Driver
        &FilterRegistration,           //Registration
        &Filter);    //RetFilter

    status = FltStartFiltering(Filter);
    if (!NT_SUCCESS(status)) {
        FltUnregisterFilter(Filter);
        ZwClose(LogHandle);
    }

    return status;
}