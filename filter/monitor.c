#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>

#define EVENT_LOG_OPERATION_SIZE 16
#define EVENT_LOG_FILE_PATH_SIZE 256
#define EVENT_LOG_PROCESS_NAME_SIZE 256

#define SERVER_PORT_NAME L"\\MonitoringFilterPort"

UNICODE_STRING ProtectedDirectory = { 0 };
FAST_MUTEX Mutex;

PFLT_FILTER RegisteredFilter = NULL;
PFLT_PORT ServerCommunicationPort = NULL;
PFLT_PORT UserCommunicationPort = NULL;


typedef PCHAR(*GET_PROCESS_IMAGE_NAME) (PEPROCESS Process);
GET_PROCESS_IMAGE_NAME gGetProcessImageFileName;

typedef struct EVENT_LOG {
    LARGE_INTEGER Time;
    WCHAR ProcessName[EVENT_LOG_PROCESS_NAME_SIZE];
    WCHAR Operation[EVENT_LOG_OPERATION_SIZE];
    WCHAR FilePath[EVENT_LOG_FILE_PATH_SIZE];
} DELETION_LOG;

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

FLT_PREOP_CALLBACK_STATUS PreDeleteDetectionCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

NTSTATUS FilterUnloadCallback(
    FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS GetProcessName(
    _In_ PFLT_CALLBACK_DATA Data,
    _Out_ PWCHAR ProcessName,
    _In_ ULONG BufferSize
);

VOID SafeCopy(
    PWCHAR dest,
    PCWCHAR src,
    SIZE_T count
);

NTSTATUS FilterConnect(
    _In_ PFLT_PORT ClientPort,
    _In_ PVOID ServerPortCookie,
    _In_reads_bytes_(SizeOfContext) PVOID ConnectionContext,
    _In_ ULONG SizeOfContext,
    _Outptr_result_maybenull_ PVOID* ConnectionCookie
);

VOID FilterDisconnect(
    _In_opt_ PVOID ConnectionCookie
);

NTSTATUS FilterCreateCommunicationPort();

NTSTATUS SendLogToUser(DELETION_LOG* deletionLog);

NTSTATUS MessageNotify(
    PVOID PortCookie,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PULONG ReturnOutputBufferLength
);

BOOLEAN IsProtectedFile(_In_ PUNICODE_STRING FilePath);

const FLT_OPERATION_REGISTRATION Callbacks[] = {
	{IRP_MJ_SET_INFORMATION, 0, PreDeleteDetectionCallback, NULL},
	{IRP_MJ_OPERATION_END} 
};

const FLT_REGISTRATION Registration = {
    sizeof(FLT_REGISTRATION),
    FLT_REGISTRATION_VERSION,
    0,
    NULL,
    Callbacks,
    FilterUnloadCallback,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

FLT_PREOP_CALLBACK_STATUS PreDeleteDetectionCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FilterRelatedObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext)
{
    NTSTATUS status = FLT_PREOP_SUCCESS_NO_CALLBACK;

    UNREFERENCED_PARAMETER(FilterRelatedObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    if (ProtectedDirectory.Length == 0) {
        return status;
    }

	if (Data->Iopb->MajorFunction != IRP_MJ_SET_INFORMATION)
	{
        return status;
    }

    FILE_INFORMATION_CLASS fileInformationClass = Data->Iopb->Parameters.SetFileInformation.FileInformationClass;
	if (fileInformationClass != FileDispositionInformation && fileInformationClass != FileDispositionInformationEx)
	{
        return status;
    }

    PFILE_DISPOSITION_INFORMATION fileInformation = (PFILE_DISPOSITION_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
	if (!fileInformation->DeleteFile)
	{
        return status;
    }

    DELETION_LOG deletionLog = { 0 };
    PFLT_FILE_NAME_INFORMATION fileNameInfo = NULL;

    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &fileNameInfo);
	if (!NT_SUCCESS(status))
	{
        return status;
    }

    status = FltParseFileNameInformation(fileNameInfo);
	if (!NT_SUCCESS(status))
	{
        FltReleaseFileNameInformation(fileNameInfo);
        return status;
    }

    SafeCopy(deletionLog.FilePath, fileNameInfo->Name.Buffer, EVENT_LOG_FILE_PATH_SIZE);
    UNICODE_STRING unicodeStr;
    RtlInitUnicodeString(&unicodeStr, deletionLog.FilePath);
    if (!IsProtectedFile(&unicodeStr)) {
        FltReleaseFileNameInformation(fileNameInfo);
        return status;
    }

    KeQuerySystemTime(&deletionLog.Time);
    GetProcessName(Data, deletionLog.ProcessName, EVENT_LOG_PROCESS_NAME_SIZE);
    SafeCopy(deletionLog.Operation, L"delete", EVENT_LOG_OPERATION_SIZE);

    if (SendLogToUser(&deletionLog) != STATUS_SUCCESS) {
        DbgPrint("Failed to send log to user.\n");
    }

    FltReleaseFileNameInformation(fileNameInfo);

    Data->IoStatus.Status = STATUS_ACCESS_DENIED;
    DbgPrint("File deletion blocked.\n");

    return FLT_PREOP_COMPLETE;
};

BOOLEAN IsProtectedFile(_In_ PUNICODE_STRING FilePath) {
    BOOLEAN result = FALSE;
    ExAcquireFastMutex(&Mutex);
    if (ProtectedDirectory.Length > 0 && RtlPrefixUnicodeString(&ProtectedDirectory, FilePath, TRUE)) {
        result = TRUE;
    }
    ExReleaseFastMutex(&Mutex);

    return result;
}

NTSTATUS SendLogToUser(DELETION_LOG* deletionLog) {
    NTSTATUS status = STATUS_SUCCESS;

    if (UserCommunicationPort != NULL) {
        status = FltSendMessage(RegisteredFilter, &UserCommunicationPort, deletionLog, sizeof(DELETION_LOG), NULL, NULL, NULL);
        if (!NT_SUCCESS(status)) {
            status = STATUS_PORT_DISCONNECTED;
        }
    }
    else {
        status = STATUS_PORT_DISCONNECTED;
    }
    return status;
}

VOID SafeCopy(PWCHAR dest, PCWCHAR src, SIZE_T count) {
    wcsncpy(dest, src, count);
    dest[count - 1] = L'\0';
}

VOID ConvertAnsiToUnicode(_In_ const PCHAR source, _Out_ PWCHAR destination, _In_ SIZE_T destSize) {
    ANSI_STRING ansiString;
    UNICODE_STRING unicodeString;

    RtlInitAnsiString(&ansiString, source);
    unicodeString.Buffer = destination;
    unicodeString.Length = 0;
    unicodeString.MaximumLength = (USHORT)destSize;

    if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, FALSE))) {
        destination[unicodeString.Length / sizeof(WCHAR)] = L'\0';
    }
    else {
        destination[0] = L'\0';
    }
}

