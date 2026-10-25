#pragma once
#ifdef _WIN32
#include <cstdint>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winternl.h>
#ifdef interface
#undef interface
#endif
// Only the test copy of console.cpp is force-included with this header. All
// wrappers call the genuine imported Win32 function and preserve its LastError.
BOOL WINAPI vd_observe_ReadConsoleW(HANDLE,LPVOID,DWORD,LPDWORD,PCONSOLE_READCONSOLE_CONTROL);
BOOL WINAPI vd_observe_WriteConsoleW(HANDLE,const VOID*,DWORD,LPDWORD,LPVOID);
BOOL WINAPI vd_observe_ReadConsoleInputW(HANDLE,PINPUT_RECORD,DWORD,LPDWORD);
BOOL WINAPI vd_observe_PeekConsoleInputW(HANDLE,PINPUT_RECORD,DWORD,LPDWORD);
BOOL WINAPI vd_observe_SetConsoleMode(HANDLE,DWORD);
BOOL WINAPI vd_observe_ReadFile(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
BOOL WINAPI vd_observe_WriteFile(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
BOOL WINAPI vd_observe_GetOverlappedResult(HANDLE,LPOVERLAPPED,LPDWORD,BOOL);
BOOL WINAPI vd_observe_CancelIoEx(HANDLE,LPOVERLAPPED);
BOOL WINAPI vd_observe_CancelSynchronousIo(HANDLE);
BOOL WINAPI vd_observe_CloseHandle(HANDLE);
FARPROC WINAPI vd_observe_GetProcAddress(HMODULE,LPCSTR);
#ifdef VD_CONSOLE_OBSERVE_CALLS
#define ReadConsoleW vd_observe_ReadConsoleW
#define WriteConsoleW vd_observe_WriteConsoleW
#define ReadConsoleInputW vd_observe_ReadConsoleInputW
#define PeekConsoleInputW vd_observe_PeekConsoleInputW
#define SetConsoleMode vd_observe_SetConsoleMode
#define ReadFile vd_observe_ReadFile
#define WriteFile vd_observe_WriteFile
#define GetOverlappedResult vd_observe_GetOverlappedResult
#define CancelIoEx vd_observe_CancelIoEx
#define CancelSynchronousIo vd_observe_CancelSynchronousIo
#define CloseHandle vd_observe_CloseHandle
#define GetProcAddress vd_observe_GetProcAddress
#else
// Queries a real outstanding OS request, not just an admission/worker signal.
bool vd_console_native_pending();
std::uintptr_t vd_console_pending_buffer();
// Explicit fault-stress scenarios; never enabled by physical key tests.
void vd_console_next_write_fault(unsigned);
void vd_console_next_read_fault();
void vd_console_last_wide_read(bool&,DWORD&,DWORD&);
std::uint64_t vd_console_written_units();
#endif
#endif
