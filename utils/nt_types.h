#pragma once
#include <windows.h>

#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((LONG)0xC0000004)
#endif
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((LONG)0x00000000)
#endif

#define MY_SystemHandleInformation 16
#define MY_ObjectNameInformation   1

typedef LONG NTSTATUS;

typedef struct MY_UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} MY_UNICODE_STRING;

typedef struct MY_SYSTEM_HANDLE_ENTRY {
    USHORT ProcessId;
    USHORT CreatorBackTraceIndex;
    UCHAR ObjectTypeNumber;
    UCHAR HandleAttributes;
    USHORT HandleValue;
    PVOID Object;
    ACCESS_MASK GrantedAccess;
} MY_SYSTEM_HANDLE_ENTRY;

typedef struct MY_SYSTEM_HANDLE_INFORMATION {
    ULONG HandleCount;
    MY_SYSTEM_HANDLE_ENTRY Handles[1];
} MY_SYSTEM_HANDLE_INFORMATION;

typedef NTSTATUS (NTAPI*pNtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

typedef NTSTATUS (NTAPI*pNtQueryObject)(
    HANDLE Handle,
    ULONG ObjectInformationClass,
    PVOID ObjectInformation,
    ULONG ObjectInformationLength,
    PULONG ReturnLength
);

typedef NTSTATUS (NTAPI*pNtDuplicateObject)(
    HANDLE SourceProcessHandle,
    HANDLE SourceHandle,
    HANDLE TargetProcessHandle,
    PHANDLE TargetHandle,
    ACCESS_MASK DesiredAccess,
    ULONG HandleAttributes,
    ULONG Options
);

extern pNtQuerySystemInformation fnNtQuerySystemInformation;
extern pNtQueryObject fnNtQueryObject;
extern pNtDuplicateObject fnNtDuplicateObject;

bool LoadNtFunctions();
