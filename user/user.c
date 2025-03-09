#include <stdio.h>
#include <strsafe.h>
#include <windows.h>
#include <fltuser.h>

#define SERVER_PORT_NAME L"\\MonitoringFilterPort"


#define EVENT_LOG_OPERATION_SIZE 16
#define EVENT_LOG_FILE_PATH_SIZE 256
#define EVENT_LOG_PROCESS_NAME_SIZE 256

typedef struct EVENT_LOG {
	SYSTEMTIME time;
	char processName[EVENT_LOG_PROCESS_NAME_SIZE];
	char operation[EVENT_LOG_OPERATION_SIZE];
	char filePath[EVENT_LOG_FILE_PATH_SIZE];
} DELETION_LOG;

int main() {
	HANDLE hPort;
	HRESULT result = FilterConnectCommunicationPort(SERVER_PORT_NAME, 0, NULL, 0, NULL, &hPort);
	if (FAILED(result)) {
		printf("Could not connect to filter: 0x%08x\n", result);
		return 1;
	}

	DELETION_LOG deletionLog;
	while (1) {
		result = FilterGetMessage(hPort, (PFILTER_MESSAGE_HEADER)&deletionLog, sizeof(deletionLog), NULL);

		if (result == S_OK) {
			printf("[%02d-%02d-%04d %02d:%02d:%02d] DELETE - %s - %s - %s\n",
				deletionLog.time.wDay, deletionLog.time.wMonth, deletionLog.time.wYear,
				deletionLog.time.wHour, deletionLog.time.wMinute, deletionLog.time.wSecond,
				deletionLog.processName, deletionLog.filePath, deletionLog.operation);
		}
	}


	return 0;
}
