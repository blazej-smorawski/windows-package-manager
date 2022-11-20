#include <fltKernel.h>
#include <ntstrsafe.h>

#define     FILENAME_SIZE 1024
#define     FILENAME_SIZE_BYTES FILENAME_SIZE * 2
#define     CHECK(status) \
if (!NT_SUCCESS(status)) {\
    return FLT_PREOP_SUCCESS_NO_CALLBACK;\
}\

PFLT_FILTER         Filter;
PFLT_PORT            Port;
IO_STATUS_BLOCK     IoStatusBlock;


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
    HANDLE              processHandle;

    processHandle = PsGetCurrentProcessId();

    status = RtlStringCchPrintfW(
        fileNameBuffer,
        FILENAME_SIZE,
        L"%wZ\n",
        &FltObjects->FileObject->FileName
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

NTSTATUS
connectNotify(
    IN PFLT_PORT ClientPort,
    IN PVOID ServerPortCookie,
    IN PVOID ConnectionContext,
    IN ULONG SizeOfContext,
    OUT PVOID* ConnectionPortCookie
) {
    UNREFERENCED_PARAMETER(ClientPort);
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
    CHECK(status)

    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(
        &attributes,
        L"\\WindowsPackageManagerPort",
        OBJ_KERNEL_HANDLE,
        NULL,
        securityDescriptor
    )

    status = FltCreateCommunicationPort(
        Filter,
        &Port,
        &attributes,
        NULL,
        connectNotify,
        disconnectNotify,
        messageNotify,
        1
    );



    return status;
}