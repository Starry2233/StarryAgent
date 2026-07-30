# Android Package Rename Progress

- [x] Migrate Shizuku Java sources to `android/java/moe/starry2233/StarryAgent/shizuku/`
- [x] Migrate Shizuku AIDL source to `android/aidl/moe/starry2233/StarryAgent/shizuku/`
- [x] Update Java/AIDL package declarations to `moe.starry2233.StarryAgent.shizuku`
- [x] Update `ShizukuRunner.APP_ID` to `moe.starry2233.StarryAgent`
- [x] Update JNI class lookup path in `src/tools/AndroidShellBridge.cpp`
- [x] Update Android `after_build` sync paths in `xmake.lua`
- [x] Rewrite generated `gradle.properties` `androidPackageName` in `after_build`
- [x] Rewrite generated Android manifest package in `after_build`
- [x] Update local docs that referenced the old Android namespace
