#!/usr/bin/env python3
from __future__ import annotations

import argparse
import pathlib
import re
import shutil

ROOT = pathlib.Path(__file__).resolve().parents[1]
OVERLAY = ROOT / "overlay"

RU = {
    "preference_title": "Общие настройки",
    "preference_title_individual": "Настройки игры",
    "preference_output_log": "Выводить лог",
    "preference_select_renderer": "Рендерер",
    "preference_opengl": "OpenGL (экспериментально)",
    "preference_software": "Программный",
    "preference_remember_last_path": "Запоминать последнюю папку",
    "preference_keep_screen_alive": "Не выключать экран",
    "preference_show_fps": "Показывать FPS",
    "preference_fps_limit": "Лимит FPS",
    "preference_hide_android_sys_btn": "Скрывать системную панель Android",
    "preference_default_font": "Шрифт по умолчанию",
    "preference_force_def_font": "Всегда использовать шрифт по умолчанию",
    "ok": "ОК",
    "cancel": "Отмена",
    "retry": "Повторить",
    "start": "Запуск",
    "stop": "Стоп",
    "continue_run": "Продолжить",
    "get_sdcard_permission": "Получить доступ",
    "menu_rotate": "Повернуть экран",
    "menu_global_preference": "Общие настройки",
    "menu_new_local_pref": "Создать настройки игры",
    "menu_local_pref": "Настройки игры",
    "menu_new_folder": "Новая папка",
    "menu_about": "О KiriVN",
    "menu_exit": "Выход",
    "menu_help": "Помощь",
    "sure_to_exit": "Выйти из KiriVN?",
    "msgbox_yes": "Да",
    "msgbox_no": "Нет",
    "msgbox_ok": "ОК",
    "use_last_path": "Последняя папка:",
    "notice": "Уведомление",
    "err_narrow_to_wide": "Не удалось преобразовать строку в Unicode.\nДанные могут быть зашифрованы, повреждены или иметь другую кодировку.\nKiriVN попробует встроенный профиль совместимости, если игра распознана.",
    "err_no_memory": "Недостаточно памяти.",
    "err_occured": "Произошла ошибка",
    "err_read_error": "Ошибка чтения",
    "delete": "Удалить",
    "device_info": "Информация об устройстве",
    "browse_patch_lib": "Библиотека патчей",
    "filemgr_unselect": "Отмена",
    "filemgr_browse": "Открыть",
    "filemgr_delete": "Удалить",
    "filemgr_copy": "Копировать",
    "filemgr_cut": "Вырезать",
    "filemgr_paste": "Вставить сюда",
    "filemgr_rename": "Переименовать",
}

def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"upstream changed: {label} not found")
    return text.replace(old, new, 1)

def copy_overlay(upstream: pathlib.Path) -> None:
    for src in OVERLAY.rglob("*"):
        if not src.is_file():
            continue
        rel = src.relative_to(OVERLAY)
        dst = upstream / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

def patch_gradle_properties(upstream: pathlib.Path) -> None:
    p = upstream / "project/android/gradle.properties"
    text = p.read_text(encoding="utf-8")
    text = re.sub(r"^PROP_COMPILE_SDK_VERSION=.*$", "PROP_COMPILE_SDK_VERSION=33", text, flags=re.M)
    text = re.sub(r"^PROP_TARGET_SDK_VERSION=.*$", "PROP_TARGET_SDK_VERSION=33", text, flags=re.M)
    p.write_text(text, encoding="utf-8", newline="\n")

def patch_gradle(upstream: pathlib.Path) -> None:
    p = upstream / "project/android/app/build.gradle"
    s = p.read_text(encoding="utf-8")
    s = replace_once(s, 'applicationId "com.yuri.kirikiri2"', 'applicationId "io.kirivn.player"', "applicationId")
    s = s.replace('versionName "1.4.0beta"', 'versionName "0.1.0-alpha"')
    debug_re = re.compile(r'(debug\s*\{.*?)(\n\s*signingConfig signingConfigs\.release)(.*?\n\s*\})', re.S)
    s, n = debug_re.subn(r'\1\3', s, count=1)
    if n != 1:
        raise RuntimeError("upstream changed: debug signingConfig not found")
    p.write_text(s, encoding="utf-8", newline="\n")

def patch_manifest(upstream: pathlib.Path) -> None:
    p = upstream / "project/android/app/AndroidManifest.xml"
    s = p.read_text(encoding="utf-8")
    s = s.replace('<uses-permission android:name="android.permission.READ_PHONE_STATE"/>\n', '')
    if "android.permission.MANAGE_EXTERNAL_STORAGE" not in s:
        marker = '<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE"\n        tools:ignore="ScopedStorage" />'
        s = replace_once(
            s, marker,
            marker + '\n    <uses-permission android:name="android.permission.MANAGE_EXTERNAL_STORAGE"\n        tools:ignore="ScopedStorage" />',
            "WRITE_EXTERNAL_STORAGE permission",
        )
    p.write_text(s, encoding="utf-8", newline="\n")

