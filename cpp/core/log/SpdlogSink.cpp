#include "SpdlogSink.h"

#include <spdlog/spdlog.h>
#if !defined(ANDROID)
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

namespace {

    spdlog::level::level_enum toSpdlogLevel(TVPLogLevel level) {
        switch(level) {
            case TVPLogLevel::Trace:
                return spdlog::level::trace;
            case TVPLogLevel::Debug:
                return spdlog::level::debug;
            case TVPLogLevel::Info:
                return spdlog::level::info;
            case TVPLogLevel::Warn:
                return spdlog::level::warn;
            case TVPLogLevel::Error:
                return spdlog::level::err;
            case TVPLogLevel::Fatal:
                return spdlog::level::critical;
        }
        return spdlog::level::info;
    }

} // namespace

void SpdlogSink::write(TVPLogLevel level, const char *category,
                       const std::string &message) {
    if(!category)
        category = "core";

    auto logger = spdlog::get(category);
    if(!logger) {
#if !defined(ANDROID)
        logger = spdlog::stdout_color_mt(category);
#else
        return;
#endif
    }

    logger->log(toSpdlogLevel(level), "{}", message);
}
