#pragma once

enum class TVPLogInitLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Off,
};

struct TVPLoggingOptions {
    TVPLogInitLevel level = TVPLogInitLevel::Info;
    const char *pattern = nullptr;
    bool plugin_logger = true;
    const char *log_level = nullptr;
};

// Scan argv for -loglevel / --loglevel (returns nullptr if absent).
const char *TVPExtractLogLevelFromArgv(int argc, char **argv);

// Create core / tjs2 / plugin spdlog loggers (platform-specific sinks).
void TVPEnsureSpdlogLoggers(const TVPLoggingOptions &options = {});

// Register SpdlogSink for TVPLog dispatcher.
void TVPRegisterDefaultLogSinks();

// Logger setup + default sinks (typical entry-point call).
void TVPInitLogging(const TVPLoggingOptions &options = {});