def patch_main_activity(upstream: pathlib.Path) -> None:
    p = upstream / "project/android/app/java/com/yuri/kirikiri2/MainActivity.java"
    s = p.read_text(encoding="utf-8")
    s = replace_once(s, "import org.tvp.kirikiri2.KR2Activity;", """import org.tvp.kirikiri2.KR2Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;""", "MainActivity imports")
    marker = "public class MainActivity extends KR2Activity {\n"
    addition = """public class MainActivity extends KR2Activity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !Environment.isExternalStorageManager()) {
            try {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse(\"package:\" + getPackageName()));
                startActivity(intent);
            } catch (Exception ignored) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        }
    }
"""
    s = replace_once(s, marker, addition, "MainActivity class")
    p.write_text(s, encoding="utf-8", newline="\n")

def patch_brand(upstream: pathlib.Path) -> None:
    p = upstream / "project/android/app/res/values/strings.xml"
    s = p.read_text(encoding="utf-8")
    s = re.sub(r'<string name="app_name">.*?</string>', '<string name="app_name">KiriVN</string>', s)
    p.write_text(s, encoding="utf-8", newline="\n")

    p = upstream / "src/core/environ/ui/MainFileSelectorForm.cpp"
    s = p.read_text(encoding="utf-8")
    s = s.replace('"Kirikiroid2")', '"KiriVN")')
    s = s.replace('"XP3Player")', '"KiriVN")')
    p.write_text(s, encoding="utf-8", newline="\n")

def patch_xp3filter(upstream: pathlib.Path) -> None:
    p = upstream / "src/plugins/xp3filter.cpp"
    s = p.read_text(encoding="utf-8")
    if '#include "KiriVNCompat.h"' not in s:
        s = replace_once(
            s,
            '#include "xp3filter.h"',
            '#include "xp3filter.h"\\n#include "KiriVNCompat.h"\\n#include "KiriVNXP3Adaptive.h"',
            "KiriVN include",
        )

    filter_hook_old = '''\tif (info->SizeOfSelf != sizeof(tTVPXP3ExtractionFilterInfo))
        TVPThrowExceptionMessage(TJS_W("Incompatible tTVPXP3ExtractionFilterInfo size"));
\tXP3FilterDecoder* decoder = FetchXP3Decoder();'''
    filter_hook_new = '''\tif (info->SizeOfSelf != sizeof(tTVPXP3ExtractionFilterInfo))
        TVPThrowExceptionMessage(TJS_W("Incompatible tTVPXP3ExtractionFilterInfo size"));
    if (KiriVNShouldBypassXP3Filter(info))
        return;
\tXP3FilterDecoder* decoder = FetchXP3Decoder();'''
    if filter_hook_old not in s:
        raise RuntimeError("upstream changed: XP3 extraction wrapper hook not found")
    s = s.replace(filter_hook_old, filter_hook_new, 1)

    pattern = re.compile(
        r'static void PostRegistCallback\(\)\s*\{.*?\n\}\n\nNCB_POST_REGIST_CALLBACK\(PostRegistCallback\);',
        re.S,
    )
    replacement = r'''static void PostRegistCallback()
{
    KiriVNResetXP3AdaptiveMode();
    ttstr path = TVPGetAppPath() + TJS_W("xp3filter.tjs");
    if (TVPIsExistentStorageNoSearch(path)) {
        iTJSTextReadStream * stream = TVPCreateTextStreamForRead(path, "");
        try
        {
            stream->Read(sXP3FilterScript, 0);
        }
        catch(...)
        {
            stream->Destruct();
            throw;
        }
        stream->Destruct();
    } else {
        ttstr builtinScript;
        if (KiriVNGetBuiltInXP3FilterScript(TVPGetAppPath(), builtinScript)) {
            sXP3FilterScript = builtinScript;
        }
    }

    if (!sXP3FilterScript.IsEmpty()) {
        TVPSetXP3ArchiveExtractionFilter(TVPXP3ArchiveExtractionFilterWrapper);
        TVPSetXP3ArchiveContentFilter(TVPXP3ArchiveContentFilterWrapper);
    }
}

NCB_POST_REGIST_CALLBACK(PostRegistCallback);'''
    s2, n = pattern.subn(replacement, s, count=1)
    if n != 1:
        raise RuntimeError("upstream changed: PostRegistCallback not found")
    p.write_text(s2, encoding="utf-8", newline="\n")



