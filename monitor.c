#include "monitor.h"

PFLT_FILTER RegisteredFilter;

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


