#include "KiriVNCompat.h"
#include "StorageIntf.h"
#include "KiriVNGeneratedProfiles.h"

using namespace TJS;

static ttstr JoinPath(const ttstr &root, const tjs_char *relative) {
    ttstr out(root);
    if (!out.IsEmpty()) {
        const tjs_char *p = out.c_str();
        tjs_int len = out.GetLen();
        if (len > 0 && p[len - 1] != TJS_W("/")[0] && p[len - 1] != TJS_W("\\")[0]) {
            out += TJS_W("/");
        }
    }
    out += relative;
    return out;
}

static bool Exists(const ttstr &root, const tjs_char *relative) {
    return TVPIsExistentStorageNoSearch(JoinPath(root, relative));
}

static bool IsRuiTomoFVE(const ttstr &root) {
    if (Exists(root, TJS_W("kirivn_ruitomo_fve.profile"))) return true;
    if (Exists(root, TJS_W("ruitomo_fve.exe"))) return true;

    const bool hasStartup = Exists(root, TJS_W("startup.tjs"));
    const bool hasScenario = Exists(root, TJS_W("scenario/ruitomo09.ks")) ||
                             Exists(root, TJS_W("scenario/ruitomo10.ks"));
    const bool hasVoice = Exists(root, TJS_W("voice.xp3")) ||
                          Exists(root, TJS_W("Voice")) ||
                          Exists(root, TJS_W("voice"));
    return hasStartup && hasScenario && hasVoice;
}

const char *KiriVNDetectProfileId(const ttstr &appPath) {
    if (IsRuiTomoFVE(appPath)) return "ruitomo_fve";
    return "";
}

bool KiriVNGetBuiltInXP3FilterScript(const ttstr &appPath, ttstr &script) {
    if (!IsRuiTomoFVE(appPath)) return false;
    script = ttstr(KiriVNProfiles::RuiTomoFveXp3Filter);
    return !script.IsEmpty();
}
