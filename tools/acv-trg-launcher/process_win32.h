#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace acv {
struct Result {
  int code = -1;
  std::string out, err;
};

inline std::string WinError(const char* operation) {
  return std::string(operation) + " failed with Win32 error " +
         std::to_string(GetLastError());
}

inline std::wstring Wide(const std::string& value) {
  if (value.empty()) return {};
  int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                 static_cast<int>(value.size()), nullptr, 0);
  if (count <= 0) throw std::runtime_error("Invalid UTF-8 in Windows process argument");
  std::wstring result(static_cast<size_t>(count), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), result.data(), count) != count)
    throw std::runtime_error("Could not convert UTF-8 process argument to UTF-16");
  return result;
}

inline std::string Utf8(const std::wstring& value) {
  if (value.empty()) return {};
  int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                 static_cast<int>(value.size()), nullptr, 0,
                                 nullptr, nullptr);
  if (count <= 0) throw std::runtime_error("Could not convert UTF-16 argument to UTF-8");
  std::string result(static_cast<size_t>(count), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), result.data(), count,
                          nullptr, nullptr) != count)
    throw std::runtime_error("Could not convert UTF-16 argument to UTF-8");
  return result;
}

inline std::wstring QuoteArgument(const std::wstring& value) {
  std::wstring result(1, L'"');
  size_t slashes = 0;
  for (wchar_t ch : value) {
    if (ch == L'\\') {
      ++slashes;
    } else if (ch == L'"') {
      result.append(slashes * 2 + 1, L'\\');
      result.push_back(L'"');
      slashes = 0;
    } else {
      result.append(slashes, L'\\');
      slashes = 0;
      result.push_back(ch);
    }
  }
  result.append(slashes * 2, L'\\');
  result.push_back(L'"');
  return result;
}

inline std::wstring CommandLine(const std::vector<std::string>& args) {
  if (args.empty() || args.front().empty())
    throw std::runtime_error("Empty command");
  std::wstring result;
  for (const auto& arg : args) {
    if (!result.empty()) result.push_back(L' ');
    result += QuoteArgument(Wide(arg));
  }
  return result;
}

class WinHandle {
 public:
  WinHandle() = default;
  explicit WinHandle(HANDLE handle) : value_(handle) {}
  ~WinHandle() { Reset(); }
  WinHandle(const WinHandle&) = delete;
  WinHandle& operator=(const WinHandle&) = delete;
  WinHandle(WinHandle&& other) noexcept : value_(other.Release()) {}
  WinHandle& operator=(WinHandle&& other) noexcept {
    if (this != &other) {
      Reset();
      value_ = other.Release();
    }
    return *this;
  }
  HANDLE get() const { return value_; }
  HANDLE Release() {
    HANDLE value = value_;
    value_ = nullptr;
    return value;
  }
  void Reset(HANDLE value = nullptr) {
    if (value_ && value_ != INVALID_HANDLE_VALUE) CloseHandle(value_);
    value_ = value;
  }
  explicit operator bool() const { return value_ && value_ != INVALID_HANDLE_VALUE; }

 private:
  HANDLE value_ = nullptr;
};

struct CaptureState {
  std::mutex mutex;
  std::string error;
  bool overflow = false;
};

inline void DrainPipe(HANDLE pipe, std::string& output, CaptureState& state) {
  char buffer[8192];
  size_t total = 0;
  for (;;) {
    DWORD count = 0;
    if (!ReadFile(pipe, buffer, sizeof(buffer), &count, nullptr)) {
      const DWORD error = GetLastError();
      if (error != ERROR_BROKEN_PIPE && error != ERROR_OPERATION_ABORTED) {
        std::lock_guard<std::mutex> guard(state.mutex);
        if (state.error.empty()) state.error = "ReadFile pipe error " + std::to_string(error);
      }
      break;
    }
    if (!count) break;
    if (total + count <= 4 * 1024 * 1024) {
      output.append(buffer, count);
      total += count;
    } else {
      std::lock_guard<std::mutex> guard(state.mutex);
      state.overflow = true;
    }
  }
  CloseHandle(pipe);
}

