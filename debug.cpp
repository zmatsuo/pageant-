#include <stdio.h>
#include <stdarg.h>
#include <vector>
#include <mutex>
#include <map>
#include <sstream>
#include <iomanip>

#include "pageant+.h"		// for DEVELOP_VERSION
#include "debug.h"
#include "winmisc.h"		// for debug_console_puts()

#if defined(DEVELOP_VERSION)
#define ENABLE
#else
#undef	ENABLE
#endif

#if defined(ENABLE)
static std::vector<debugOutputFunc> outputFuncs_;
static std::mutex mtx_;
static std::mutex mtx_fl_vprintf_;
static std::map<DWORD, std::string> thread_name_;
#endif

void debug_add_output(debugOutputFunc func)
{
	(void)func;
#if defined(ENABLE)
	outputFuncs_.emplace_back(func);
#endif
}

void debug_remove_output(debugOutputFunc func)
{
	(void)func;
#if defined(ENABLE)
	for (auto itr = outputFuncs_.begin(); itr != outputFuncs_.end(); itr++) {
		if (*itr == func) {
			outputFuncs_.erase(itr);
			return;
		}
	}
#endif
}

void debug_output_str(const char *s)
{
	(void)s;
#if defined(ENABLE)
	std::lock_guard<std::mutex> lock(mtx_);
	if (outputFuncs_.size() == 0) {
		// default output
		printf("%s", s);
		return;
	}
	for (const auto &f : outputFuncs_) {
		f(s);
	}
#endif
}

void debug_vprintf(const char *fmt, va_list ap)
{
#if defined(ENABLE)
	char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);

	if (len < 0) {
		// フォーマット失敗?
		debug_output_str("failure in vsnprintf()\n");
		return;
	}
	if (len == sizeof(buf)) {
		buf[sizeof(buf)-1] = '\0';
	}

	debug_output_str(buf);
#else
    (void)fmt;
    (void)ap;
#endif	// defined(ENABLE)
}

void debug_printf(const char *fmt, ...)
{
#if defined(ENABLE)
    va_list ap;

    va_start(ap, fmt);
    debug_vprintf(fmt, ap);
    va_end(ap);
#else
    (void)fmt;
#endif	// defined(ENABLE)
}

#if defined(ENABLE)
static const char *getFileName(const char *fullPathfileName)
{
	const char *fn = strrchr(fullPathfileName, '\\');
	if (fn == NULL) {
		fn = strrchr(fullPathfileName, '/');
	}
	if (fn == NULL) {
		fn = fullPathfileName;
	} else {
		fn++;
	}
	return fn;
}
#endif

#if defined(ENABLE)
static std::string createTimestamp()
{
	static SYSTEMTIME systime_prev;
	SYSTEMTIME systime;
	::GetLocalTime(&systime);
	std::ostringstream oss;
	if (systime.wYear != systime_prev.wYear ||
		systime.wMonth != systime_prev.wMonth ||
		systime.wDay != systime_prev.wDay)
	{
		systime_prev = systime;
		oss
			<< std::setfill('0')
			<< std::setw(2) << systime.wYear % 1000
			<< "/"
			<< std::setw(2) << systime.wMonth
			<< "/"
			<< std::setw(2) << systime.wDay
			<< " ";
	}
	oss
		<< std::setfill('0')
		<< std::setw(2) << systime.wHour
		<< ":"
		<< std::setw(2) << systime.wMinute
		<< ":"
		<< std::setw(2) << systime.wSecond
		<< "."
		<< std::setw(3) << systime.wMilliseconds;
	return oss.str();
}
#endif

#if defined(ENABLE)
static std::string getThreadName()
{
	std::ostringstream ss;
	const DWORD tid = GetCurrentThreadId();
	const auto &thread_name = thread_name_[tid];
	if (thread_name.empty()) {
		ss << "TID:0x" << std::hex << tid;
	} else {
		ss << "T:" << thread_name
		   << "(0x" << std::hex << tid << ")";
	}
	return ss.str();
}
#endif

void debug_fl_vprintf(const char *fn, int line,  _Printf_format_string_ const char *fmt, const va_list ap)
{
#if defined(ENABLE)
	std::lock_guard<std::mutex> lock(mtx_fl_vprintf_);
	const char *fname = getFileName(fn);
	std::ostringstream ss;
	ss << fname << ":" << line << " ";
#if 1
	ss << createTimestamp() << " " << getThreadName() << " ";
#endif
	debug_output_str(ss.str().c_str());
	debug_vprintf(fmt, ap);
#else	// defined(ENABLE)
    (void)fn;
    (void)line;
    (void)fmt;
    (void)ap;
#endif	// defined(ENABLE)
}

void debug_fl_printf(const char *fn, int line, const char *fmt, ...)
{
#if defined(ENABLE)
	va_list ap;
    va_start(ap, fmt);
	debug_fl_vprintf(fn, line, fmt, ap);
    va_end(ap);
#else
	(void)fn;
	(void)line;
    (void)fmt;
#endif	// defined(ENABLE)
}

void debug_memdump(const void *buf, size_t len, int L)
{
#if defined(ENABLE)
    size_t i;
    const unsigned char *p = (unsigned char *)buf;
    char foo[17];
    if (L) {
		int delta;
		debug_printf("\t%d (0x%x) bytes:\n", len, len);
		delta = (int)(15 & (uintptr_t) p);
		p -= delta;
		len += delta;
    }
	while(1) {
		debug_printf("  ");
		if (L)
			debug_printf("%p: ", p);
		strcpy_s(foo, "................");	/* sixteen dots */
		for (i = 0; i < 16 && i < len; ++i) {
			if (&p[i] < (unsigned char *) buf) {
				debug_printf("   ");	       /* 3 spaces */
				foo[i] = ' ';
			} else {
				debug_printf("%c%02.2x",
							 &p[i] != (unsigned char *) buf
							 && i % 4 ? '.' : ' ', p[i]
					);
				if (p[i] >= ' ' && p[i] <= '~')
					foo[i] = (char) p[i];
			}
		}
		foo[i] = '\0';
		debug_printf("%*s%s\n", (16 - i) * 3 + 2, "", foo);
		if (len < 16) {
			break;
		}
		p += 16;
		len -= 16;
    }
#else
    (void)buf;
    (void)len;
    (void)L;
#endif	// defined(ENABLE)
}

void debug_set_thread_name(const char *name)
{
#if defined(ENABLE)
	const DWORD tid = GetCurrentThreadId();
	thread_name_[tid] = name;
#else
	(void)name;
#endif
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
