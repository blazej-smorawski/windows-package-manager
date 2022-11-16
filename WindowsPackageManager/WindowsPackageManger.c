#include <fltKernel.h>
#include <ntstrsafe.h>

#define     FILENAME_SIZE 1024
#define     FILENAME_SIZE_BYTES FILENAME_SIZE * 2
#define     CHECK(status) \
if (!NT_SUCCESS(status)) {\
    return FLT_PREOP_SUCCESS_NO_CALLBACK;\
}\

PFLT_FILTER         Filter;
RTL_AVL_TABLE       LogTable;
HANDLE              LogHandle;
IO_STATUS_BLOCK     IoStatusBlock;
DWORD               Counter;

CHAR                ShMem[33554432];
SIZE_T              ShMemOffset;
FAST_MUTEX          ShMemMutex;

NTSTATUS ZwQueryInformationProcess(
    _In_      HANDLE           ProcessHandle,
    _In_      PROCESSINFOCLASS ProcessInformationClass,
    _Out_     PVOID            ProcessInformation,
    _In_      ULONG            ProcessInformationLength,
    _Out_opt_ PULONG           ReturnLength
);

NTSTATUS WriteLogToFile() {
    NTSTATUS            status;
    UNICODE_STRING      uniName;
    OBJECT_ATTRIBUTES   objAttr;

    RtlInitUnicodeString(&uniName, L"\\DosDevices\\C:\\WINDOWS\\WindowsPackageManager.log");  // or L"\\SystemRoot\\example.txt"
    InitializeObjectAttributes(&objAttr, &uniName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL, NULL
    );

    status = ZwCreateFile(&LogHandle,
        GENERIC_WRITE,
        &objAttr, &IoStatusBlock, NULL,
        FILE_ATTRIBUTE_NORMAL,
        0,
        FILE_OVERWRITE_IF,
        FILE_SYNCHRONOUS_IO_NONALERT,
        NULL, 0
    );
    
    if (!NT_SUCCESS(status)) {
        return STATUS_FILE_NOT_AVAILABLE;
    }

    for (PWCHAR log = RtlEnumerateGenericTableAvl(&LogTable, TRUE);
        log != NULL;
        log = RtlEnumerateGenericTableAvl(&LogTable, FALSE)) {
        
        SIZE_T logLength;

        status = RtlStringCbLengthW(
            log,
            FILENAME_SIZE,
            &logLength
        );
        CHECK(status)

        status = ZwWriteFile(
            LogHandle,
            NULL, NULL, NULL,
            &IoStatusBlock,
            log,
            (ULONG)logLength,
            NULL, NULL
        );
        CHECK(status)
    }

    ZwClose(LogHandle);
    return 0;
}

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
    //size_t              fileNameLength;
    HANDLE              processHandle;
    OBJECT_ATTRIBUTES   objectAttributes;
    CLIENT_ID           clientId;
    UNICODE_STRING      exeNameString;
    BOOLEAN             newLog;

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

    RtlInsertElementGenericTableAvl(
        &LogTable,
        &fileNameBuffer,
        FILENAME_SIZE_BYTES,
        &newLog
    );
    Counter++;

    if (Counter % 100 == 0) {
        status = WriteLogToFile();
        CHECK(status)
    }

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
    //ZwClose(LogHandle);

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

RTL_GENERIC_COMPARE_RESULTS
CompareFileNames(
    __in struct _RTL_AVL_TABLE* Table,
    __in PVOID  FirstStruct,
    __in PVOID  SecondStruct
) {
    UNREFERENCED_PARAMETER(Table);
    UNICODE_STRING str1;
    UNICODE_STRING str2;

    str1.Buffer = FirstStruct;
    str2.Buffer = SecondStruct;
    str1.Length = FILENAME_SIZE;
    str2.Length = FILENAME_SIZE;
    str1.MaximumLength = FILENAME_SIZE;
    str2.MaximumLength = FILENAME_SIZE;

    LONG result = RtlCompareUnicodeString(
        &str1,
        &str2,
        FALSE
    );

    if (result == 0) {
        return GenericEqual;
    }
    else if (result > 0) {
        return GenericGreaterThan;
    }
    else {
        return GenericLessThan;
    }
}

PVOID
AllocateRoutine (
    __in struct _RTL_AVL_TABLE* Table,
    __in CLONG  ByteSize
) {
    UNREFERENCED_PARAMETER(Table);

    ExAcquireFastMutex(&ShMemMutex);

    if(ShMem + ShMemOffset + ByteSize > ShMem + sizeof(ShMem))
    {
        ShMemOffset = 0;
    }

    PCHAR ret = ShMem + ShMemOffset;
    ShMemOffset += ByteSize;

    KdPrint(("%p + %z = %p", ShMem, ShMemOffset, ret));

    /*return ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        ByteSize,
        'wpma'
    );*/

    ExReleaseFastMutex(&ShMemMutex);

    return ret;
}

VOID
FreeRoutine(
    __in struct _RTL_AVL_TABLE* Table,
    __in PVOID  Buffer
) {
    UNREFERENCED_PARAMETER(Table);
    UNREFERENCED_PARAMETER(Buffer);
    /*ExFreePoolWithTag(
        Buffer,
        'wpma'
    );*/
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) {
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS    status;
    Counter = 0;
    ShMemOffset = 0;

    ExInitializeFastMutex(&ShMemMutex);

    RtlInitializeGenericTableAvl(
        &LogTable,
        CompareFileNames,
        AllocateRoutine,
        FreeRoutine,
        NULL
    );

    status = FltRegisterFilter(
        DriverObject,                  //Driver
        &FilterRegistration,           //Registration
        &Filter
    );

    status = FltStartFiltering(Filter);
    if (!NT_SUCCESS(status)) {
        FltUnregisterFilter(Filter);
        ZwClose(LogHandle);
    }

    return status;
}