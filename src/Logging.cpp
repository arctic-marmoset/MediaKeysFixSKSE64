#include "Logging.hpp"

#ifdef SOURCE_LOCATION_IF_DEBUG
#error SOURCE_LOCATION_IF_DEBUG already defined.
#endif

#ifndef NDEBUG
#define SOURCE_LOCATION_IF_DEBUG "[%s:%#]"
#else
#define SOURCE_LOCATION_IF_DEBUG ""
#endif

namespace
{
    constexpr const char *GlobalLoggerName = "GLOBAL";
}

void Logging::Init()
{
    std::optional<std::filesystem::path> maybePath = SKSE::log::log_directory();
    if (!maybePath)
    {
        SKSE::stl::report_and_fail("Unable to lookup SKSE logs directory.");
    }

    std::filesystem::path path = std::exchange(maybePath, std::nullopt).value();
    path /= fmt::format("{}.log", SKSE::PluginDeclaration::GetSingleton()->GetName());

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true));
    if (IsDebuggerPresent())
    {
        sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
    }

    auto logger = std::make_shared<spdlog::logger>(
        GlobalLoggerName,
        sinks.begin(),
        sinks.end()
    );

    logger->set_level(spdlog::level::level_enum::trace);
    logger->flush_on(spdlog::level::level_enum::trace);
    logger->set_pattern("[%Y-%m-%dT%T.%e][%n][%t][%L]" SOURCE_LOCATION_IF_DEBUG " %v");

    spdlog::set_default_logger(std::move(logger));
}
