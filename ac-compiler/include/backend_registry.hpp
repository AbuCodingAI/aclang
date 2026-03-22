#pragma once
#include "../include/ast.hpp"
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <stdexcept>

// Backend registry - eliminates if-else chain in main.cpp
class BackendRegistry {
public:
    using GeneratorFunc = std::function<std::string(const ASTNode&)>;
    using RunnerFunc = std::function<std::string(const std::string&)>;

    struct BackendInfo {
        std::string extension;
        GeneratorFunc generator;
        RunnerFunc runner;
        
        BackendInfo(const std::string& ext, GeneratorFunc gen, RunnerFunc run)
            : extension(ext), generator(std::move(gen)), runner(std::move(run)) {}
    };

private:
    static std::unordered_map<std::string, BackendInfo> backends;

public:
    // Register a backend
    static void registerBackend(const std::string& name, 
                               const std::string& extension,
                               GeneratorFunc generator,
                               RunnerFunc runner) {
        backends.emplace(name, BackendInfo(extension, std::move(generator), std::move(runner)));
    }

    // Check if backend exists
    static bool hasBackend(const std::string& name) {
        return backends.find(name) != backends.end();
    }

    // Get backend info
    static const BackendInfo& getBackend(const std::string& name) {
        auto it = backends.find(name);
        if (it == backends.end()) {
            throw std::runtime_error("Backend '" + name + "' not registered");
        }
        return it->second;
    }

    // Get all registered backend names
    static std::vector<std::string> getBackendNames() {
        std::vector<std::string> names;
        for (const auto& pair : backends) {
            names.push_back(pair.first);
        }
        return names;
    }

    // Initialize all standard backends
    static void initializeStandardBackends();
};

// RAII helper for automatic backend registration
class BackendRegistrar {
public:
    BackendRegistrar(const std::string& name,
                    const std::string& extension,
                    BackendRegistry::GeneratorFunc generator,
                    BackendRegistry::RunnerFunc runner) {
        BackendRegistry::registerBackend(name, extension, std::move(generator), std::move(runner));
    }
};

// Macro for easy backend registration
#define REGISTER_BACKEND(name, ext, gen_func, runner_func) \
    static BackendRegistrar _registrar_##name(name, ext, gen_func, runner_func);