def patch_storage_app_path(upstream: pathlib.Path) -> None:
    p = upstream / "src/core/base/win32/StorageImpl.cpp"
    s = p.read_text(encoding="utf-8")
    old = '''ttstr TVPGetAppPath()
{
#if 0
\tstatic ttstr exepath(TVPExtractStoragePath(TVPNormalizeStorageName(ExePath())));
\treturn exepath;
#endif
\tstatic ttstr apppath(TVPExtractStoragePath(TVPProjectDir));
\treturn apppath;
}'''
    new = '''ttstr TVPGetAppPath()
{
#if 0
\tstatic ttstr exepath(TVPExtractStoragePath(TVPNormalizeStorageName(ExePath())));
\treturn exepath;
#endif
\t// KiriVN: the selected game changes at runtime. Do not cache the first
\t// TVPProjectDir value, otherwise compatibility profiles and xp3filter.tjs
\t// keep pointing at the launcher/previous directory.
\treturn TVPExtractStoragePath(TVPProjectDir);
}'''
    if old not in s:
        raise RuntimeError("upstream changed: TVPGetAppPath block not found")
    s = s.replace(old, new, 1)
    p.write_text(s, encoding="utf-8", newline="\n")

def patch_text_stream(upstream: pathlib.Path) -> None:
    p = upstream / "src/core/base/TextStream.cpp"
    s = p.read_text(encoding="utf-8")

    probe_old = '''\t\ttjs_uint8 mark[3] = {0,0};
\t\t\tStream->Read(mark, 3);'''
    probe_new = '''\t\ttjs_uint8 mark[3] = {0,0};
            // KiriVN: read a larger first probe so the adaptive XP3 layer can
            // reliably distinguish an original Cx-encrypted archive from an
            // already-plain translated/repacked archive.
            tjs_uint8 kirivn_probe[64] = {0};
            tjs_uint64 kirivn_remaining =
                Stream->GetSize() > ofs ? Stream->GetSize() - ofs : 0;
            if(kirivn_remaining >= 3)
            {
                tjs_uint kirivn_probe_size =
                    (tjs_uint)(kirivn_remaining > sizeof(kirivn_probe) ?
                        sizeof(kirivn_probe) : kirivn_remaining);
                Stream->Read(kirivn_probe, kirivn_probe_size);
                mark[0] = kirivn_probe[0];
                mark[1] = kirivn_probe[1];
                mark[2] = kirivn_probe[2];
                Stream->SetPosition(ofs + 3);
            }
            else
            {
                Stream->Read(mark, 3);
            }'''
    if probe_old not in s:
        raise RuntimeError("upstream changed: TextStream initial 3-byte probe not found")
    s = s.replace(probe_old, probe_new, 1)

    # KiriVN: decode each narrow text file independently. Russian translation
    # files are UTF-8 while original game files may still be Shift-JIS.
    decoder_pattern = re.compile(
        r'extern size_t TextStream_mbstowcs\(tjs_char \*pwcs, const tjs_nchar \*s, size_t n\) \{.*?\n\}\n\n(?=static ttstr enc_utf8)',
        re.S,
    )
    decoder_replacement = r'''extern size_t TextStream_mbstowcs(tjs_char *pwcs, const tjs_nchar *s, size_t n) {
    if (mbtowc_for_text_stream) {
        return _TextStream_mbstowcs(mbtowc_for_text_stream, pwcs, s, n);
    }

    // KiriVN: UTF-8 first (Russian translations), then legacy encodings.
    // Do not make auto-detection sticky across files because many old VNs
    // mix translated UTF-8 scripts with untouched Shift-JIS system scripts.
    size_t ret = _TextStream_mbstowcs(utf8_mbtowc, pwcs, s, n);
    if (ret != (size_t)-1) return ret;

    ret = _TextStream_mbstowcs(sjis_mbtowc, pwcs, s, n);
    if (ret != (size_t)-1) return ret;

    return _TextStream_mbstowcs(gbk_mbtowc, pwcs, s, n);
}

'''
    s, n = decoder_pattern.subn(decoder_replacement, s, count=1)
    if n != 1:
        raise RuntimeError("upstream changed: TextStream_mbstowcs block not found")

    decode_pattern = re.compile(
        r'BufferLen = TextStream_mbstowcs\(NULL, \(tjs_nchar\*\)nbuf, 0\);\n'
        r'\s*if \(BufferLen == \(size_t\)-1\) \{\n'
        r'\s*ttstr msg\(TVPGetMessageByLocale\("err_narrow_to_wide"\)\);\n'
        r'\s*TVPThrowExceptionMessage\(msg\.c_str\(\)\);\n'
        r'\s*\}\n'
        r'\s*Buffer = new tjs_char \[ BufferLen \+1\];\n'
        r'\s*TextStream_mbstowcs\(Buffer, \(tjs_nchar\*\)nbuf, BufferLen\);'
    )
    decode_replacement = r'''bool looksUtf16LE = false;
                        if(size >= 8 && (size & 1) == 0)
                        {
                            size_t pairs = size / 2;
                            if(pairs > 256) pairs = 256;
                            size_t plausible = 0;
                            size_t badControl = 0;
                            size_t lineBreaks = 0;
                            size_t zeroHigh = 0;

                            for(size_t i = 0; i < pairs; ++i)
                            {
                                tjs_uint16 wc = (tjs_uint16)nbuf[i * 2] |
                                    ((tjs_uint16)nbuf[i * 2 + 1] << 8);

                                if(nbuf[i * 2 + 1] == 0) ++zeroHigh;
                                if(wc == 10 || wc == 13) ++lineBreaks;

                                bool ok =
                                    wc == 9 || wc == 10 || wc == 13 ||
                                    (wc >= 0x20 && wc <= 0x7e) ||      // ASCII
                                    (wc >= 0x00a0 && wc <= 0x024f) || // Latin
                                    (wc >= 0x0400 && wc <= 0x052f) || // Cyrillic
                                    (wc >= 0x2000 && wc <= 0x206f) || // punctuation
                                    (wc >= 0x3000 && wc <= 0x30ff) || // JP punctuation/kana
                                    (wc >= 0x3400 && wc <= 0x9fff) || // CJK
                                    (wc >= 0xf900 && wc <= 0xfaff) || // CJK compat
                                    (wc >= 0xff00 && wc <= 0xffef);   // full-width

                                if(ok) ++plausible;

                                if((wc < 0x20 && wc != 9 && wc != 10 && wc != 13) ||
                                   (wc >= 0xd800 && wc <= 0xdfff) ||
                                   wc == 0xfffe || wc == 0xffff)
                                    ++badControl;
                            }

                            // UTF-16LE scripts from translated KiriKiri games often
                            // have no BOM and can be mostly Japanese/Cyrillic, so
                            // checking only for zero high bytes is insufficient.
                            // Require overwhelmingly text-like Unicode plus either
                            // real UTF-16 line breaks or a classic ASCII/UTF-16 pattern.
                            looksUtf16LE =
                                (plausible * 100 >= pairs * 88) &&
                                (badControl * 100 <= pairs * 2) &&
                                (lineBreaks > 0 || zeroHigh * 3 >= pairs);
                        }

                        if(looksUtf16LE)
                        {
                            BufferLen = size / 2;
                            Buffer = new tjs_char[BufferLen + 1];
                            for(size_t i = 0; i < BufferLen; ++i)
                            {
                                Buffer[i] = (tjs_char)(
                                    (tjs_uint16)nbuf[i * 2] |
                                    ((tjs_uint16)nbuf[i * 2 + 1] << 8));
                            }
                        }
                        else
                        {
                            BufferLen = TextStream_mbstowcs(NULL, (tjs_nchar*)nbuf, 0);
                            if (BufferLen == (size_t)-1) {
                                ttstr msg(TVPGetMessageByLocale("err_narrow_to_wide"));
                                msg += TJS_W(" File: ");
                                msg += name;
                                TVPThrowExceptionMessage(msg.c_str());
                            }
                            Buffer = new tjs_char [ BufferLen +1];
                            TextStream_mbstowcs(Buffer, (tjs_nchar*)nbuf, BufferLen);
                        }'''
    s, n = decode_pattern.subn(decode_replacement, s, count=1)
    if n != 1:
        raise RuntimeError("upstream changed: narrow decode block not found")

    p.write_text(s, encoding="utf-8", newline="\n")

