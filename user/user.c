

#include <stdio.h>
#include <windows.h>
#include <fltuser.h>

#define PROTECTED_DIRECTORY L"D:\\NO_DELETE"

#define EVENT_LOG_OPERATION_SIZE 16
#define EVENT_LOG_FILE_PATH_SIZE 256
#define EVENT_LOG_PROCESS_NAME_SIZE 256

const PWSTR MonitoringPortName = L"\\MonitoringFilterPort";

typedef struct EVENT_LOG {
    LARGE_INTEGER Time; // Timestamp of the deletion attempt.
    WCHAR processName[EVENT_LOG_PROCESS_NAME_SIZE]; // Process name that attempted deletion.
    WCHAR operation[EVENT_LOG_OPERATION_SIZE]; // Operation type (e.g., "DELETE").
    WCHAR filePath[EVENT_LOG_FILE_PATH_SIZE]; // Path of the file targeted for deletion.
} DELETION_LOG;

int main() {
    HANDLE hPort;
    HRESULT result = FilterConnectCommunicationPort(MonitoringPortName, FLT_PORT_FLAG_SYNC_HANDLE, NULL, 0, NULL, &hPort);
    if (FAILED(result)) {
        wprintf(L"Could not connect to filter: 0x%08x\n", result);
        return 1;
    }

    // Send the directory path that should be protected to the filter driver.
    WCHAR buffer[] = PROTECTED_DIRECTORY;
    DWORD returnBytes;
    result = FilterSendMessage(hPort, buffer, sizeof(buffer), NULL, 0, &returnBytes);
    if (FAILED(result)) {
        wprintf(L"Could not send the FILE_PATH to filter: 0x%08x\n", result);
        CloseHandle(hPort);
        return 1;
    }

    DELETION_LOG deletionLog;
    while (1) {
        result = FilterGetMessage(hPort, (PFILTER_MESSAGE_HEADER)&deletionLog, sizeof(deletionLog), NULL);

        if (result == S_OK) {
            FILETIME ft;
            SYSTEMTIME st;
            ft.dwLowDateTime = deletionLog.Time.LowPart;
            ft.dwHighDateTime = deletionLog.Time.HighPart;
            if (FileTimeToSystemTime(&ft, &st)) {
                wprintf(L"[%02d-%02d-%04d %02d:%02d:%02d] DELETE - %s - %s - %s\n",
                    st.wDay, st.wMonth, st.wYear,
                    st.wHour, st.wMinute, st.wSecond,
                    deletionLog.processName, deletionLog.filePath, deletionLog.operation);
            }
            else {
                wprintf(L"Failed to convert time.\n");
            }
        }
        else {
            wprintf(L"Failed to get message: 0x%08x\n", result);
        }
    }

    CloseHandle(hPort);
    return 0;
}