NTSTATUS GetProcessName(_In_ PFLT_CALLBACK_DATA Data, _Out_ PWCHAR ProcessNameW, _In_ ULONG BufferSize) {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PEPROCESS process = FltGetRequestorProcess(Data);
    if (NULL != process && NULL != gGetProcessImageFileName)
    {
        PCHAR imageName = gGetProcessImageFileName(process);
        if (NULL != imageName)
        {
            CHAR ProcessName[EVENT_LOG_PROCESS_NAME_SIZE] = { 0 };
            status = RtlStringCbCopyA(ProcessName, sizeof(ProcessName), imageName);
            if (NT_SUCCESS(status)) {
                ConvertAnsiToUnicode(ProcessName, ProcessNameW, BufferSize);
            }
        }
    }

    return status;
}


NTSTATUS FilterUnloadCallback(
    FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);

    if (ServerCommunicationPort != NULL) {
        FltCloseCommunicationPort(ServerCommunicationPort);
        ServerCommunicationPort = NULL;
    }

    if (UserCommunicationPort != NULL) {
        FltCloseCommunicationPort(UserCommunicationPort);
        UserCommunicationPort = NULL;
    }

    FltUnregisterFilter(RegisteredFilter);

    return STATUS_SUCCESS;
}

NTSTATUS FilterConnect(
    _In_ PFLT_PORT ClientPort,
    _In_ PVOID ServerPortCookie,
    _In_reads_bytes_(SizeOfContext) PVOID ConnectionContext,
    _In_ ULONG SizeOfContext,
    _Outptr_result_maybenull_ PVOID* ConnectionCookie)
{
    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);
    *ConnectionCookie = NULL;

    UserCommunicationPort = ClientPort;
    return STATUS_SUCCESS;
}

VOID FilterDisconnect(
    _In_opt_ PVOID ConnectionCookie)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ConnectionCookie);
    if (UserCommunicationPort != NULL)
    {
        FltCloseClientPort(RegisteredFilter, &UserCommunicationPort);
        UserCommunicationPort = NULL;
    }
}

NTSTATUS MessageNotify(
    PVOID PortCookie,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PULONG ReturnOutputBufferLength
) {
    PAGED_CODE();

    UNREFERENCED_PARAMETER(PortCookie);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ReturnOutputBufferLength);

    NTSTATUS status = STATUS_SUCCESS;

    if (InputBuffer != NULL) {
        __try {
            PWCHAR wInputBuffer = InputBuffer;
            wInputBuffer[InputBufferLength - 1] = L'\0';
			RtlInitUnicodeString(&ProtectedDirectory, wInputBuffer);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }
    }
    else {
        status = STATUS_INVALID_PARAMETER;
    }

    return status;
}

NTSTATUS FilterCreateCommunicationPort()
{
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING portName;
    NTSTATUS status;

    InitializeObjectAttributes(&objectAttributes, &portName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = FltCreateCommunicationPort(RegisteredFilter,
        &ServerCommunicationPort,
        &objectAttributes,
        NULL,
        FilterConnect,
        FilterDisconnect,
        MessageNotify,
        1);
    if (!NT_SUCCESS(status)) {
        KdPrint(("FltCreateCommunicationPort failed: 0x%08x\n", status));
    }
    else {
        KdPrint(("Successfully created communication port.\n"));
    }

    return status;
}

NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT MonitoringFilterObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    ExInitializeFastMutex(&Mutex);

    UNICODE_STRING sPsGetProcessImageFileName = RTL_CONSTANT_STRING(L"PsGetProcessImageFileName");
    gGetProcessImageFileName = (GET_PROCESS_IMAGE_NAME)MmGetSystemRoutineAddress(&sPsGetProcessImageFileName);

	NTSTATUS status = FltRegisterFilter(MonitoringFilterObject, &Registration, &RegisteredFilter);
    if (!NT_SUCCESS(status)) {
        DbgPrint("FltRegisterFilter failed, status: 0x%x\n", status);
        return status;
    }

    status = FilterCreateCommunicationPort();
    if (!NT_SUCCESS(status)) {
        DbgPrint("FilterCreateCommunicationPort failed, status: 0x%x\n", status);
        FltUnregisterFilter(RegisteredFilter);
        return status;
    }

    status = FltStartFiltering(RegisteredFilter);
    if (!NT_SUCCESS(status)) {
        DbgPrint("FltStartFiltering failed, status: 0x%x\n", status);
        FltUnregisterFilter(RegisteredFilter);
        return status;
    }

    return status;
}
