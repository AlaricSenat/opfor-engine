#pragma once

#include <memory>
#include <csignal>

#include "engine/core/log.hpp"

#ifdef OP4_PLATFORM_LINUX
# define OP4_DEBUGBREAK() raise(SIGTRAP)
# define OP4_ABORT() abort();
#else
# warning "Unsupported Platform!"
# define OP4_DEBUGBREAK() (void)0
# define OP4_ABORT() (void)0
#endif

#define OP4_CORE_ASSERT(x, ...) { if (!(x)) { OP4_CORE_ERROR("Assertion Failed: {0}\n", __VA_ARGS__); OP4_DEBUGBREAK(); } }
#define OP4_CORE_EXPECT(x, ...) { if (!(x)) { OP4_CORE_ERROR("Assertion Failed: {0}\n", __VA_ARGS__); OP4_ABORT(); } }
#define OP4_CORE_UNREACHABLE(...) { OP4_CORE_ERROR("Unreachable: {0}\n"); OP4_ABORT(); }

namespace opfor {

template <typename T>
using SharedPtr = std::shared_ptr<T>;

template <typename T>
using UniquePtr = std::unique_ptr<T>;

template <typename T, typename ... Args>
constexpr SharedPtr<T> MakeShared(Args&&... args)
{
	return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T, typename ... Args>
constexpr UniquePtr<T> MakeUnique(Args&&... args)
{
	return std::make_unique<T>(std::forward<Args>(args)...);
}

}