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

    // Common Android repacks/translation bundles keep the original encrypted
    // data.xp3 but omit the Windows exe. The folder name is usually ruitomo*
    // (for example ruitomo_ru_game), so use that as a guarded fallback.
    ttstr lowerRoot(root);
    lowerRoot.ToLowerCase();
    const bool pathSaysRuiTomo =
        lowerRoot.IndexOf(ttstr(TJS_W("ruitomo"))) >= 0 ||
        lowerRoot.IndexOf(ttstr(TJS_W("るいは智を呼ぶ"))) >= 0;
    if (pathSaysRuiTomo && Exists(root, TJS_W("data.xp3"))) return true;

    const bool hasStartup = Exists(root, TJS_W("startup.tjs"));
    const bool hasScenario = Exists(root, TJS_W("scenario/ruitomo09.ks")) ||
                             Exists(root, TJS_W("scenario/ruitomo10.ks"));
    return hasStartup && hasScenario;
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
