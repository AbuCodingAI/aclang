#ifndef AC_EVENT_LISTENER_HPP
#define AC_EVENT_LISTENER_HPP

#include <functional>
#include <unordered_map>
#include <string>

namespace ac {

class EventListener {
private:
    std::unordered_map<std::string, std::function<void()>> bindings;

public:
    void bind(const std::string& key, std::function<void()> callback) {
        bindings[key] = callback;
    }

    void trigger(const std::string& key) {
        auto it = bindings.find(key);
        if (it != bindings.end()) {
            it->second();
        }
        // If no binding exists, do nothing
    }

    bool has_binding(const std::string& key) const {
        return bindings.find(key) != bindings.end();
    }

    void check_and_trigger(const std::string& key) {
        // Automatically check if key has a binding and trigger it
        // If no binding, do absolutely nothing
        trigger(key);
    }
};

// Global event listener instance
static EventListener global_listener;

} // namespace ac

#endif // AC_EVENT_LISTENER_HPP
