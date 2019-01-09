#pragma once

#define BENCHMARK_OS_LINUX 1

#include "cycleclock.h"
#include <atomic>

#include <assert.h>
#include <cstdio>
#include <string>
#include <fstream>
#include <iostream>

#include <dlfcn.h>

typedef uint64_t journal_entry_t;

namespace benchmark {

double stod(const std::string& str, size_t* pos = nullptr);
double stod(const std::string& str, size_t* pos) {
  /* Record previous errno */
  const int oldErrno = errno;
  errno = 0;

  const char* strStart = str.c_str();
  char* strEnd = const_cast<char*>(strStart);
  const double result = strtod(strStart, &strEnd);

  /* Restore previous errno */
  const int strtodErrno = errno;
  errno = oldErrno;

  /* Check for errors and return */
  if (strtodErrno == ERANGE) {
    throw std::out_of_range(
      "stoul failed: " + str + " is outside of range of int");
  } else if (strEnd == strStart || strtodErrno != 0) {
    throw std::invalid_argument(
      "stoul failed: " + str + " is not an integer");
  }
  if (pos != nullptr) {
    *pos = static_cast<size_t>(strEnd - strStart);
  }
  return result;
}

template <class ArgT>
bool ReadFromFile(std::string const& fname, ArgT* arg) {
  *arg = ArgT();
  std::ifstream f(fname.c_str());
  if (!f.is_open()) return false;
  f >> *arg;
  return f.good();
}

double GetCPUCyclesPerSecond() {
#if defined BENCHMARK_OS_LINUX || defined BENCHMARK_OS_CYGWIN
  long freq;

  // If the kernel is exporting the tsc frequency use that. There are issues
  // where cpuinfo_max_freq cannot be relied on because the BIOS may be
  // exporintg an invalid p-state (on x86) or p-states may be used to put the
  // processor in a new mode (turbo mode). Essentially, those frequencies
  // cannot always be relied upon. The same reasons apply to /proc/cpuinfo as
  // well.
  if (ReadFromFile("/sys/devices/system/cpu/cpu0/tsc_freq_khz", &freq)
      // If CPU scaling is in effect, we want to use the *maximum* frequency,
      // not whatever CPU speed some random processor happens to be using now.
      || ReadFromFile("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
                      &freq)) {
    // The value is in kHz (as the file name suggests).  For example, on a
    // 2GHz warpstation, the file contains the value "2000000".
    return freq * 1000.0;
  }

  const double error_value = -1;
  double bogo_clock = error_value;

  std::ifstream f("/proc/cpuinfo");
  if (!f.is_open()) {
    std::cerr << "failed to open /proc/cpuinfo\n";
    return error_value;
  }

  auto startsWithKey = [](std::string const& Value, std::string const& Key) {
    if (Key.size() > Value.size()) return false;
    auto Cmp = [&](char X, char Y) {
      return std::tolower(X) == std::tolower(Y);
    };
    return std::equal(Key.begin(), Key.end(), Value.begin(), Cmp);
  };

  std::string ln;
  while (std::getline(f, ln)) {
    if (ln.empty()) continue;
    size_t SplitIdx = ln.find(':');
    std::string value;
    if (SplitIdx != std::string::npos) value = ln.substr(SplitIdx + 1);
    // When parsing the "cpu MHz" and "bogomips" (fallback) entries, we only
    // accept positive values. Some environments (virtual machines) report zero,
    // which would cause infinite looping in WallTime_Init.
    if (startsWithKey(ln, "cpu MHz")) {
      if (!value.empty()) {
        double cycles_per_second = benchmark::stod(value) * 1000000.0;
        if (cycles_per_second > 0) return cycles_per_second;
      }
    } else if (startsWithKey(ln, "bogomips")) {
      if (!value.empty()) {
        bogo_clock = benchmark::stod(value) * 1000000.0;
        if (bogo_clock < 0.0) bogo_clock = error_value;
      }
    }
  }
  if (f.bad()) {
    std::cerr << "Failure reading /proc/cpuinfo\n";
    return error_value;
  }
  if (!f.eof()) {
    std::cerr << "Failed to read to end of /proc/cpuinfo\n";
    return error_value;
  }
  f.close();
  // If we found the bogomips clock, but nothing better, we'll use it (but
  // we're not happy about it); otherwise, fallback to the rough estimation
  // below.
  if (bogo_clock >= 0.0) return bogo_clock;

#elif defined BENCHMARK_HAS_SYSCTL
  constexpr auto* FreqStr =
#if defined(BENCHMARK_OS_FREEBSD) || defined(BENCHMARK_OS_NETBSD)
      "machdep.tsc_freq";
#elif defined BENCHMARK_OS_OPENBSD
      "hw.cpuspeed";
#else
      "hw.cpufrequency";
#endif
  unsigned long long hz = 0;
#if defined BENCHMARK_OS_OPENBSD
  if (GetSysctl(FreqStr, &hz)) return hz * 1000000;
#else
  if (GetSysctl(FreqStr, &hz)) return hz;
#endif
  fprintf(stderr, "Unable to determine clock rate from sysctl: %s: %s\n",
          FreqStr, strerror(errno));

#elif defined BENCHMARK_OS_WINDOWS
  // In NT, read MHz from the registry. If we fail to do so or we're in win9x
  // then make a crude estimate.
  DWORD data, data_size = sizeof(data);
  if (IsWindowsXPOrGreater() &&
      SUCCEEDED(
          SHGetValueA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      "~MHz", nullptr, &data, &data_size)))
    return static_cast<double>((int64_t)data *
                               (int64_t)(1000 * 1000));  // was mhz
#elif defined (BENCHMARK_OS_SOLARIS)
  kstat_ctl_t *kc = kstat_open();
  if (!kc) {
    std::cerr << "failed to open /dev/kstat\n";
    return -1;
  }
  kstat_t *ksp = kstat_lookup(kc, (char*)"cpu_info", -1, (char*)"cpu_info0");
  if (!ksp) {
    std::cerr << "failed to lookup in /dev/kstat\n";
    return -1;
  }
  if (kstat_read(kc, ksp, NULL) < 0) {
    std::cerr << "failed to read from /dev/kstat\n";
    return -1;
  }
  kstat_named_t *knp =
      (kstat_named_t*)kstat_data_lookup(ksp, (char*)"current_clock_Hz");
  if (!knp) {
    std::cerr << "failed to lookup data in /dev/kstat\n";
    return -1;
  }
  if (knp->data_type != KSTAT_DATA_UINT64) {
    std::cerr << "current_clock_Hz is of unexpected data type: "
              << knp->data_type << "\n";
    return -1;
  }
  double clock_hz = knp->value.ui64;
  kstat_close(kc);
  return clock_hz;
#endif
  assert(!"platform not supported");
#if 0
  // If we've fallen through, attempt to roughly estimate the CPU clock rate.
  const int estimate_time_ms = 1000;
  const auto start_ticks = cycleclock::Now();
  SleepForMilliseconds(estimate_time_ms);
  return static_cast<double>(cycleclock::Now() - start_ticks);
#endif
}
};