def _xml_attr(value: str) -> str:
    return (value.replace("&", "&amp;")
                 .replace("<", "&lt;")
                 .replace(">", "&gt;")
                 .replace('"', "&quot;")
                 .replace("\n", "&#10;"))

def make_russian_locale(upstream: pathlib.Path) -> None:
    src = upstream / "project/ui/Resources/res/locale/en_us.xml"
    dst = upstream / "project/ui/Resources/res/locale/ru_ru.xml"
    text = src.read_text(encoding="utf-8")
    for key, value in RU.items():
        pattern = re.compile(r'(<Item\\s+id="' + re.escape(key) + r'"\\s+text=")[^"]*(")')
        text, _ = pattern.subn(lambda m: m.group(1) + _xml_attr(value) + m.group(2), text, count=1)
    dst.write_text(text, encoding="utf-8", newline="\n")

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("upstream", type=pathlib.Path)
    args = ap.parse_args()
    upstream = args.upstream.resolve()
    copy_overlay(upstream)
    patch_gradle_properties(upstream)
    patch_gradle(upstream)
    patch_manifest(upstream)
    patch_main_activity(upstream)
    patch_brand(upstream)
    patch_xp3filter(upstream)
    patch_storage_app_path(upstream)
    patch_text_stream(upstream)
    make_russian_locale(upstream)
    print("KiriVN overlay applied to", upstream)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
