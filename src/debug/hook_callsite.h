#pragma once

#include <stdint.h>
#include "debug/callsite_classifier.h"

namespace HookCallsite {

void cacheSelfModule();

bool classifyCaller(CallerInfo &out);
bool classifyExternalCaller(CallerInfo &out);
bool classifyStackCallerInModule(Module target, CallerInfo &out);
typedef bool (*ExternalCallerVisitor)(const CallerInfo &info, void *ctx);
bool visitExternalCallers(ExternalCallerVisitor visitor, void *ctx);
uint32_t getCallerFnStart();
uint32_t GetFnStartExternal();
uint32_t GetFnStart();
void recordParent(const char *childName);
uint32_t recordAndGetFnStart(const char *childName);
uint32_t recordAndGetFnStartExternal(const char *childName);
uint32_t recordAndGetCallerRvaExternal(const char *childName);
uint32_t recordAndGetSofExeCallerRva(const char *childName);

}


