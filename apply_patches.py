Import("env")
import os
import shutil
import filecmp

#
# Apply the bundled lv_disp.c fix before building.
#
# lvgl v8.3.7 (pulled from the upstream zip in platformio.ini) ships without the
# screen-load-animation guard from lvgl PR #4487 plus the prev_scr/scr_to_load
# reset. Without it the KNOMI shows a black screen when screens switch quickly.
# The project ships the fixed file as "lv_disp_(bugfix_backup).c"; here we copy
# it over the installed lvgl source so every build is flash-ready with no manual
# step. Idempotent: only copies when the target differs.
#
PROJECT_DIR = env.subst("$PROJECT_DIR")
LIBDEPS_DIR = env.subst("$PROJECT_LIBDEPS_DIR")
PIOENV      = env.subst("$PIOENV")

src = os.path.join(PROJECT_DIR, "lv_disp_(bugfix_backup).c")
dst = os.path.join(LIBDEPS_DIR, PIOENV, "lvgl", "src", "core", "lv_disp.c")

if not os.path.isfile(src):
    print("[patch] skip: bundled fix not found at %s" % src)
elif not os.path.isfile(dst):
    # lvgl isn't a dependency of this env (e.g. the native test env) -> nothing to do.
    print("[patch] skip: lvgl not present for env '%s'" % PIOENV)
elif filecmp.cmp(src, dst, shallow=False):
    print("[patch] lvgl lv_disp.c already patched")
else:
    shutil.copyfile(src, dst)
    print("[patch] applied lv_disp.c black-screen fix -> %s" % dst)
