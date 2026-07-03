#pragma once

// Precompiled header: commonly used, stable C++ standard headers for the Core project
#pragma once

// Basic types & utilities
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <type_traits>
#include <utility>
#include <concepts>
#include <limits>

// Containers & algorithms
#include <array>
#include <vector>
#include <deque>
#include <queue>
#include <set>
#include <map>
#include <unordered_map>
#include <string>
#include <string_view>
#include <span>
#include <optional>
#include <variant>
#include <memory>
#include <functional>
#include <algorithm>
#include <numeric>
#include <iterator>

// Concurrency / threading (used by JobSystem, etc.)
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <future>
#include <chrono>

// C++20 utilities
#include <ranges>
#include <format> // optional: include if formatting is used across many TUs

// Note: avoid project headers here (e.g. Reflection/*.h) to prevent frequent PCH
// invalidation when project headers change.
