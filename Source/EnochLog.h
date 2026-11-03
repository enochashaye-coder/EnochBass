#pragma once
// v3.6: a small log of what Enoch does when an instance is created, its window opened or closed, and the instance removed.
// It is written to %APPDATA%\Enoch\enoch-log.txt, one line at a time (opened, written and closed for every line), so that if
// the DAW ever stops responding, the last line shows how far the plugin got. Plain C file calls only: it is safe to use while
// the plugin is being unloaded, and it is never called from the audio thread.
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>
#include <chrono>
#include <string>
#ifdef _WIN32
 #include <direct.h>
#else
 #include <sys/stat.h>
#endif

namespace enochlog
{
inline std::string filePath()
{
#ifdef _WIN32
    const char* base = std::getenv ("APPDATA");
    std::string dir = std::string (base != nullptr ? base : ".") + "\\Enoch";
    _mkdir (dir.c_str());
    return dir + "\\enoch-log.txt";
#else
    const char* base = std::getenv ("HOME");
    std::string dir = std::string (base != nullptr ? base : ".") + "/.enoch";
    mkdir (dir.c_str(), 0755);
    return dir + "/enoch-log.txt";
#endif
}

inline void write (const char* fmt, ...)
{
    const std::string path = filePath();
    std::FILE* f = std::fopen (path.c_str(), "a");
    if (f == nullptr) return;
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t (now);
    const int ms = (int) (std::chrono::duration_cast<std::chrono::milliseconds> (now.time_since_epoch()).count() % 1000);
    std::tm tmv {};
#ifdef _WIN32
    localtime_s (&tmv, &t);
#else
    localtime_r (&t, &tmv);
#endif
    char stamp[32];
    std::strftime (stamp, sizeof (stamp), "%Y-%m-%d %H:%M:%S", &tmv);
#ifdef JucePlugin_Name
    std::fprintf (f, "%s.%03d  %s  ", stamp, ms, JucePlugin_Name);
#else
    std::fprintf (f, "%s.%03d  Enoch  ", stamp, ms);
#endif
    va_list args;
    va_start (args, fmt);
    std::vfprintf (f, fmt, args);
    va_end (args);
    std::fputc ('\n', f);
    std::fclose (f);
}

/** keeps the log short: started fresh when it grows past about half a megabyte */
inline void trimIfLarge()
{
    const std::string path = filePath();
    std::FILE* f = std::fopen (path.c_str(), "rb");
    if (f == nullptr) return;
    std::fseek (f, 0, SEEK_END);
    const long size = std::ftell (f);
    std::fclose (f);
    if (size > 512 * 1024) std::remove (path.c_str());
}

/** a member that writes one line when it is destroyed (put first in a class: it is destroyed last) */
struct Mark
{
    const char* what;
    explicit Mark (const char* w) : what (w) {}
    ~Mark() { write ("%s", what); }
};

/** one per plugin DLL: lines when Windows loads and unloads the plugin */
struct ModuleMark
{
    ModuleMark()  { trimIfLarge(); write ("plugin loaded by the DAW"); }
    ~ModuleMark() { write ("plugin being unloaded by the DAW (static clean-up)"); }
};
} // namespace enochlog
