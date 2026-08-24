#pragma once

#include "../core/logging/Logger.hpp"

#include <exception>
#include <string>
#include <utility>

namespace Tutones::Runtime
{
    // Feature runtimes are optional capabilities, not application-critical services.
    // A newly-added feature must never tear down the renderer, hooks, native runtime,
    // or unrelated working features merely because its own Start() fails.
    template<typename StartFn, typename StopFn>
    [[nodiscard]] bool StartOptionalRuntime(
        const char* name,
        StartFn&& start,
        StopFn&& stop) noexcept
    {
        const char* runtimeName = name ? name : "Unnamed feature";

        try
        {
            if (std::forward<StartFn>(start)())
            {
                TUTONES_LOG_INFO(
                    "runtime.features",
                    std::string(runtimeName) + " started");
                return true;
            }
        }
        catch (const std::exception& exception)
        {
            TUTONES_LOG_ERROR(
                "runtime.features",
                std::string(runtimeName) + " startup threw: " + exception.what());
        }
        catch (...)
        {
            TUTONES_LOG_ERROR(
                "runtime.features",
                std::string(runtimeName) + " startup threw an unknown exception");
        }

        try
        {
            std::forward<StopFn>(stop)();
        }
        catch (...)
        {
            TUTONES_LOG_ERROR(
                "runtime.features",
                std::string(runtimeName) + " cleanup threw after startup failure");
        }

        TUTONES_LOG_WARN(
            "runtime.features",
            std::string(runtimeName)
                + " is unavailable; Tutones will continue with all other runtimes");
        return false;
    }
}
