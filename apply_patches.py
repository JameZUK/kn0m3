Import("env")
import os
import re
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


def patch_elegant_ota():
    #
    # The BTT fork of AsyncElegantOTA requires an "MD5" multipart field and
    # returns HTTP 400 on any problem (missing/invalid MD5, begin/write/end
    # failure). The bundled web frontend only shows the server message for 500
    # responses -- for 400 it just prints "[HTTP ERROR] Bad Request", hiding the
    # real cause and making network flashing fail opaquely.
    #
    # Fix: make MD5 optional (revert to upstream ElegantOTA behaviour -- the
    # ESP32 image header is still validated on flash) and report genuine flash
    # failures as 500 with Update.errorString() so they're actually readable.
    #
    dst = os.path.join(LIBDEPS_DIR, PIOENV, "AsyncElegantOTA", "src", "AsyncElegantOTA.cpp")
    if not os.path.isfile(dst):
        print("[patch] skip AsyncElegantOTA: not present for env '%s'" % PIOENV)
        return

    text = open(dst, "r").read()
    if "kn0m3: MD5 verification disabled" in text:
        print("[patch] AsyncElegantOTA already patched")
        return

    original = text

    # 1) Drop the mandatory MD5 check (the hasParam-missing + setMD5-invalid block).
    md5_block = re.compile(
        r'if\(!request->hasParam\("MD5", true\)\)\s*\{.*?MD5 parameter missing"\);\s*\}\s*'
        r'if\(!Update\.setMD5\([^;]*\)\)\s*\{.*?MD5 parameter invalid"\);\s*\}',
        re.DOTALL)
    text = md5_block.sub(
        "// kn0m3: MD5 verification disabled for OTA compatibility (see apply_patches.py)",
        text, count=1)

    # 2) Make real flash failures visible (frontend shows responseText only for 500).
    text = text.replace(
        'request->send(400, "text/plain", "OTA could not begin")',
        'request->send(500, "text/plain", String("OTA flash failed: ") + Update.errorString())')
    text = text.replace(
        'request->send(400, "text/plain", "Could not end OTA")',
        'request->send(500, "text/plain", String("OTA verify failed: ") + Update.errorString())')

    if text == original or "kn0m3: MD5 verification disabled" not in text:
        print("[patch] WARNING: AsyncElegantOTA layout changed, OTA patch NOT applied")
        return

    open(dst, "w").write(text)
    print("[patch] applied AsyncElegantOTA MD5-optional / readable-error fix")


patch_lvgl()
patch_elegant_ota()
