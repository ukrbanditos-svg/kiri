#include "KiriVNXP3Adaptive.h"
#include "KiriVNCompat.h"
#include "StorageImpl.h"
#include "tjs.h"

using namespace TJS;

namespace {
enum class ArchiveMode {
    Unknown = 0,
    EncryptedCx,
    PlainRepack
};

static ArchiveMode gMode = ArchiveMode::Unknown;

static bool IsTextFile(const ttstr &name) {
    ttstr lower = name.AsLowerCase();
    const tjs_char *p = lower.c_str();
    const tjs_int n = lower.GetLen();

    const tjs_char *suffixes[] = {
        TJS_W(".tjs"), TJS_W(".ks"), TJS_W(".txt"), TJS_W(".csv")
    };
    for (const tjs_char *suffix : suffixes) {
        const tjs_int m = (tjs_int)TJS_strlen(suffix);
        if (n >= m && TJS_strcmp(p + n - m, suffix) == 0) return true;
    }
    return false;
}

static bool LooksLikePlainUtf16LE(const tjs_uint8 *b, tjs_uint n) {
    if (n >= 2 && b[0] == 0xff && b[1] == 0xfe) return true;
    if (n < 8 || (n & 1)) return false;

    tjs_uint pairs = n / 2;
    if (pairs > 128) pairs = 128;

    tjs_uint plausible = 0;
    tjs_uint bad = 0;
    tjs_uint zeroHigh = 0;

    for (tjs_uint i = 0; i < pairs; ++i) {
        const tjs_uint16 wc =
            (tjs_uint16)b[i * 2] | ((tjs_uint16)b[i * 2 + 1] << 8);

        if (b[i * 2 + 1] == 0) ++zeroHigh;

        const bool ok =
            wc == 9 || wc == 10 || wc == 13 ||
            (wc >= 0x20 && wc <= 0x7e) ||
            (wc >= 0x00a0 && wc <= 0x024f) ||
            (wc >= 0x0400 && wc <= 0x052f) ||
            (wc >= 0x2000 && wc <= 0x206f) ||
            (wc >= 0x3000 && wc <= 0x30ff) ||
            (wc >= 0x3400 && wc <= 0x9fff) ||
            (wc >= 0xf900 && wc <= 0xfaff) ||
            (wc >= 0xff00 && wc <= 0xffef);

        if (ok) ++plausible;
        if ((wc < 0x20 && wc != 9 && wc != 10 && wc != 13) ||
            (wc >= 0xd800 && wc <= 0xdfff) ||
            wc == 0xfffe || wc == 0xffff) {
            ++bad;
        }
    }

    return plausible * 100 >= pairs * 90 &&
           bad * 100 <= pairs * 2 &&
           zeroHigh * 5 >= pairs;
}

static bool LooksLikePlainNarrowText(const tjs_uint8 *b, tjs_uint n) {
    if (n >= 3 && b[0] == 0xef && b[1] == 0xbb && b[2] == 0xbf) return true;
    if (n < 12) return false;

    tjs_uint asciiText = 0;
    tjs_uint controls = 0;
    tjs_uint syntax = 0;
    tjs_uint lineBreaks = 0;

    const tjs_uint count = n > 128 ? 128 : n;
    for (tjs_uint i = 0; i < count; ++i) {
        const tjs_uint8 c = b[i];
        if (c == 9 || c == 10 || c == 13 || (c >= 0x20 && c <= 0x7e))
            ++asciiText;
        if (c < 0x20 && c != 9 && c != 10 && c != 13)
            ++controls;
        if (c == 10 || c == 13)
            ++lineBreaks;
        if (c == '/' || c == ';' || c == '{' || c == '}' ||
            c == '=' || c == '(' || c == ')' || c == '@')
            ++syntax;
    }

    return asciiText * 100 >= count * 72 &&
           controls * 100 <= count * 3 &&
           (syntax >= 2 || lineBreaks >= 1);
}

static bool LooksLikePlainScript(const tTVPXP3ExtractionFilterInfo *info) {
    if (!info || !info->Buffer || !info->BufferSize) return false;
    const tjs_uint8 *b = static_cast<const tjs_uint8 *>(info->Buffer);
    const tjs_uint n = info->BufferSize;
    return LooksLikePlainUtf16LE(b, n) || LooksLikePlainNarrowText(b, n);
}
}

void KiriVNResetXP3AdaptiveMode() {
    gMode = ArchiveMode::Unknown;
}

bool KiriVNShouldBypassXP3Filter(const tTVPXP3ExtractionFilterInfo *info) {
    if (!info) return false;

    // Only use this compatibility heuristic for RuiTomo. Other games keep
    // the normal Kirikiroid/Yuri extraction-filter behavior.
    if (ttstr(KiriVNDetectProfileId(TVPGetAppPath())) != TJS_W("ruitomo_fve"))
        return false;

    if (gMode == ArchiveMode::PlainRepack) return true;
    if (gMode == ArchiveMode::EncryptedCx) return false;

    // Decide once from the beginning of the first text script we see.
    if (info->Offset == 0 && IsTextFile(info->FileName) && info->BufferSize >= 12) {
        if (LooksLikePlainScript(info)) {
            gMode = ArchiveMode::PlainRepack;
            return true;
        }

        gMode = ArchiveMode::EncryptedCx;
        return false;
    }

    // Unknown: preserve original Cx behavior until a sufficiently large
    // text probe is available.
    return false;
}
