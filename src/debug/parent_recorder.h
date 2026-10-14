#pragma once

#include <string>
#include <map>
#include <set>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "debug/callsite_classifier.h"

// Records unique parent function starts (per module) for a given child hook name
class ParentRecorder {
public:
    static ParentRecorder &Instance();

    // Pass relative ("rsrc/func_parents") or absolute directory. If nullptr, derive next to DLL.
    void initialize(const char *parentsDir);

    // Record a parent for the given child hook label when functionStartRva is known (non-zero)
    void record(const char *childName, const CallerInfo &info);
    void flushAll();

private:
    ParentRecorder();

    struct ParentKey {
        Module module;
        uint32_t fnStartRva;
        bool operator<(const ParentKey &o) const {
            if (module != o.module) return (int)module < (int)o.module;
            return fnStartRva < o.fnStartRva;
        }
    };

    struct ChildSet {
        std::set<ParentKey> parents;
        bool dirty = false;
    };

    std::map<std::string, ChildSet> _childToParents;
    std::string _parentsDir; // absolute on disk

    void ensureParentsDir();
    void loadExistingData();
    void flushChildToDisk(const std::string &childName, const ChildSet &set);
    static const char *moduleToLeaf(Module m);
    static Module moduleFromString(const char *str);
};


