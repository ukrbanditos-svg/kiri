# KiriVN MVP

KiriVN is an Android KiriKiri/KAG player fork layer built on top of **Kirikiroid2Yuri**.
This repository stores only KiriVN-owned changes and checks out a pinned upstream commit during CI.

## MVP features

- Separate Android application id: `io.kirivn.player`
- App name: **KiriVN**
- Russian UI locale (`ru_ru.xml`)
- No `READ_PHONE_STATE` permission
- Android 11+ all-files access flow for sideload use
- Compatibility profile framework in native C++
- Built-in profile for **Rui wa Tomo o Yobu - Full Voice Edition**
- RuiTomo CxEncryption filter is fetched from the public Kirikiroid2 patch library,
  verified against Git blob SHA-1 `48060490e83d1524a8b0eea0cf1ded45c80c2a74`,
  then compiled into the APK
- A game-provided `xp3filter.tjs` has priority over KiriVN's built-in fallback

## RuiTomo detection

The `ruitomo_fve` profile is selected when one of these conditions matches:

1. `kirivn_ruitomo_fve.profile` exists in the game root, or
2. `ruitomo_fve.exe` exists, or
3. the unpacked game contains `startup.tjs`, characteristic RuiTomo scenario files, and voice data.

If automatic detection does not trigger, copy `profiles/kirivn_ruitomo_fve.profile`
into the game folder.

## Build APK

Open **Actions -> Build KiriVN Android -> Run workflow**.
After the workflow finishes, download artifact **KiriVN-Android**.

Pinned upstream: YuriSizuku/Kirikiroid2Yuri @
`6e61ce3b81416ceb2be3427a5f0471edefab7151`.

KiriKiri/Kirikiroid2 and third-party components retain their original licenses and notices.
