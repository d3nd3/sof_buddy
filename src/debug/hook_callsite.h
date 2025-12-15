#pragma once

#include <stdint.h>
#include "debug/callsite_classifier.h"

namespace HookCallsite {

void cacheSelfModule();

bool classifyCaller(CallerInfo &out);
bool classifyExternalCaller(CallerInfo &out);
uint32_t getCallerFnStart();
uint32_t GetFnStartExternal();
uint32_t GetFnStart();
void recordParent(const char *childName);
uint32_t recordAndGetFnStart(const char *childName);
uint32_t recordAndGetFnStartExternal(const char *childName);

}


