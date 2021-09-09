#!/usr/bin/python

import os
import platform
import shutil
import subprocess
import sys


def call(cmdline):
    p = subprocess.Popen(cmdline.split(), stderr=subprocess.PIPE)
    (out, err) = p.communicate()
    if not p.returncode == 0:
        print("%s failed, returned %d" % (cmdline[0], p.returncode))
        exit(p.returncode)


def sdk_install_media(mediaDirName):
    if os.path.exists(mediaDirName):
        installcmd = "adb push " + mediaDirName + "/." + " /sdcard/"
        print("Executing: %s " % installcmd)
        call(installcmd)


if __name__ == "__main__":

    media_folder = "sdcard_SDK"
    if len(sys.argv) > 1 and sys.argv[1] != "":
        media_folder = sys.argv[1]

    sdk_install_media(media_folder)
