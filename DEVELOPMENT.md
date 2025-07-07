# Verge Core Development Guide

This document outlines the development standards, practices, and guidelines for contributing to Verge Core.

## Modern C++ Migration

Verge Core is actively migrating from legacy C++ patterns to modern C++17/20 standards. This initiative focuses on improving performance, security, and maintainability while reducing external dependencies.

### Migration Goals

1. **C++17 Standard Adoption**: Full transition from C++14 to C++17 with selective C++20 features
2. **Boost Dependency Reduction**: Replace Boost components with standard library equivalents
3. **Memory Safety**: Implement RAII patterns and smart pointer usage
4. **Thread Safety**: Modern concurrency patterns with standard library primitives
5. **Type Safety**: Leverage modern C++ type system features

## Coding Standards

### C++ Standard Library Over Boost

Replace Boost components with standard library equivalents where possible:

```cpp
// ❌ Old (Boost)
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/signals2.hpp>

boost::filesystem::path config_path = boost::filesystem::current_path();
boost::thread worker_thread([]() { /* work */ });
boost::signals2::signal<void()> event_signal;

// ✅ New (Standard Library)
#include <filesystem>
#include <thread>
#include <functional>

std::filesystem::path config_path = std::filesystem::current_path();
std::thread worker_thread([]() { /* work */ });
std::function<void()> event_callback;
```

### Memory Management

Use smart pointers and RAII patterns:

```cpp
// ❌ Old (Raw Pointers)
CNode* pnode = new CNode(params);
// ... risky manual memory management
delete pnode;

// ✅ New (Smart Pointers)
auto pnode = std::make_unique<CNode>(params);
// ... automatic cleanup
```

### Thread Safety

Use standard library concurrency primitives:

```cpp
// ❌ Old (Bitcoin Core legacy)
CCriticalSection cs_main;
LOCK(cs_main);

// ✅ New (Standard Library)
std::shared_mutex main_mutex;
std::shared_lock<std::shared_mutex> lock(main_mutex);
```

### Type Safety and Const Correctness

```cpp
// ✅ Good practices
class BlockValidator {
    mutable std::shared_mutex validation_mutex_;
    
public:
    [[nodiscard]] ValidationResult ValidateBlock(const CBlock& block) const noexcept;
    [[nodiscard]] bool IsValid() const noexcept { return is_valid_; }
    
private:
    std::atomic<bool> is_valid_{false};
};
```

### Error Handling

Use modern error handling patterns:

```cpp
// ✅ Modern error handling
#include <optional>
#include <expected> // C++23, use tl::expected for C++17

std::optional<CBlock> ParseBlock(const std::vector<uint8_t>& data) noexcept;
std::expected<ValidationResult, ValidationError> ValidateTransaction(const CTransaction& tx);
```

## Architecture Guidelines

### Dependency Injection

Use dependency injection for better testability:

```cpp
class ChainStateManager {
    std::unique_ptr<BlockValidator> validator_;
    std::unique_ptr<Database> database_;
    
public:
    ChainStateManager(std::unique_ptr<BlockValidator> validator,
                     std::unique_ptr<Database> database)
        : validator_(std::move(validator))
        , database_(std::move(database)) {}
};
```

### Interface Segregation

Define focused interfaces:

```cpp
class IBlockValidator {
public:
    virtual ~IBlockValidator() = default;
    virtual ValidationResult ValidateBlock(const CBlock& block) = 0;
    virtual ValidationResult ValidateTransaction(const CTransaction& tx) = 0;
};

class INetworkManager {
public:
    virtual ~INetworkManager() = default;
    virtual void SendMessage(NodeId node, const CNetMessage& msg) = 0;
    virtual void DisconnectNode(NodeId node) = 0;
};
```

### Factory Patterns

Use factory patterns for complex object creation:

```cpp
class MiningAlgorithmFactory {
public:
    static std::unique_ptr<IMiningAlgorithm> CreateAlgorithm(AlgorithmType type) {
        switch (type) {
            case AlgorithmType::Scrypt:
                return std::make_unique<ScryptAlgorithm>();
            case AlgorithmType::X17:
                return std::make_unique<X17Algorithm>();
            // ...
        }
    }
};
```

