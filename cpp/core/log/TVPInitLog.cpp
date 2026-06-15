#include "TVPInitLog.h"

#include "SpdlogSink.h"
#include "TVPLog.h"
#include "TVPLogLevelConfig.h"

#include <cstring>

#include <spdlog/spdlog.h>
#if defined(ANDROID)
#include <spdlog/sinks/android_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

namespace {

    constexpr auto kDefaultLogPattern =
        "%Y-%m-%dT%H:%M:%S.%e%z %^%-5L%$ %t --- [%n] : %v";

    spdlog::level::level_enum toSpdlogInitLevel(TVPLogInitLevel level) {
        switch(level) {
            case TVPLogInitLevel::Trace:
                return spdlog::level::trace;
            case TVPLogInitLevel::Debug:
                return spdlog::level::debug;
            case TVPLogInitLevel::Info:
                return spdlog::level::info;
            case TVPLogInitLevel::Warn:
                return spdlog::level::warn;
            case TVPLogInitLevel::Error:
                return spdlog::level::err;
            case TVPLogInitLevel::Off:
                return spdlog::level::off;
        }
        return spdlog::level::info;
    }

    void ensureNamedLogger(const char *name) {
        if(spdlog::get(name))
            return;

#if defined(ANDROID)
        static const struct {
            const char *name;
            const char *tag;
        } kAndroidLoggers[] = {
            { "core", "KrKr2NativeCore" },
            { "tjs2", "KrKr2NativeTjs2" },
            { "plugin", "KrKr2NativePlugin" },
        };
        for(const auto &entry : kAndroidLoggers) {
            if(std::strcmp(entry.name, name) == 0) {
                spdlog::android_logger_mt(entry.name, entry.tag);
                return;
            }
        }
        spdlog::android_logger_mt(name, name);
#else
        spdlog::stdout_color_mt(name);
#endif
    }

} // namespace

TVPLogLevel initLevelToLogLevel(TVPLogInitLevel level) {
    switch(level) {
        case TVPLogInitLevel::Trace:
            return TVPLogLevel::Trace;
        case TVPLogInitLevel::Debug:
            return TVPLogLevel::Debug;
        case TVPLogInitLevel::Info:
            return TVPLogLevel::Info;
        case TVPLogInitLevel::Warn:
            return TVPLogLevel::Warn;
        case TVPLogInitLevel::Error:
            return TVPLogLevel::Error;
        case TVPLogInitLevel::Off:
            return TVPLogLevel::Fatal;
    }
    return TVPLogLevel::Info;
}

const char *TVPExtractLogLevelFromArgv(int argc, char **argv) {
    for(int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if(!arg)
            continue;
        if(std::strncmp(arg, "-loglevel=", 10) == 0)
            return arg + 10;
        if(std::strncmp(arg, "--loglevel=", 11) == 0)
            return arg + 11;
        if((std::strcmp(arg, "-loglevel") == 0 ||
            std::strcmp(arg, "--loglevel") == 0) &&
           i + 1 < argc)
            return argv[++i];
    }
    return nullptr;
}

void TVPEnsureSpdlogLoggers(const TVPLoggingOptions &options) {
    static bool initialized = false;
    if(initialized)
        return;
    initialized = true;

    spdlog::set_level(toSpdlogInitLevel(options.level));
    spdlog::set_pattern(options.pattern ? options.pattern : kDefaultLogPattern);

    ensureNamedLogger("core");
    ensureNamedLogger("tjs2");
    ensureNamedLogger("plugin");

    spdlog::set_default_logger(spdlog::get("core"));
}

void TVPRegisterDefaultLogSinks() {
    static bool registered = false;
    if(registered)
        return;
    registered = true;

    TVPRegisterLogSink(std::make_unique<SpdlogSink>());
}

void TVPInitLogging(const TVPLoggingOptions &options) {
    TVPEnsureSpdlogLoggers(options);
    if(options.log_level)
        TVPApplyLogLevelSpec(options.log_level,
                             initLevelToLogLevel(options.level));
    TVPRegisterDefaultLogSinks();
}
