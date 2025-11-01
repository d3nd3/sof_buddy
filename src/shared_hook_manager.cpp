/*
 * Shared Hook Manager - Callback Coordination System
 * 
 * Allows multiple features to register callbacks for the same hooks.
 * Executes callbacks in priority order with Pre/Post phase support.
 * 
 * We put all the initial hooks for shared hook into the dispatcher folder.
 * Usage Flow:
 * 1. Initial hook: Use REGISTER_HOOK/REGISTER_HOOK_VOID/REGISTER_HOOK_LEN/REGISTER_MODULE_HOOK
 *    to install the detour on the target function.
 * 2. Dispatch: Inside the hook function, call DISPATCH_SHARED_HOOK(hook_name, phase) to dispatch
 *    to all registered callbacks.
 * 3. Registration: Features use REGISTER_SHARED_HOOK_CALLBACK to register callbacks for the hook.
 * 
 * Provides lifecycle hooks: EarlyStartup, PreCvarInit, PostCvarInit, RefDllLoaded, GameDllLoaded
 * Prevents duplicate hooking and enables cooperative feature architecture.
 */

#include "shared_hook_manager.h"
#include "util.h"
#include <algorithm>

SharedHookManager& SharedHookManager::Instance() {
    static SharedHookManager instance;
    return instance;
}

void SharedHookManager::RegisterCallback(const std::string& hook_name, const std::string& feature_name,
                                       const std::string& callback_name, std::function<void()> callback,
                                       int priority, SharedHookPhase phase) {
    hook_callbacks[hook_name].emplace_back(feature_name, callback_name, callback, priority, true, phase);
    SortCallbacksByPriority(hook_callbacks[hook_name]);
    
    PrintOut(PRINT_LOG, "Registered shared hook callback: %s::%s for hook %s (priority %d, phase %s)\n", 
             feature_name.c_str(), callback_name.c_str(), hook_name.c_str(), priority,
             phase == SharedHookPhase::Pre ? "Pre" : "Post");
}

/*
 * This function is responsible for dispatching shared hooks. It takes a hook name and a phase (pre or post) as input.
 * It iterates through all registered callbacks for the given hook name and executes the ones that are enabled and match the specified phase.
 * If a callback throws an exception, it catches it and prints an error message.
 */
void SharedHookManager::DispatchHook(const std::string& hook_name, SharedHookPhase phase) {
    auto it = hook_callbacks.find(hook_name);
    if (it == hook_callbacks.end()) {
        return; // No callbacks registered
    }
    
    for (auto& callback : it->second) {
        if (callback.enabled && callback.phase == phase) {
            try {
                callback.callback();
            } catch (...) {
                PrintOut(PRINT_BAD, "Exception in shared hook callback: %s::%s\n", 
                         callback.feature_name.c_str(), callback.callback_name.c_str());
            }
        }
    }
}

// Optional feature: Temporarily disable/enable specific feature callbacks at runtime.
// Not currently used in the project, but available for debugging or dynamic hook management.
void SharedHookManager::EnableCallback(const std::string& hook_name, const std::string& feature_name, bool enabled) {
    auto it = hook_callbacks.find(hook_name);
    if (it == hook_callbacks.end()) {
        return;
    }
    
    for (auto& callback : it->second) {
        if (callback.feature_name == feature_name) {
            callback.enabled = enabled;
            PrintOut(PRINT_LOG, "Shared hook callback %s::%s %s\n", 
                     feature_name.c_str(), callback.callback_name.c_str(),
                     enabled ? "enabled" : "disabled");
        }
    }
}

std::vector<SharedHookCallback>& SharedHookManager::GetCallbacks(const std::string& hook_name) {
    return hook_callbacks[hook_name];
}

void SharedHookManager::PrintHookInfo(const std::string& hook_name) {
    auto it = hook_callbacks.find(hook_name);
    if (it == hook_callbacks.end()) {
        PrintOut(PRINT_LOG, "No callbacks registered for hook: %s\n", hook_name.c_str());
        return;
    }
    
    PrintOut(PRINT_LOG, "Hook '%s' has %zu callback(s):\n", hook_name.c_str(), it->second.size());
    for (size_t i = 0; i < it->second.size(); ++i) {
        const auto& cb = it->second[i];
        PrintOut(PRINT_LOG, "  %zu. %s::%s (priority %d, %s)\n", 
                 i + 1, cb.feature_name.c_str(), cb.callback_name.c_str(), 
                 cb.priority, cb.enabled ? "enabled" : "disabled");
    }
}

void SharedHookManager::SortCallbacksByPriority(std::vector<SharedHookCallback>& callbacks) {
    // Sort by priority (higher priority = earlier execution)
    std::sort(callbacks.begin(), callbacks.end(), 
              [](const SharedHookCallback& a, const SharedHookCallback& b) {
                  return a.priority > b.priority;
              });
}