#define MDR_TYPE uint16_t
#define MDR_MAX  0xFFFF

class MdrJournal
{
public:
    MdrJournal()
    {
    }

    inline void event_add(uint32_t event)
    {
        uint64_t cycles_now = benchmark::cycleclock::Now();
        uint32_t current_idx = idx.fetch_add(1);
        call_time[current_idx] = (cycles_now<<8) | (event&0xFF);
    }

    inline void call_begin(void* fptr)
    {
        uint64_t cycles_now = benchmark::cycleclock::Now();
        uint32_t current_idx = idx.fetch_add(1);
        call_time[current_idx] = (cycles_now<<1) | 1;
        call_addr[current_idx] = (uint64_t)fptr;
    }

    inline void call_end(void* fptr)
    {
        uint64_t cycles_now = benchmark::cycleclock::Now();
        uint32_t current_idx = idx.fetch_add(1);
        call_time[current_idx] = (cycles_now<<1);
        call_addr[current_idx] = (uint64_t)fptr;
    }

    bool flush(FILE* fh)
    {
        uint32_t current_idx = idx.load();
        for (uint32_t i = current_idx; i < MDR_MAX; ++i)
        {
            Dl_info dlinfo;
            void* sym_addr = (void*)call_addr[i];

            dladdr(sym_addr, &dlinfo);

            ptrdiff_t offset;
            char sign;
            char gen;
            if ((uintptr_t)sym_addr >= (uintptr_t)dlinfo.dli_saddr)
            {
              offset = (uintptr_t)sym_addr - (uintptr_t)dlinfo.dli_saddr;
              sign = '+';
            }
            else
            {
              offset = (uintptr_t)dlinfo.dli_saddr - (uintptr_t)sym_addr;
              sign = '-';
            }
            if (call_time[i] & 1)
            {
              gen = 'B';
            }
            else
            {
              gen = 'E';
            }
            fprintf(fh, "%c@0x%lx %s(%s%c%lx) [%p]\n"
                , gen
                , call_time[i]>>1
                , dlinfo.dli_fname
                , dlinfo.dli_sname
                , sign
                , offset
                , sym_addr
                );
        }
        for (uint32_t i = 0; i<current_idx; ++i)
        {
            Dl_info dlinfo;
            void* sym_addr = (void*)call_addr[i];

            dladdr(sym_addr, &dlinfo);

            ptrdiff_t offset;
            char sign;
            char gen;
            if ((uintptr_t)sym_addr >= (uintptr_t)dlinfo.dli_saddr)
            {
              offset = (uintptr_t)sym_addr - (uintptr_t)dlinfo.dli_saddr;
              sign = '+';
            }
            else
            {
              offset = (uintptr_t)dlinfo.dli_saddr - (uintptr_t)sym_addr;
              sign = '-';
            }
            if (call_time[i] & 1)
            {
              gen = 'B';
            }
            else
            {
              gen = 'E';
            }
            fprintf(fh, "%c@0x%lx %s(%s%c%lx) [%p]\n"
                , gen
                , call_time[i]>>1
                , dlinfo.dli_fname
                , dlinfo.dli_sname
                , sign
                , offset
                , sym_addr
                );
        }

        return true;
    }

private:
    std::atomic<MDR_TYPE> idx={0};
    journal_entry_t call_addr[MDR_MAX]={0};
    journal_entry_t call_time[MDR_MAX]={0};
};
