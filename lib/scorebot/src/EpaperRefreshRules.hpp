#pragma once

#include <cstdint>

namespace scorebot {

enum class EpaperRefresh { None, Partial, Full };

inline constexpr uint32_t kEpaperDailyRefreshMs = 24u * 60u * 60u * 1000u;
inline constexpr uint32_t kEpaperCleanupQuietMs = 30u * 1000u;
inline constexpr unsigned kEpaperMaxPartials = 100;

constexpr EpaperRefresh epaperRefresh(bool hasBaseline, bool changed, unsigned partials,
                                      uint32_t sinceChangeMs, uint32_t sinceFullMs) {
    if (!hasBaseline || sinceFullMs >= kEpaperDailyRefreshMs ||
        (partials >= kEpaperMaxPartials && (changed || sinceChangeMs >= kEpaperCleanupQuietMs))) {
        return EpaperRefresh::Full;
    }
    return changed ? EpaperRefresh::Partial : EpaperRefresh::None;
}

}  // namespace scorebot
