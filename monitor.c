
#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>

PFLT_FILTER RegisteredFilter;

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
				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				DbgPrint("File deletion blocked.\n");
				return FLT_PREOP_COMPLETE;
			}
		}
	}

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
};

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