inline void WritePipe(HANDLE pipe, const std::string& input, CaptureState& state) {
  size_t offset = 0;
  while (offset < input.size()) {
    DWORD count = 0;
    const DWORD wanted = static_cast<DWORD>(
        std::min<size_t>(input.size() - offset, MAXDWORD));
    if (!WriteFile(pipe, input.data() + offset, wanted, &count, nullptr)) {
      const DWORD error = GetLastError();
      if (error != ERROR_BROKEN_PIPE && error != ERROR_NO_DATA) {
        std::lock_guard<std::mutex> guard(state.mutex);
        if (state.error.empty()) state.error = "WriteFile pipe error " + std::to_string(error);
      }
      break;
    }
    if (!count) break;
    offset += count;
  }
  CloseHandle(pipe);
}

inline Result Run(const std::vector<std::string>& args,
                  const std::string& input = "", int timeout_ms = 60000,
                  std::function<bool()> cancelled = {}) {
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  WinHandle output_read, output_write, error_read, error_write, input_read, input_write;
  HANDLE child_out = nullptr, parent_out = nullptr;
  if (!CreatePipe(&parent_out, &child_out, &security, 0)) throw std::runtime_error(WinError("CreatePipe stdout"));
  output_read.Reset(parent_out); output_write.Reset(child_out);
  HANDLE child_err = nullptr, parent_err = nullptr;
  if (!CreatePipe(&parent_err, &child_err, &security, 0)) throw std::runtime_error(WinError("CreatePipe stderr"));
  error_read.Reset(parent_err); error_write.Reset(child_err);
  HANDLE child_in = nullptr, parent_in = nullptr;
  if (!CreatePipe(&child_in, &parent_in, &security, 0)) throw std::runtime_error(WinError("CreatePipe stdin"));
  input_read.Reset(child_in); input_write.Reset(parent_in);
  if (!SetHandleInformation(output_read.get(), HANDLE_FLAG_INHERIT, 0) ||
      !SetHandleInformation(error_read.get(), HANDLE_FLAG_INHERIT, 0) ||
      !SetHandleInformation(input_write.get(), HANDLE_FLAG_INHERIT, 0))
    throw std::runtime_error(WinError("SetHandleInformation"));

  WinHandle job(CreateJobObjectW(nullptr, nullptr));
  if (!job) throw std::runtime_error(WinError("CreateJobObjectW"));
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                               &limits, sizeof(limits)))
    throw std::runtime_error(WinError("SetInformationJobObject"));

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = input_read.get();
  startup.hStdOutput = output_write.get();
  startup.hStdError = error_write.get();
  std::wstring command = CommandLine(args);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                      CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED,
                      nullptr, nullptr, &startup, &process))
    throw std::runtime_error(WinError("CreateProcessW"));
  WinHandle process_handle(process.hProcess), thread_handle(process.hThread);
  if (!AssignProcessToJobObject(job.get(), process_handle.get())) {
    TerminateProcess(process_handle.get(), 1);
    throw std::runtime_error(WinError("AssignProcessToJobObject"));
  }
  output_write.Reset(); error_write.Reset(); input_read.Reset();
  if (ResumeThread(thread_handle.get()) == DWORD(-1)) {
    TerminateJobObject(job.get(), 1);
    throw std::runtime_error(WinError("ResumeThread"));
  }

  Result result;
  CaptureState state;
  std::thread stdout_reader, stderr_reader, stdin_writer;
  auto join_readers = [&] {
    if (stdin_writer.joinable()) stdin_writer.join();
    if (stdout_reader.joinable()) stdout_reader.join();
    if (stderr_reader.joinable()) stderr_reader.join();
  };
  auto terminate_job = [&] {
    if (!TerminateJobObject(job.get(), 1)) TerminateProcess(process_handle.get(), 1);
    WaitForSingleObject(process_handle.get(), INFINITE);
    job.Reset();
    join_readers();
  };
  try {
    stdout_reader = std::thread(
        [handle = std::move(output_read), &result, &state]() mutable {
          DrainPipe(handle.Release(), result.out, state);
        });
    stderr_reader = std::thread(
        [handle = std::move(error_read), &result, &state]() mutable {
          DrainPipe(handle.Release(), result.err, state);
        });
    stdin_writer = std::thread(
        [handle = std::move(input_write), &input, &state]() mutable {
          WritePipe(handle.Release(), input, state);
        });
  } catch (...) {
    terminate_job();
    throw;
  }
  const auto start = std::chrono::steady_clock::now();
  bool timed_out = false, was_cancelled = false;
  try {
    for (;;) {
      if (cancelled && cancelled()) { was_cancelled = true; break; }
      const DWORD wait = WaitForSingleObject(process_handle.get(), 20);
      if (wait == WAIT_OBJECT_0) break;
      if (wait == WAIT_FAILED) throw std::runtime_error(WinError("WaitForSingleObject"));
      if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(timeout_ms)) {
        timed_out = true;
        break;
      }
    }
  } catch (...) {
    terminate_job();
    throw;
  }
  if (timed_out || was_cancelled) {
    terminate_job();
  }
  DWORD exit_code = 0;
  if (!GetExitCodeProcess(process_handle.get(), &exit_code)) {
    const std::string error = WinError("GetExitCodeProcess");
    terminate_job();
    throw std::runtime_error(error);
  }
  // Closing this job handle also closes any descendant pipe handles on normal
  // exit, so reader threads cannot wait forever on an orphaned child.
  job.Reset();
  join_readers();
  if (was_cancelled) throw std::runtime_error("Cancelled");
  if (timed_out) throw std::runtime_error("Backend command timed out");
  {
    std::lock_guard<std::mutex> guard(state.mutex);
    if (state.overflow) throw std::runtime_error("Backend output exceeds 4 MiB");
    if (!state.error.empty()) throw std::runtime_error(state.error);
  }
  result.code = static_cast<int>(exit_code);
  return result;
}

