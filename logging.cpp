#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(__linux)
#include <syscall.h>
#endif
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ctime>
#include <iomanip>

#include "logging.h"

namespace MumbleClient {

namespace logging {

const char* const log_severity_names[LOG_NUM_SEVERITIES] = { "INFO", "WARNING", "ERROR", "FATAL" };

int32_t log_level = 0;

int32_t CurrentProcessId() {
#if defined(_WIN32)
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}

int32_t CurrentThreadId() {
#if defined(_WIN32)
	return GetCurrentThreadId();
#elif defined(__APPLE__)
	return mach_thread_self();
#elif defined(__linux)
	return syscall(__NR_gettid);
#endif
}

void SetLogLevel(int32_t level) {
	log_level = level;
}

int32_t GetLogLevel() {
	return log_level;
}

LogMessage::LogMessage(const char* file, int32_t line) : severity_(LOG_INFO) {
	Init(file, line);
}

LogMessage::LogMessage(const char* file, int32_t line, LogSeverity severity) : severity_(severity) {
	Init(file, line);
}

void LogMessage::Init(const char* file, int32_t line) {
	const char* last_slash = strrchr(file, '\\');
	if (last_slash)
		file = last_slash + 1;

	stream_ <<  '[';
	stream_ << CurrentProcessId() << ':';
	stream_ << CurrentThreadId() << ':';

	time_t t = time(NULL);

	struct tm local_time;
#if defined(_WIN32)
	localtime_s(&local_time, &t);
#else
	localtime_r(&t, &local_time);
#endif
	struct tm* tm_time = &local_time;
	stream_ << std::setfill('0')
		<< std::setw(2) << 1 + tm_time->tm_mon
		<< std::setw(2) << tm_time->tm_mday
		<< '/'
		<< std::setw(2) << tm_time->tm_hour
		<< std::setw(2) << tm_time->tm_min
		<< std::setw(2) << tm_time->tm_sec
		<< ':';

	stream_ << log_severity_names[severity_] << ":" << file <<
		"(" << line << ")] ";

	message_start_ = stream_.tellp();
}

LogMessage::~LogMessage() {
	if (severity_ < log_level)
		return;

	stream_ << std::endl;
	std::string str_newline(stream_.str());

#if defined(_WIN32)
	OutputDebugString(str_newline.c_str());
#endif
	fprintf(stderr, "%s", str_newline.c_str());
	fflush(stderr);

	if (severity_ == LOG_FATAL) {
#if _WIN32
		__debugbreak();
#else
		abort();
#endif
	}
}

}  // namespace logging

}  // namespace MumbleClient
