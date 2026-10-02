#pragma once

#include "XP3Archive.h"
#include "tjs.h"

void KiriVNResetXP3AdaptiveMode();
void KiriVNPrepareXP3Context(const TJS::ttstr &archiveName, TJS::tTJSVariant *ctx);
bool KiriVNShouldBypassXP3Filter(const tTVPXP3ExtractionFilterInfo *info, TJS::tTJSVariant *ctx);