class Game {
 public:
  unsigned long pid = 0;
  int exit_code = 0;
  ~Game() { Close(); }
  void Start(const std::vector<std::string>& args, const std::string& log) {
    if (pid) throw std::runtime_error("Game is already running");
    WinHandle logfile(CreateFileW(Wide(log).c_str(), FILE_APPEND_DATA,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!logfile) throw std::runtime_error(WinError("CreateFileW game log"));
    WinHandle null_input(CreateFileW(L"NUL", GENERIC_READ,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!null_input) throw std::runtime_error(WinError("Open NUL"));
    if (!SetHandleInformation(logfile.get(), HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) ||
        !SetHandleInformation(null_input.get(), HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
      throw std::runtime_error(WinError("SetHandleInformation game"));
    WinHandle job(CreateJobObjectW(nullptr, nullptr));
    if (!job) throw std::runtime_error(WinError("CreateJobObjectW game"));
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = null_input.get();
    startup.hStdOutput = logfile.get();
    startup.hStdError = logfile.get();
    std::wstring command = CommandLine(args);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED,
                        nullptr, nullptr, &startup, &process))
      throw std::runtime_error(WinError("CreateProcessW game"));
    WinHandle process_handle(process.hProcess), thread_handle(process.hThread);
    if (!AssignProcessToJobObject(job.get(), process_handle.get())) {
      TerminateProcess(process_handle.get(), 1);
      throw std::runtime_error(WinError("AssignProcessToJobObject game"));
    }
    if (ResumeThread(thread_handle.get()) == DWORD(-1)) {
      TerminateJobObject(job.get(), 1);
      throw std::runtime_error(WinError("ResumeThread game"));
    }
    pid = process.dwProcessId;
    process_ = process_handle.Release();
    job_ = job.Release();
  }
  bool Poll() {
    if (!process_) return false;
    const DWORD wait = WaitForSingleObject(process_, 0);
    if (wait == WAIT_TIMEOUT) return true;
    if (wait != WAIT_OBJECT_0) throw std::runtime_error(WinError("WaitForSingleObject game"));
    DWORD code = 0;
    if (!GetExitCodeProcess(process_, &code)) throw std::runtime_error(WinError("GetExitCodeProcess game"));
    exit_code = static_cast<int>(code);
    pid = 0;
    Close();
    return false;
  }
  void Stop() {
    if (!process_) return;
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);
    if (WaitForSingleObject(process_, 2000) == WAIT_TIMEOUT && job_)
      TerminateJobObject(job_, 1);
    WaitForSingleObject(process_, INFINITE);
    Poll();
  }

 private:
  void Close() {
    if (process_) { CloseHandle(process_); process_ = nullptr; }
    if (job_) { CloseHandle(job_); job_ = nullptr; }
  }
  HANDLE process_ = nullptr;
  HANDLE job_ = nullptr;
};
}  // namespace acv
