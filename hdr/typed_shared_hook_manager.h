#pragma once

#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <type_traits>
#include "util.h"

template<typename Ret, typename... Args>
class TypedSharedHookManager {
public:
    struct TypedCallback {
        std::string feature_name;
        std::string callback_name;
        int priority;
        bool enabled;
        
        TypedCallback(const std::string& feat, const std::string& name, int prio = 0, bool en = true)
            : feature_name(feat), callback_name(name), priority(prio), enabled(en) {}
    };
    
    struct PreCallback : TypedCallback {
        std::function<void(Args&...)> callback;
        
        PreCallback(const std::string& feat, const std::string& name,
                   std::function<void(Args&...)> cb, int prio = 0, bool en = true)
            : TypedCallback(feat, name, prio, en), callback(cb) {}
    };
    
    struct PostCallback : TypedCallback {
        template<typename CB>
        PostCallback(const std::string& feat, const std::string& name,
                    CB cb, int prio = 0, bool en = true)
            : TypedCallback(feat, name, prio, en), callback_impl(cb) {}
        
        template<typename R = Ret>
        typename std::enable_if<std::is_void<R>::value, void>::type call(Args... args) {
            callback_impl.func(args...);
        }
        template<typename R = Ret>
        typename std::enable_if<!std::is_void<R>::value, R>::type call(R result, Args... args) {
            return callback_impl.func(result, args...);
        }
        
    private:
        struct VoidCallback {
            std::function<void(Args...)> func;
            template<typename CB> VoidCallback(CB cb) : func(cb) {}
        };
        struct NonVoidCallback {
            std::function<Ret(Ret, Args...)> func;
            template<typename CB> NonVoidCallback(CB cb) : func(cb) {}
        };
        typename std::conditional<std::is_void<Ret>::value, VoidCallback, NonVoidCallback>::type callback_impl;
    };
    
    void RegisterPreCallback(const std::string& feature_name, const std::string& callback_name,
                            std::function<void(Args&...)> callback, int priority = 0) {
        pre_callbacks.emplace_back(feature_name, callback_name, callback, priority, true);
        SortCallbacksByPriority(pre_callbacks);
        
        PrintOut(PRINT_LOG, "Registered typed pre-callback: %s::%s (priority %d)\n",
                 feature_name.c_str(), callback_name.c_str(), priority);
    }
    
    template<typename CB>
    void RegisterPostCallback(const std::string& feature_name, const std::string& callback_name,
                              CB callback, int priority = 0) {
        post_callbacks.emplace_back(feature_name, callback_name, callback, priority, true);
        SortCallbacksByPriority(post_callbacks);
        
        PrintOut(PRINT_LOG, "Registered typed post-callback: %s::%s (priority %d) - total post callbacks: %zu\n",
                 feature_name.c_str(), callback_name.c_str(), priority, post_callbacks.size());
    }
    
    void DispatchPre(Args&... args) {
        for (auto& callback : pre_callbacks) {
            if (callback.enabled) {
                try {
                    callback.callback(args...);
                } catch (...) {
                    PrintOut(PRINT_BAD, "Exception in typed pre-callback: %s::%s\n",
                             callback.feature_name.c_str(), callback.callback_name.c_str());
                }
            }
        }
    }
    
    template<typename R = Ret>
    typename std::enable_if<std::is_void<R>::value, void>::type DispatchPost(Args... args);
    
    template<typename R = Ret>
    typename std::enable_if<!std::is_void<R>::value, R>::type DispatchPost(R result, Args... args);
    
    void EnableCallback(const std::string& feature_name, bool enabled) {
        for (auto& cb : pre_callbacks) {
            if (cb.feature_name == feature_name) {
                cb.enabled = enabled;
            }
        }
        for (auto& cb : post_callbacks) {
            if (cb.feature_name == feature_name) {
                cb.enabled = enabled;
            }
        }
    }
    
    size_t GetPreCallbackCount() const { return pre_callbacks.size(); }
    size_t GetPostCallbackCount() const { return post_callbacks.size(); }

private:
    std::vector<PreCallback> pre_callbacks;
    std::vector<PostCallback> post_callbacks;
    
    void SortCallbacksByPriority(std::vector<PreCallback>& callbacks) {
        std::sort(callbacks.begin(), callbacks.end(),
                  [](const PreCallback& a, const PreCallback& b) {
                      return a.priority > b.priority;
                  });
    }
    
    void SortCallbacksByPriority(std::vector<PostCallback>& callbacks) {
        std::sort(callbacks.begin(), callbacks.end(),
                  [](const PostCallback& a, const PostCallback& b) {
                      return a.priority > b.priority;
                  });
    }
};

template<typename Ret, typename... Args>
template<typename R>
typename std::enable_if<std::is_void<R>::value, void>::type
TypedSharedHookManager<Ret, Args...>::DispatchPost(Args... args) {
    for (auto& callback : post_callbacks) {
        if (callback.enabled) {
            try {
                callback.call(args...);
            } catch (...) {
                PrintOut(PRINT_BAD, "Exception in typed post-callback: %s::%s\n",
                         callback.feature_name.c_str(), callback.callback_name.c_str());
            }
        }
    }
}

template<typename Ret, typename... Args>
template<typename R>
typename std::enable_if<!std::is_void<R>::value, R>::type
TypedSharedHookManager<Ret, Args...>::DispatchPost(R result, Args... args) {
    for (auto& callback : post_callbacks) {
        if (callback.enabled) {
            try {
                result = callback.call(result, args...);
            } catch (...) {
                PrintOut(PRINT_BAD, "Exception in typed post-callback: %s::%s\n",
                         callback.feature_name.c_str(), callback.callback_name.c_str());
            }
        }
    }
    return result;
}

