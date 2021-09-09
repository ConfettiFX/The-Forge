#!/usr/bin/python

# This first bit of code is common bootstrapping code
# to determine the SDK root, and to set up the import
# path for additional python code.

# begin bootstrap
import os
import sys


def init():
    root = os.path.realpath(os.path.dirname(os.path.realpath(__file__)))
    print("root = " + root)
    os.chdir(root)  # make sure we are always executing from the project directory
    while os.path.isdir(os.path.join(root, "bin/scripts/build")) == False:
        root = os.path.realpath(os.path.join(root, ".."))
        if (
            len(root) <= 5
        ):  # Should catch both Posix and Windows root directories (e.g. '/' and 'C:\')
            print("Unable to find SDK root. Exiting.")
            sys.exit(1)
    root = os.path.abspath(root)
    os.environ["OCULUS_SDK_PATH"] = root
    sys.path.append(root + "/bin/scripts/build")


init()
import ovrbuild

ovrbuild.init()
# end bootstrap


ovrbuild.build()
