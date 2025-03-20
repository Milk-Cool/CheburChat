Import("env")

import subprocess
import shutil
from os.path import join
import sys

def build_and_copy(source, target, env):
    p = subprocess.Popen(
        ["bash", "-i", "-c", "npm run build"] # Workaround for nvm
        if sys.platform == "linux" or sys.platform == "linux2"
        else ["npm", "run", "build"],
        cwd="frontend")
    p.wait()

    shutil.copytree(join("frontend", "dist"), "data", dirs_exist_ok=True)

env.AddPreAction("upload", build_and_copy)