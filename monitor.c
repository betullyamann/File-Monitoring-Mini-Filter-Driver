
#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include <ntifs.h>
#include <ntddk.h>

#define EVENT_LOG_OPERATION_SIZE 16
#define EVENT_LOG_FILE_PATH_SIZE 256
#define EVENT_LOG_PROCESS_NAME_SIZE 256

PFLT_FILTER RegisteredFilter = NULL;

typedef struct EVENT_LOG {
	LARGE_INTEGER Time;
	ULONG ProcessId;
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

VOID LogDeletion(
	_In_ PFLT_CALLBACK_DATA Data,
	_Inout_ DELETION_LOG* deletionLog
);

VOID SafeCopy(
	PWCHAR dest, 
	PCWCHAR src, 
	SIZE_T count
);

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
	UNREFERENCED_PARAMETER(FilterRelatedObjects);
	UNREFERENCED_PARAMETER(CompletionContext);

	if (Data->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION)
	{
		FILE_INFORMATION_CLASS fileInformationClass = Data->Iopb->Parameters.SetFileInformation.FileInformationClass;
		if (fileInformationClass == FileDispositionInformation || fileInformationClass == FileDispositionInformationEx)
		{
			PFILE_DISPOSITION_INFORMATION fileInformation = (PFILE_DISPOSITION_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
			if (fileInformation->DeleteFile)
			{
				DELETION_LOG deletionLog = { 0 };
				LogDeletion(Data, &deletionLog);

				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				DbgPrint("File deletion blocked.\n");
				return FLT_PREOP_COMPLETE;
			}
		}
	}

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
};

VOID SafeCopy(PWCHAR dest, PCWCHAR src, SIZE_T count) {
	wcsncpy(dest, src, count);
	dest[count - 1] = L'\0';
}

VOID LogDeletion(
	_In_ PFLT_CALLBACK_DATA Data,
	_Inout_ DELETION_LOG* deletionLog)
{
	PFLT_FILE_NAME_INFORMATION fileNameInfo = NULL;

	NTSTATUS status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &fileNameInfo);
	if (NT_SUCCESS(status))
	{
		status = FltParseFileNameInformation(fileNameInfo);
		if (NT_SUCCESS(status))
		{
			KeQuerySystemTime(&deletionLog->Time);
			deletionLog->ProcessId = FltGetRequestorProcessId(Data);
			SafeCopy(deletionLog->Operation, L"delete", EVENT_LOG_OPERATION_SIZE);
			SafeCopy(deletionLog->FilePath, fileNameInfo->Name.Buffer, EVENT_LOG_FILE_PATH_SIZE);
		}
		FltReleaseFileNameInformation(fileNameInfo);
	}
	else
	{
		KeQuerySystemTime(&deletionLog->Time);
		deletionLog->ProcessId = FltGetRequestorProcessId(Data);
		SafeCopy(deletionLog->Operation, L"delete", EVENT_LOG_OPERATION_SIZE);
		SafeCopy(deletionLog->FilePath, L"unknown", EVENT_LOG_FILE_PATH_SIZE);
	}
}

NTSTATUS FilterUnloadCallback(
	FLT_FILTER_UNLOAD_FLAGS Flags) 
{
	UNREFERENCED_PARAMETER(Flags);
	FltUnregisterFilter(RegisteredFilter);

	return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT MonitoringFilterObject,
	_In_ PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	NTSTATUS status = FltRegisterFilter(MonitoringFilterObject, &Registration, &RegisteredFilter);
	if (NT_SUCCESS(status)) {
		status = FltStartFiltering(RegisteredFilter);
		if (!NT_SUCCESS(status)) {
			FltUnregisterFilter(RegisteredFilter);
		}
	}

	return status;
}


