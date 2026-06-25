Import("env")
import os
import shutil
import filecmp

#
# Build-time patches applied to third-party libraries pulled by PlatformIO.
# Run automatically as a pre-build hook (see platformio.ini -> extra_scripts).
# Each patch is idempotent and tolerant: if the upstream file changes shape and
# a patch no longer matches, it warns and the build continues.
#
PROJECT_DIR = env.subst("$PROJECT_DIR")
LIBDEPS_DIR = env.subst("$PROJECT_LIBDEPS_DIR")
PIOENV      = env.subst("$PIOENV")


def patch_lvgl():
    #
    # lvgl v8.3.7 (upstream zip) ships without the screen-load-animation guard
    # from lvgl PR #4487 plus the prev_scr/scr_to_load reset. Without it the
    # KNOMI shows a black screen when screens switch quickly. We ship the fixed
    # file as "lv_disp_(bugfix_backup).c" and copy it over the installed source.
    #
    src = os.path.join(PROJECT_DIR, "lv_disp_(bugfix_backup).c")
    dst = os.path.join(LIBDEPS_DIR, PIOENV, "lvgl", "src", "core", "lv_disp.c")

    if not os.path.isfile(src):
        print("[patch] skip lvgl: bundled fix not found at %s" % src)
    elif not os.path.isfile(dst):
        print("[patch] skip lvgl: not present for env '%s'" % PIOENV)
    elif filecmp.cmp(src, dst, shallow=False):
        print("[patch] lvgl lv_disp.c already patched")
    else:
        shutil.copyfile(src, dst)
        print("[patch] applied lv_disp.c black-screen fix")


patch_lvgl()
# Note: kn0m3 ships its own OTA handler (src/ota.cpp) and no longer depends on
# the BTT AsyncElegantOTA fork, so its MD5 patch has been retired.
