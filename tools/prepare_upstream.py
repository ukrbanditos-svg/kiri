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

def patch_gradle(upstream: pathlib.Path) -> None:
    p = upstream / "project/android/app/build.gradle"
    s = p.read_text(encoding="utf-8")
    s = replace_once(s, 'applicationId "com.yuri.kirikiri2"', 'applicationId "io.kirivn.player"', "applicationId")
    s = s.replace('versionName "1.4.0beta"', 'versionName "0.1.0-alpha"')
    s = s.replace('outputFileName = "krkr2yuri_v${defaultConfig.versionName}.apk"',
                  'outputFileName = "KiriVN_v${defaultConfig.versionName}.apk"')
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
        s = replace_once(s, '#include "xp3filter.h"', '#include "xp3filter.h"\n#include "KiriVNCompat.h"', "KiriVN include")

    pattern = re.compile(
        r'static void PostRegistCallback\(\)\s*\{.*?\n\}\n\nNCB_POST_REGIST_CALLBACK\(PostRegistCallback\);',
        re.S,
    )
    replacement = r'''static void PostRegistCallback()
{
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
    dst.write_text(text, encoding="utf-8", newline="\\n")

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("upstream", type=pathlib.Path)
    args = ap.parse_args()
    upstream = args.upstream.resolve()
    copy_overlay(upstream)
    patch_gradle(upstream)
    patch_manifest(upstream)
    patch_main_activity(upstream)
    patch_brand(upstream)
    patch_xp3filter(upstream)
    make_russian_locale(upstream)
    print("KiriVN overlay applied to", upstream)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
