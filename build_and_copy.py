Import("env")

import subprocess
import shutil
from os.path import join
import os
import sys

PACKAGE_DIR = "frontend"
IN_DIR = join("frontend", "dist")
OUT_DIR = "data"

def build_and_copy(source, target, env):
    p = subprocess.Popen(
        ["bash", "-i", "-c", "npm run build"] # Workaround for nvm
        if sys.platform == "linux" or sys.platform == "linux2"
        else ["npm", "run", "build"],
        cwd=PACKAGE_DIR)
    p.wait()

    shutil.rmtree(OUT_DIR)

    os.mkdir(OUT_DIR)
    f = open(join(OUT_DIR, ".gitkeep"), "w")
    f.close()

    shutil.copytree(IN_DIR, OUT_DIR, dirs_exist_ok=True)

env.AddPreAction("buildfs", build_and_copy)
env.AddPreAction("uploadfs", build_and_copy)
env.AddPreAction("uploadfsota", build_and_copy)