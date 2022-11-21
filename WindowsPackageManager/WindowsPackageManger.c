#include <fltKernel.h>
#include <ntstrsafe.h>

#define     FILENAME_SIZE 1024
#define     FILENAME_SIZE_BYTES FILENAME_SIZE * 2
#define     CHECK(status) \
if (!NT_SUCCESS(status)) {\
    return FLT_PREOP_SUCCESS_NO_CALLBACK;\
}\

PFLT_FILTER         Filter;
PFLT_PORT           ServerPort;
PFLT_PORT           GlobalClientPort;
HANDLE              TargetPID;
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

    NTSTATUS            status;
    HANDLE              processHandle;
    LARGE_INTEGER       timeout;

    timeout.QuadPart = -10;

    processHandle = PsGetCurrentProcessId();

    if (TargetPID != NULL && TargetPID == processHandle) {
        status = FltSendMessage(
            Filter,
            &GlobalClientPort,
            FltObjects->FileObject->FileName.Buffer,
            FltObjects->FileObject->FileName.Length,
            0,
            0,
            &timeout
        );
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
    TargetPID = NULL;
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
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ReturnOutputBufferLength);

    if (InputBufferLength == sizeof(void*)) {
        TargetPID = *((HANDLE*)InputBuffer);
    }
    return 0;
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) {
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS    status;

    TargetPID = NULL;
    GlobalClientPort = NULL;

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
    CHECK(status)

    FltFreeSecurityDescriptor(
        securityDescriptor
    );

    return status;
}