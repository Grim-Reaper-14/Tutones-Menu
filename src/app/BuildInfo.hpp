#pragma once

#ifndef TUTONES_GIT_REVISION
#define TUTONES_GIT_REVISION "unknown"
#endif

#ifndef TUTONES_BUILD_NUMBER
#define TUTONES_BUILD_NUMBER "local"
#endif

namespace Tutones::App::BuildInfo
{
    inline constexpr const char Revision[] = TUTONES_GIT_REVISION;
    inline constexpr const char BuildNumber[] = TUTONES_BUILD_NUMBER;
    inline constexpr const char Label[] = TUTONES_GIT_REVISION " #" TUTONES_BUILD_NUMBER;
}
