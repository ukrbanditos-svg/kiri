#include "KiriVNXP3Adaptive.h"
#include "KiriVNCompat.h"
#include "StorageImpl.h"
#include "tjs.h"

using namespace TJS;

namespace {
enum class DataArchiveMode {
    Unknown = 0,
    EncryptedCx,
    PlainRepack
};

enum class StreamArchiveKind {
    Unknown = 0,
    DataXp3 = 1,
    OtherXp3 = 2
};

static DataArchiveMode gDataMode = DataArchiveMode::Unknown;

static bool EndsWith(const ttstr &value, const tjs_char *suffix) {
    const tjs_int n = value.GetLen();
    const tjs_int m = (tjs_int)TJS_strlen(suffix);
    if (n < m) return false;
    return TJS_strcmp(value.c_str() + n - m, suffix) == 0;
}

static bool IsTextFile(const ttstr &name) {
    ttstr lower = name.AsLowerCase();
    const tjs_char *suffixes[] = {
        TJS_W(".tjs"), TJS_W(".ks"), TJS_W(".txt"), TJS_W(".csv")
    };
    for (const tjs_char *suffix : suffixes) {
        if (EndsWith(lower, suffix)) return true;
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

static bool IsRuiTomoProfileActive() {
    return ttstr(KiriVNDetectProfileId(TVPGetAppPath())) == TJS_W("ruitomo_fve");
}
}

void KiriVNResetXP3AdaptiveMode() {
    gDataMode = DataArchiveMode::Unknown;
}

void KiriVNPrepareXP3Context(const ttstr &archiveName, tTJSVariant *ctx) {
    if (!ctx || !IsRuiTomoProfileActive()) return;

    ttstr lower = archiveName.AsLowerCase();
    if (EndsWith(lower, TJS_W("data.xp3"))) {
        *ctx = (tjs_int64)StreamArchiveKind::DataXp3;
    } else {
        // Translation layout: data.xp3 is repacked/plain, while the sibling
        // archives (bgimage/fgimage/voice/etc.) are the original Cx archives.
        *ctx = (tjs_int64)StreamArchiveKind::OtherXp3;
    }
}

bool KiriVNShouldBypassXP3Filter(const tTVPXP3ExtractionFilterInfo *info, tTJSVariant *ctx) {
    if (!info || !IsRuiTomoProfileActive()) return false;

    StreamArchiveKind kind = StreamArchiveKind::Unknown;
    if (ctx && ctx->Type() == tvtInteger) {
        kind = (StreamArchiveKind)ctx->AsInteger();
    }

    // Original sibling archives must keep the RuiTomo Cx filter.
    if (kind == StreamArchiveKind::OtherXp3) return false;

    if (kind == StreamArchiveKind::DataXp3) {
        if (gDataMode == DataArchiveMode::PlainRepack) return true;
        if (gDataMode == DataArchiveMode::EncryptedCx) return false;

        // Determine the translated data.xp3 once from a real text file.
        if (info->Offset == 0 && IsTextFile(info->FileName) && info->BufferSize >= 12) {
            if (LooksLikePlainScript(info)) {
                gDataMode = DataArchiveMode::PlainRepack;
                return true;
            }
            gDataMode = DataArchiveMode::EncryptedCx;
            return false;
        }

        return false;
    }

    return false;
}
