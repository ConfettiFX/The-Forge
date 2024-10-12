#!/usr/bin/python
# Copyright (c) 2017-2024 The Forge Interactive Inc.
#
# This file is part of The-Forge
# (see https://github.com/ConfettiFX/The-Forge).
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

import argparse
import os
import platform
import re
import subprocess
import sys

scriptDirPath = os.path.dirname(os.path.abspath(__file__))
theForgePath = os.path.dirname(os.path.dirname(scriptDirPath))
customMiddlewarePath = os.path.join(os.path.dirname(theForgePath), "Custom-Middleware")
gitmodulesPath = os.path.join(theForgePath, ".gitmodules")
filesToFormatFilePath = os.path.join(scriptDirPath, "clang-format-files.txt")
outFilesFilePath = os.path.join(scriptDirPath, "out-files.txt")

systemName = platform.system()
clangFormatExe = os.path.join(scriptDirPath, systemName, "clang-format") + (".exe" if systemName == "Windows" else "")
assert(os.path.isfile(clangFormatExe))
clangFormatArgs = "-i --style=file --fallback-style=none"

dirExcludeRegex = re.compile("\.(git|vs|cache|codelite)|^(Art|Data|Documents|Jenkins|Scripts|Tools|Libraries)|(ForgeShadingLanguage|Shaders|ThirdParty|Debug|Release|WebGpu)$")
fileExtensions = (".h", ".hpp", ".hxx", ".c", ".cpp", ".cxx", ".inc", ".m", ".mm", ".java")
filesToFormat = []

def walkLocalDir(path: str) -> None:
    for dirpath, dirnames, filenames in os.walk(path):
        newDirnames = []
        relDirpath = os.path.relpath(dirpath, path)

        for dirname in dirnames:
            subDirpath = dirname if relDirpath == "." else os.path.join(relDirpath, dirname)
            if dirExcludeRegex.search(subDirpath):
                print("Skipping formatting directory", subDirpath)
            else:
                newDirnames.append(dirname)
        dirnames[:] = newDirnames

        for filename in filenames:
            if filename.endswith(fileExtensions):
                filesToFormat.append(os.path.join(dirpath, filename))

def walkRepoDiff(path: str, remote: str, branch: str) -> None:
    os.chdir(path)
    command = "git fetch {}".format(remote)
    subprocess.run(command)
    command = "git diff --name-only --ignore-submodules --diff-filter=ACM {}/{}".format(remote, branch)
    #print("Running diff command:", command)
    fileList = subprocess.run(command, capture_output=True, text=True).stdout
    #print("Diff result is:\n", fileList)

    for filename in fileList.split("\n"):
        if not filename:
            continue

        filepath = os.path.join(path, filename)
        if not dirExcludeRegex.search(filepath) and filepath.endswith(fileExtensions):
            filesToFormat.append(filepath)

