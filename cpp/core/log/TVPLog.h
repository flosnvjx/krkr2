//---------------------------------------------------------------------------
// Unified logging API (core_log)
//---------------------------------------------------------------------------
#ifndef TVPLogH
#define TVPLogH

#include <fmt/format.h>

#include <memory>
#include <string>

enum class TVPLogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

struct ITVPSink {
    virtual void write(TVPLogLevel level, const char *category,
                       const std::string &message) = 0;
    virtual ~ITVPSink() = default;
};

void TVPLog(TVPLogLevel level, const char *category,
            const std::string &message);
void TVPRegisterLogSink(std::unique_ptr<ITVPSink> sink);
void TVPUnregisterAllLogSinks();

template <typename... Args>
void TVPLogFmt(const TVPLogLevel level, const char *category,
               fmt::format_string<Args...> fmt, Args &&...args) {
    TVPLog(level, category, fmt::format(fmt, std::forward<Args>(args)...));
}

class TVPCategoryLogger {
    const char *category_;

    void log(const TVPLogLevel level, const std::string &message) const {
        TVPLog(level, category_, message);
    }

public:
    explicit TVPCategoryLogger(const char *category) : category_(category) {}

    [[nodiscard]] const char *category() const { return category_; }

    void trace(const std::string &message) const {
        log(TVPLogLevel::Trace, message);
    }
    void debug(const std::string &message) const {
        log(TVPLogLevel::Debug, message);
    }
    void info(const std::string &message) const {
        log(TVPLogLevel::Info, message);
    }
    void warn(const std::string &message) const {
        log(TVPLogLevel::Warn, message);
    }
    void error(const std::string &message) const {
        log(TVPLogLevel::Error, message);
    }
    void critical(const std::string &message) const {
        log(TVPLogLevel::Fatal, message);
    }

    template <typename... Args>
    void trace(fmt::format_string<Args...> fmt, Args &&...args) const {
        TVPLogFmt(TVPLogLevel::Trace, category_, fmt,
                  std::forward<Args>(args)...);
    }

    template <typename... Args>
    void debug(fmt::format_string<Args...> fmt, Args &&...args) const {
        TVPLogFmt(TVPLogLevel::Debug, category_, fmt,
                  std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(fmt::format_string<Args...> fmt, Args &&...args) const {
        TVPLogFmt(TVPLogLevel::Info, category_, fmt,
                  std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(fmt::format_string<Args...> fmt, Args &&...args) const {
        TVPLogFmt(TVPLogLevel::Warn, category_, fmt,
                  std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(fmt::format_string<Args...> fmt, Args &&...args) const {
        TVPLogFmt(TVPLogLevel::Error, category_, fmt,
                  std::forward<Args>(args)...);
    }

    template <typename... Args>
    void critical(fmt::format_string<Args...> fmt, Args &&...args) const {
        TVPLogFmt(TVPLogLevel::Fatal, category_, fmt,
                  std::forward<Args>(args)...);
    }
};

inline TVPCategoryLogger &TVPCoreLog() {
    static TVPCategoryLogger logger("core");
    return logger;
}

inline TVPCategoryLogger &TVPTjs2Log() {
    static TVPCategoryLogger logger("tjs2");
    return logger;
}

inline TVPCategoryLogger &TVPPluginLog() {
    static TVPCategoryLogger logger("plugin");
    return logger;
}

inline TVPCategoryLogger &TVPEngineLog() {
    static TVPCategoryLogger logger("engine");
    return logger;
}

inline TVPCategoryLogger &TVPExceptionLog() {
    static TVPCategoryLogger logger("exception");
    return logger;
}

#endif