## Build System

### CMake Integration

We're migrating to CMake for better cross-platform support:

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(VergeCore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(verge-core PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -fstack-protector-strong
    )
endif()
```

### Dependency Management

Prefer system packages or vcpkg:

```cmake
# Find standard dependencies
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)

# Target-specific linking
target_link_libraries(verge-core PRIVATE
    Boost::system
    Boost::filesystem
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
)
```

## Testing Standards

### Unit Testing

Use modern testing frameworks:

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("BlockValidator validates correct blocks", "[validation]") {
    auto validator = std::make_unique<BlockValidator>();
    CBlock valid_block = CreateValidBlock();
    
    auto result = validator->ValidateBlock(valid_block);
    
    REQUIRE(result.IsValid());
    REQUIRE(result.GetError() == ValidationError::None);
}
```

### Benchmarking

Use standard benchmarking libraries:

```cpp
#include <benchmark/benchmark.h>

static void BM_BlockValidation(benchmark::State& state) {
    auto validator = std::make_unique<BlockValidator>();
    auto block = CreateTestBlock();
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(validator->ValidateBlock(block));
    }
}
BENCHMARK(BM_BlockValidation);
```

## Performance Guidelines

### Memory Optimization

- Use `std::string_view` for read-only string parameters
- Prefer move semantics for expensive-to-copy objects
- Use object pools for frequently allocated objects

```cpp
class BlockValidator {
    std::unordered_map<uint256, ValidationResult> cache_;
    
public:
    ValidationResult ValidateBlock(std::string_view block_data) const;
    void CacheResult(uint256 hash, ValidationResult result);
};
```

### Concurrency Patterns

- Use `std::atomic` for simple shared state
- Prefer `std::shared_mutex` for reader-writer scenarios
- Use thread pools for CPU-intensive work

```cpp
class ThreadPool {
    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    
public:
    template<typename F>
    auto submit(F&& f) -> std::future<decltype(f())>;
};
```

## Migration Timeline

### Phase 1: Foundation (Months 1-3)
- [x] Update README.md with modern requirements
- [ ] Replace `boost::filesystem` with `std::filesystem`
- [ ] Replace `CCriticalSection` with `std::mutex`
- [ ] Implement smart pointer usage in core classes

### Phase 2: Architecture (Months 4-6)
- [ ] Refactor validation layer with modern patterns
- [ ] Implement dependency injection for major components
- [ ] Create interface abstractions for mining algorithms
- [ ] Modernize network layer with standard library primitives

### Phase 3: Optimization (Months 7-9)
- [ ] Implement thread pools for parallel processing
- [ ] Optimize memory usage with custom allocators
- [ ] Add comprehensive benchmarking suite
- [ ] Performance regression testing

## Contributing

### Pull Request Guidelines

1. **Follow Modern C++ Standards**: All new code should use C++17 features
2. **Add Tests**: Include unit tests for new functionality
3. **Documentation**: Update relevant documentation
4. **Performance**: Consider performance implications of changes
5. **Memory Safety**: Use RAII and smart pointers

### Code Review Checklist

- [ ] Uses modern C++ patterns (C++17/20)
- [ ] Proper const correctness and noexcept specifications
- [ ] Thread-safe where applicable
- [ ] Includes appropriate tests
- [ ] Documentation updated
- [ ] No raw pointers for owned resources
- [ ] Proper error handling

### Development Environment

Recommended development setup:

```bash
# VS Code with C++ extensions
code --install-extension ms-vscode.cpptools
code --install-extension ms-vscode.cmake-tools

# Or CLion/Qt Creator for full IDE experience
```

## Resources

- [C++17 Features](https://en.cppreference.com/w/cpp/17)
- [Modern C++ Best Practices](https://github.com/isocpp/CppCoreGuidelines)
- [CMake Documentation](https://cmake.org/documentation/)
- [Catch2 Testing Framework](https://github.com/catchorg/Catch2)
- [Google Benchmark](https://github.com/google/benchmark) 