def walkSubmodulesDiffs(path: str, remote: str, branch: str) -> None:
    os.chdir(path)
    command = "git submodule status"
    status = subprocess.run(command, capture_output=True, text=True).stdout

    for line in status.split("\n"):
        if not line:
            continue

        submoduleName = line.lstrip().split(" ")[1] # NOTE: won't work if the submodule has a space in the name. Don't use spaces in submodule names
        submodulePath = os.path.join(path, submoduleName)

        if dirExcludeRegex.search(submoduleName):
            print("Skipping formatting submodule", submoduleName)
            continue

        os.chdir(path)
        command = "git ls-tree --object-only {}/{} {}".format(remote, branch, submoduleName)
        commitHash = subprocess.run(command, capture_output=True, text=True).stdout.rstrip()

        os.chdir(submodulePath)
        command = "git diff --name-only --ignore-submodules --diff-filter=ACM {}".format(commitHash)
        #print("Running diff command:", command)
        proc = subprocess.run(command, capture_output=True, text=True)

        if proc.returncode != 0:
            print("Error getting diff for submodule {}: {}".format(submoduleName, proc.stderr.rstrip()), file = sys.stderr)
            continue

        fileList = proc.stdout
        #print("Diff result is:\n", fileList)
        
        for filename in fileList.split("\n"):
            if not filename:
                continue
    
            filepath = os.path.join(submodulePath, filename)
            if not dirExcludeRegex.search(filepath) and filepath.endswith(fileExtensions):
                filesToFormat.append(filepath)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Format files using clang-format. Default behaviour is to format files in all non-ignored folders.")
    parser.add_argument("--diff", action="store_true", help='Format only the diff between current changes and REMOTE/BRANCH instead of all files.')
    parser.add_argument("--branch", type=str, default="master", help="Branch to use with diff command. Default is `master`.")
    parser.add_argument("--remote", type=str, default="origin", help="Remote to use with diff command. Default is `origin`.")
    #parser.add_argument("--tidy", action="store_true", help="Run clang-tidy after clang-format.")
    parser.add_argument("--dry-run", action="store_true", help="If set, do not actually make the formatting changes. Files that require formatting are written to `out-files.txt`")
    args = parser.parse_args()

    customMiddlewareFound = os.path.isdir(customMiddlewarePath)
    if not customMiddlewareFound:
        print("Custom-Middleware not found next to The-Forge repo", file = sys.stderr)

    print("Started formatting")

    if args.diff:
        oldCwd = os.getcwd()
        
        walkRepoDiff(theForgePath, args.remote, args.branch)
        walkSubmodulesDiffs(theForgePath, args.remote, args.branch)

        if customMiddlewareFound:
            customMiddlewareBranchName = "master"

            with open(gitmodulesPath, "r") as f:
                for line in f:
                    if line.startswith("#Custom-Middleware/Ephemeris"):
                        customMiddlewareBranchName = line.split(":")[1].strip()
                        break

            walkRepoDiff(customMiddlewarePath, args.remote, customMiddlewareBranchName)

        os.chdir(oldCwd)
    else:
        walkLocalDir(theForgePath)

        if customMiddlewareFound:
            walkLocalDir(customMiddlewarePath)

    if filesToFormat:
        print("Files to format written to", filesToFormatFilePath)
        with open(filesToFormatFilePath, "w") as f:
            f.write("\n".join(filesToFormat))
    else:
        print("Nothing to format")
        exit(0)

    if args.dry_run:
        clangFormatArgs += " --dry-run --Werror --ferror-limit=1"

    command = "{} {}".format(clangFormatExe, clangFormatArgs)
    proc = None
    exitCode = 0

    with open(filesToFormatFilePath, "r") as inFilesFile:
        if args.dry_run:
            with open(outFilesFilePath, "w") as outFilesFile:

                for fileToFormat in inFilesFile.readlines():
                    fileToFormat = fileToFormat.strip()
                    print(f"\tFormatting {fileToFormat}")

                    proc = subprocess.run(f'{command} "{fileToFormat}"', capture_output=True, text=True, shell=True)

                    if proc.returncode != 0:
                        exitCode = proc.returncode
                        
                        for line in proc.stderr.split("\n"):
                            print(f'\t\t{line.strip()}')
                            if not line:
                                continue

                            if line.endswith("[-Wclang-format-violations]"):
                                firstIndex = line.find(":")
                                secondIndex = line.find(":", firstIndex + 1)
                                filepath = line[:secondIndex]
                                relpath = os.path.relpath(filepath, theForgePath)
                                outFilesFile.write(f'\n{relpath}')
        else:
            for fileToFormat in inFilesFile.readlines():
                fileToFormat = fileToFormat.strip()
                print(f"\tFormatting {fileToFormat}")

                proc = subprocess.run(f'{command} "{fileToFormat}"', shell=True)
                if proc.returncode != 0:
                    exitCode = proc.returncode

    exit(exitCode)
