#pragma once
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

// Shared hook system for multi-feature hooks
// Use this when multiple features need to hook the same function

enum class SharedHookPhase {
    Pre = 0,
    Post = 1
};

struct SharedHookCallback {
    std::string feature_name;
    std::string callback_name;
    std::function<void()> callback;
    int priority;
    bool enabled;
    SharedHookPhase phase;
    
    SharedHookCallback(const std::string& feat, const std::string& name,
                      std::function<void()> cb, int prio = 0, bool en = true,
                      SharedHookPhase ph = SharedHookPhase::Post)
        : feature_name(feat), callback_name(name), callback(cb), priority(prio), enabled(en), phase(ph) {}
};

class SharedHookManager {
public:
    static SharedHookManager& Instance();
    
    // Register a callback for a shared hook
    void RegisterCallback(const std::string& hook_name, const std::string& feature_name,
                         const std::string& callback_name, std::function<void()> callback,
                         int priority = 0, SharedHookPhase phase = SharedHookPhase::Post);
    
    // Call all registered callbacks for a hook and phase (in priority order)
    void DispatchHook(const std::string& hook_name, SharedHookPhase phase);
    
    // Enable/disable specific feature callback
    void EnableCallback(const std::string& hook_name, const std::string& feature_name, bool enabled);
    
    // Get callback info for debugging
    std::vector<SharedHookCallback>& GetCallbacks(const std::string& hook_name);
    
    // Print registered callbacks for debugging
    void PrintHookInfo(const std::string& hook_name);

private:
    SharedHookManager() = default;
    ~SharedHookManager() = default;
    SharedHookManager(const SharedHookManager&) = delete;
    SharedHookManager& operator=(const SharedHookManager&) = delete;
    
    std::unordered_map<std::string, std::vector<SharedHookCallback>> hook_callbacks;
    
    void SortCallbacksByPriority(std::vector<SharedHookCallback>& callbacks);
};

// Convenience macros for shared hooks

// Register a callback for a shared hook (auto-registration) with phase
#define REGISTER_SHARED_HOOK_CALLBACK(hook_name, feature_name, callback_name, priority, phase) \
    namespace { \
        struct AutoSharedHook_##hook_name##_##feature_name##_##callback_name { \
            AutoSharedHook_##hook_name##_##feature_name##_##callback_name() { \
                SharedHookManager::Instance().RegisterCallback( \
                    #hook_name, #feature_name, #callback_name, \
                    []() { callback_name(); }, priority, SharedHookPhase::phase); \
            } \
        }; \
        static AutoSharedHook_##hook_name##_##feature_name##_##callback_name \
            g_AutoSharedHook_##hook_name##_##feature_name##_##callback_name; \
    }

// Create a shared hook dispatcher (put this where the original function was hooked)
#define DISPATCH_SHARED_HOOK(hook_name, phase) \
    SharedHookManager::Instance().DispatchHook(#hook_name, SharedHookPhase::phase)
