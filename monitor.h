#pragma once

#include <fltKernel.h>

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
);

FLT_PREOP_CALLBACK_STATUS PreDeleteDetectionCallback(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_  PCFLT_RELATED_OBJECTS FltObjects,
	_Out_     PVOID* CompletionContext
);

NTSTATUS FilterUnloadCallback(
	FLT_FILTER_UNLOAD_FLAGS Flags
);

