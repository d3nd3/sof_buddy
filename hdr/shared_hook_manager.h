#pragma once
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <any>
#include <algorithm>
#include "util.h"

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

struct SharedHookCallbackBase {
    std::string feature_name;
    std::string callback_name;
    int priority;
    bool enabled;
    SharedHookPhase phase;
    
    SharedHookCallbackBase(const std::string& feat, const std::string& name,
                          int prio = 0, bool en = true, SharedHookPhase ph = SharedHookPhase::Post)
        : feature_name(feat), callback_name(name), priority(prio), enabled(en), phase(ph) {}
    virtual ~SharedHookCallbackBase() = default;
};

template<typename... Args>
struct SharedHookCallbackWithArgs : public SharedHookCallbackBase {
    std::function<void(Args...)> callback;
    
    SharedHookCallbackWithArgs(const std::string& feat, const std::string& name,
                              std::function<void(Args...)> cb, int prio = 0, bool en = true,
                              SharedHookPhase ph = SharedHookPhase::Post)
        : SharedHookCallbackBase(feat, name, prio, en, ph), callback(cb) {}
};

class SharedHookManager {
public:
    static SharedHookManager& Instance();
    
    // Register a callback for a shared hook (no parameters)
    void RegisterCallback(const std::string& hook_name, const std::string& feature_name,
                         const std::string& callback_name, std::function<void()> callback,
                         int priority = 0, SharedHookPhase phase = SharedHookPhase::Post);
    
    // Register a parameterized callback for a shared hook
    template<typename... Args>
    void RegisterCallback(const std::string& hook_name, const std::string& feature_name,
                         const std::string& callback_name, std::function<void(Args...)> callback,
                         int priority = 0, SharedHookPhase phase = SharedHookPhase::Post) {
        auto cb = std::make_shared<SharedHookCallbackWithArgs<Args...>>(feature_name, callback_name, callback, priority, true, phase);
        hook_callbacks_with_args[hook_name].push_back(cb);
        SortCallbacksByPriority(hook_callbacks_with_args[hook_name]);
        
        PrintOut(PRINT_LOG, "Registered shared hook callback: %s::%s for hook %s (priority %d, phase %s, with parameters)\n", 
                 feature_name.c_str(), callback_name.c_str(), hook_name.c_str(), priority,
                 phase == SharedHookPhase::Pre ? "Pre" : "Post");
    }
    
    // Call all registered callbacks for a hook and phase (in priority order, no parameters)
    void DispatchHook(const std::string& hook_name, SharedHookPhase phase);
    
    // Call all registered parameterized callbacks for a hook and phase (in priority order)
    template<typename... Args>
    void DispatchHook(const std::string& hook_name, SharedHookPhase phase, Args... args) {
        auto it = hook_callbacks_with_args.find(hook_name);
        if (it != hook_callbacks_with_args.end()) {
            for (auto& callback_ptr : it->second) {
                auto* callback = dynamic_cast<SharedHookCallbackWithArgs<Args...>*>(callback_ptr.get());
                if (callback && callback->enabled && callback->phase == phase) {
                    try {
                        callback->callback(args...);
                    } catch (...) {
                        PrintOut(PRINT_BAD, "Exception in shared hook callback: %s::%s\n", 
                                 callback->feature_name.c_str(), callback->callback_name.c_str());
                    }
                }
            }
        }
        // Also dispatch non-parameterized callbacks
        DispatchHook(hook_name, phase);
    }
    
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
    std::unordered_map<std::string, std::vector<std::shared_ptr<SharedHookCallbackBase>>> hook_callbacks_with_args;
    
    void SortCallbacksByPriority(std::vector<SharedHookCallback>& callbacks);
    
    void SortCallbacksByPriority(std::vector<std::shared_ptr<SharedHookCallbackBase>>& callbacks) {
        std::sort(callbacks.begin(), callbacks.end(),
                  [](const std::shared_ptr<SharedHookCallbackBase>& a, const std::shared_ptr<SharedHookCallbackBase>& b) {
                      return a->priority > b->priority;
                  });
    }
};

// Create a shared hook (put this where the original function was hooked)
#define DISPATCH_SHARED_HOOK(hook_name, phase) \
    SharedHookManager::Instance().DispatchHook(#hook_name, SharedHookPhase::phase);

// Create a shared hook with parameters (put this where the original function was hooked)
#define DISPATCH_SHARED_HOOK_ARGS(hook_name, phase, ...) \
    SharedHookManager::Instance().DispatchHook(#hook_name, SharedHookPhase::phase, __VA_ARGS__);
