#pragma once

#include "LogRecord.hpp"

namespace Tutones::Core::Logging
{
    class ILogSink
    {
    public:
        virtual ~ILogSink() = default;

        virtual void Write(const LogRecord& record) noexcept = 0;
        virtual void Flush() noexcept = 0;
    };
}
