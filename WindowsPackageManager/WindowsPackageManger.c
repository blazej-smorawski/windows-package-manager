#include <fltKernel.h>
#include <ntstrsafe.h>

#define     FILENAME_SIZE 2048
#define     FILENAME_SIZE_BYTES FILENAME_SIZE * 2
#define     CHECK(status) \
if (!NT_SUCCESS(status)) {\
    return FLT_POSTOP_FINISHED_PROCESSING;\
}\

PFLT_FILTER         Filter;
PFLT_PORT           ServerPort;
PFLT_PORT           GlobalClientPort;
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

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
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

    WCHAR       fileNameBuffer[FILENAME_SIZE];
    /*
     * This buffer is going to receive `UNICODE_STRING` + `WCHAR[]` buffer 
     */
    WCHAR       exeNameBuffer[FILENAME_SIZE + sizeof(UNICODE_STRING)];

    NTSTATUS            status;
    PUNICODE_STRING     exeNameString;
    size_t              fileNameLength;

    HANDLE              processHandle;
    OBJECT_ATTRIBUTES   objectAttributes;
    CLIENT_ID           clientId;

    ULONG               createDisposition = 0;
    BOOLEAN             isNewFile = FALSE;
    LARGE_INTEGER       timeout;

    if (GlobalClientPort == NULL) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }
    /*
     * Get create disposition
     * `Options`
     * Bitmask of flags that specify the options to be applied when creating or opening the file, 
     * as well as the action to be taken if the file already exists.
     * The low 24 bits of this member correspond to the CreateOptions parameter to FltCreateFile.
     * The high 8 bits correspond to the CreateDisposition parameter to FltCreateFile.
     */
    createDisposition = (Data->Iopb->Parameters.Create.Options >> 24) & 0x000000FF;

    /*
     * Check if new file is creating or not
     */ 
    isNewFile = ((FILE_SUPERSEDE == createDisposition)
        || (FILE_CREATE == createDisposition)
        || (FILE_OPEN_IF == createDisposition)
        || (FILE_OVERWRITE == createDisposition)
        || (FILE_OVERWRITE_IF == createDisposition));

    if (!isNewFile) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

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

    status = ZwQueryInformationProcess(
        processHandle,
        ProcessImageFileName,
        exeNameBuffer,
        FILENAME_SIZE_BYTES + sizeof(UNICODE_STRING),
        NULL
    );
    CHECK(status)

    exeNameString = (PUNICODE_STRING)exeNameBuffer;
    status = RtlStringCchPrintfW(
        fileNameBuffer,
        FILENAME_SIZE,
        L"%wZ::%wZ\0\n",
        &FltObjects->FileObject->FileName,
        exeNameString
    );
    CHECK(status)

    status = RtlStringCbLengthW(
        fileNameBuffer,
        FILENAME_SIZE,
        &fileNameLength
    );
    CHECK(status)

    timeout.QuadPart = -10;
    status = FltSendMessage(
        Filter,
        &GlobalClientPort,
        fileNameBuffer,
        (ULONG)fileNameLength + 2, // `Length` does not include the trailing null character
        0,
        0,
        &timeout
    );

    return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS
FilterUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
) {
    UNREFERENCED_PARAMETER(Flags);

    FltCloseCommunicationPort(ServerPort);
    FltUnregisterFilter(Filter);

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
connectNotify(
    IN PFLT_PORT ClientPort,
    IN PVOID ServerPortCookie,
    IN PVOID ConnectionContext,
    IN ULONG SizeOfContext,
    OUT PVOID* ConnectionPortCookie
) {
    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);
    UNREFERENCED_PARAMETER(ConnectionPortCookie);

    GlobalClientPort = ClientPort;
    return 0;
}

VOID
disconnectNotify(
    IN PVOID ConnectionCookie
) {
    UNREFERENCED_PARAMETER(ConnectionCookie);
}

NTSTATUS
messageNotify(
    IN PVOID PortCookie,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    OUT PULONG ReturnOutputBufferLength
) {
    UNREFERENCED_PARAMETER(PortCookie);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ReturnOutputBufferLength);

    return 0;
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) {
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS    status;

    status = FltRegisterFilter(
        DriverObject,                  //Driver
        &FilterRegistration,           //Registration
        &Filter
    );

    status = FltStartFiltering(Filter);
    if (!NT_SUCCESS(status)) {
        FltUnregisterFilter(Filter);
    }

    // Create port for communication
    PSECURITY_DESCRIPTOR securityDescriptor;
    status = FltBuildDefaultSecurityDescriptor(
        &securityDescriptor,
        FLT_PORT_ALL_ACCESS | FLT_PORT_CONNECT
    );

    UNICODE_STRING portName;
    RtlInitUnicodeString(&portName, L"\\WindowsPackageManagerPort");

    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(
        &attributes,
        &portName,
        OBJ_KERNEL_HANDLE,
        NULL,
        securityDescriptor
    )

    status = FltCreateCommunicationPort(
        Filter,
        &ServerPort,
        &attributes,
        NULL,
        connectNotify,
        disconnectNotify,
        messageNotify,
        100
    );
    if (!NT_SUCCESS(status)) {
        FltUnregisterFilter(Filter);
    }

    FltFreeSecurityDescriptor(
        securityDescriptor
    );

    return status;
}