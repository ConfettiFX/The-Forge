#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define INITGUID
#include <evntrace.h>
#include <evntcons.h>

#include <unordered_map>

#include "../../microprofile.h"

const GUID g_ThreadClassGuid = { 0x3d6fa8d1, 0xfe05, 0x11d0, 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c };

std::unordered_map<uint32_t, uint32_t> g_ProcessIds;

uint64_t g_Switches = 0;
uint64_t g_SwitchesLast = 0;

HANDLE g_Pipe = INVALID_HANDLE_VALUE;
bool g_PipeWaiting = true;

uint32_t GetProcessId(uint32_t nThreadId)
{
	auto it = g_ProcessIds.find(nThreadId);
	if (it != g_ProcessIds.end())
		return it->second;

	uint32_t& nProcessId = g_ProcessIds[nThreadId];

	HANDLE Thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, nThreadId);
	if (Thread != INVALID_HANDLE_VALUE)
	{
		nProcessId = GetProcessIdOfThread(Thread);
		CloseHandle(Thread);
	}

	return nProcessId;
}

struct EventThread
{
	uint32_t ProcessId;
	uint32_t TThreadId;
	uint32_t StackBase;
	uint32_t StackLimit;
	uint32_t UserStackBase;
	uint32_t UserStackLimit;
	uint32_t StartAddr;
	uint32_t Win32StartAddr;
	uint32_t TebBase;
	uint32_t SubProcessTag;
};

struct EventCSwitch
{
	uint32_t NewThreadId;
	uint32_t OldThreadId;
	int8_t   NewThreadPriority;
	int8_t   OldThreadPriority;
	uint8_t  PreviousCState;
	int8_t   SpareByte;
	int8_t   OldThreadWaitReason;
	int8_t   OldThreadWaitMode;
	int8_t   OldThreadState;
	int8_t   OldThreadWaitIdealProcessor;
	uint32_t NewThreadWaitTime;
	uint32_t Reserved;
};

void ReopenPipe()
{
	if (g_Pipe != INVALID_HANDLE_VALUE)
		CloseHandle(g_Pipe);

	DWORD nPipeBuffer = 128 << 10;

	g_Pipe = CreateNamedPipeA("\\\\.\\pipe\\microprofile-contextswitch", PIPE_ACCESS_OUTBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE, 1, nPipeBuffer, 0, 0, NULL);

	if (g_Pipe == INVALID_HANDLE_VALUE)
	{
		printf("Failed to open named pipe: %d\n", GetLastError());
		exit(1);
	}

	g_PipeWaiting = true;

	printf("Pipe open, waiting for connection... ");
	fflush(stdout);
}

void UpdateSwitchStats()
{
	unsigned int N = 10000;

	if (g_PipeWaiting)
	{
		g_PipeWaiting = false;
		printf("connected\n");
	}

	g_Switches++;

	if (g_Switches >= g_SwitchesLast + N)
	{
		printf("\r%lld processed", g_Switches);
		fflush(stdout);

		g_SwitchesLast = g_Switches;
	}
}

void ContextSwitchPut(const MicroProfileContextSwitch& Switch)
{
	DWORD nResult;
	if (WriteFile(g_Pipe, &Switch, sizeof(Switch), &nResult, NULL))
	{
		UpdateSwitchStats();
	}
	else
	{
		DWORD nError = GetLastError();

		if (nError == ERROR_PIPE_LISTENING)
		{
		}
		else if (nError == ERROR_NO_DATA)
		{
			printf("; pipe broken\n");
			ReopenPipe();

			g_Switches = g_SwitchesLast = 0;
		}
		else
		{
			printf("Error writing to pipe: %d\n", nError);
			exit(1);
		}
	}
}

VOID WINAPI ContextSwitchCallback(PEVENT_TRACE pEvent)
{
	if (pEvent->Header.Guid == g_ThreadClassGuid)
	{
		if (pEvent->Header.Class.Type == 2 || pEvent->Header.Class.Type == 4)
		{
			EventThread* pData = (EventThread*)pEvent->MofData;

			// Invalidate TID->PID mapping
			g_ProcessIds.erase(pData->TThreadId);
		}
		else if (pEvent->Header.Class.Type == 36)
		{
			EventCSwitch* pData = (EventCSwitch*)pEvent->MofData;

			if (pData->NewThreadId != 0 || pData->OldThreadId != 0)
			{
				MicroProfileContextSwitch Switch;
				Switch.nThreadOut = pData->OldThreadId;
				Switch.nThreadIn = pData->NewThreadId;
				Switch.nProcessIn = GetProcessId(Switch.nThreadIn);
				Switch.nCpu = pEvent->BufferContext.ProcessorNumber;
				Switch.nTicks = pEvent->Header.TimeStamp.QuadPart;

				ContextSwitchPut(Switch);
			}
		}
	}
}

ULONG WINAPI BufferCallback(PEVENT_TRACE_LOGFILE Buffer)
{
	return TRUE;
}

struct KernelTraceProperties : public EVENT_TRACE_PROPERTIES
{
	char dummy[sizeof(KERNEL_LOGGER_NAME)];
};

void ContextSwitchShutdownTrace()
{
	KernelTraceProperties Properties;

	ZeroMemory(&Properties, sizeof(Properties));
	Properties.Wnode.BufferSize = sizeof(Properties);
	Properties.Wnode.Guid = SystemTraceControlGuid;
	Properties.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
	Properties.LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
	Properties.LogFileNameOffset = 0;

	ControlTrace(NULL, KERNEL_LOGGER_NAME, &Properties, EVENT_TRACE_CONTROL_STOP);
}

ULONG ContextSwitchStartTrace()
{
	KernelTraceProperties Properties;
	ZeroMemory(&Properties, sizeof(Properties));
	Properties.Wnode.BufferSize = sizeof(Properties);
	Properties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
	Properties.Wnode.ClientContext = 1; // Use QueryPerformanceCounter timestamps
	Properties.Wnode.Guid = SystemTraceControlGuid;
	Properties.BufferSize = 1;
	Properties.NumberOfBuffers = 128;
	Properties.EnableFlags = EVENT_TRACE_FLAG_CSWITCH | EVENT_TRACE_FLAG_THREAD;
	Properties.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
	Properties.MaximumFileSize = 0;
	Properties.LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
	Properties.LogFileNameOffset = 0;

	TRACEHANDLE SessionHandle;
	return StartTrace(&SessionHandle, KERNEL_LOGGER_NAME, &Properties);
}

int main()
{
	ContextSwitchShutdownTrace();

	ULONG nStatus = ContextSwitchStartTrace();

	if (ERROR_SUCCESS != nStatus)
	{
		printf("Failed to start ETW trace: %d\n", nStatus);
		return 1;
	}

	printf("Context switch trace started\n");

	ReopenPipe();

	EVENT_TRACE_LOGFILE Log;
	ZeroMemory(&Log, sizeof(Log));

	Log.LoggerName = KERNEL_LOGGER_NAME;
	Log.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
	Log.EventCallback = ContextSwitchCallback;
	Log.BufferCallback = BufferCallback;

	TRACEHANDLE hLog = OpenTrace(&Log);
	ProcessTrace(&hLog, 1, 0, 0);
	CloseTrace(hLog);

	return 0;
}