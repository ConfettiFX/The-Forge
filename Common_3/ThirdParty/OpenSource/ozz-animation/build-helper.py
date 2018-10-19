#!/usr/bin/python
#----------------------------------------------------------------------------#
#                                                                            #
# ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  #
# and distributed under the MIT License (MIT).                               #
#                                                                            #
# Copyright (c) 2017 Guillaume Blanc                                         #
#                                                                            #
# Permission is hereby granted, free of charge, to any person obtaining a    #
# copy of this software and associated documentation files (the "Software"), #
# to deal in the Software without restriction, including without limitation  #
# the rights to use, copy, modify, merge, publish, distribute, sublicense,   #
# and/or sell copies of the Software, and to permit persons to whom the      #
# Software is furnished to do so, subject to the following conditions:       #
#                                                                            #
# The above copyright notice and this permission notice shall be included in #
# all copies or substantial portions of the Software.                        #
#                                                                            #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    #
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
# DEALINGS IN THE SOFTWARE.                                                  #
#                                                                            #
#----------------------------------------------------------------------------#

# CMake python helper script.

import subprocess
import multiprocessing
import shutil
import sys
import os
import os.path
import re
import platform
from functools import partial


# Build global path variables.
root = os.path.abspath(os.path.join(os.getcwd(), '.'))
build_dir = os.path.join(root, 'build')
build_dir_cc = os.path.join(root, 'build-cc')
cmake_cache_file = os.path.join(build_dir, 'CMakeCache.txt')
ctest_cache_file = os.path.join(build_dir, 'CTestTestfile.cmake')
config = 'Release'
generators = {0: 'default'}
generator = generators[0]
enable_testing = False
emscripten_path = os.environ.get('EMSCRIPTEN')

def ValidateCMake():
  try:
    # Test that cmake can be executed, silently...
    pipe = subprocess.Popen(['cmake'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = pipe.communicate()
  except OSError as e:
    print("CMake is not installed or properly setup. Please visit www.cmake.org.")
    return False

  print("CMake is installed and setup properly.")
  return True

def CheckEmscripten():
  if(emscripten_path == None):
    return False

  try:
    # Test that cmake can be executed, silently...
    pipe = subprocess.Popen(['emcc'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = pipe.communicate()
  except OSError as e:
    print("Emscripten is not installed or properly setup.")
    return False

  print("Emscripten is installed and setup properly.")
  return True

def MakeBuildDir(_build_dir = build_dir):
  print("Creating out-of-source build directory: \"" + _build_dir + "\".")
  if not os.path.exists(_build_dir):
    os.makedirs(_build_dir)
  return True

def CleanBuildDir():
  print("Deleting out-of-source build directory: \"" + build_dir + "\".")
  if os.path.exists(build_dir):
    shutil.rmtree(build_dir)
  print("Deleting out-of-source cross compilation build directory: \"" + build_dir_cc + "\".")
  if os.path.exists(build_dir_cc):
    shutil.rmtree(build_dir_cc)
  return True

def Configure():
  # Configure build process.
  print("Configuring build project.")
  options = ['cmake']
  options += ['-D', 'CMAKE_BUILD_TYPE=' + config]

  if (enable_testing) :
    options += ['-D', 'ozz_build_tests=1']
  else:
    options += ['-D', 'ozz_build_tests=0']

  global generator
  if(generator != 'default'):
    options += ['-G', generator]
  options += [root]
  config_process = subprocess.Popen(options, cwd=build_dir)
  config_process.wait()
  if(config_process.returncode != 0):
    print("Configuration failed.")
    return False
  print("Configuration succeeded.")

  # Updates generator once configuration is complete
  generator = DetectGenerator()

  return True

def ConfigureCC():
  # Configure build process.
  print("Configuring emscripten cross compilation build with path: " + emscripten_path)
  options = ['cmake']

  options += ['-D', 'CMAKE_BUILD_TYPE=' + config]
  options += ['-D', 'CMAKE_TOOLCHAIN_FILE=' + emscripten_path + '/cmake/Modules/Platform/Emscripten.cmake']

  if(platform.system() == 'Windows'):
    options += ['-G', 'MinGW Makefiles']
  else:
    options += ['-G', 'Unix Makefiles']

  options += [root]
  config_process = subprocess.Popen(options, cwd=build_dir_cc)
  config_process.wait()
  if(config_process.returncode != 0):
    print("Configuration failed.")
    return False
  print("Configuration succeeded.")

  # Updates generator once configuration is complete
  generator = DetectGenerator()

  return True

def Build(_build_dir = build_dir):
  # Configure build process.
  print("Building project.")
  options = ['cmake', '--build', _build_dir, '--config', config, '--use-stderr'];
  # Appends parallel build option if supported by the generator.
  if "Unix Makefiles" in generator:
    options += ['--', '-j' + str(multiprocessing.cpu_count())]
  config_process = subprocess.Popen(options, cwd=_build_dir)
  config_process.wait()
  if(config_process.returncode != 0):
    print("Build failed.")
    return False
  print("Build succeeded.")
  return True

def Test():
  # Configure Test process, parallelize a lot of tests in order to stress their dependencies
  print("Running unit tests.")
  options = ['ctest' ,'--output-on-failure', '-j' + str(multiprocessing.cpu_count() * 4), '--build-config', config]
  config_process = subprocess.Popen(options, cwd=build_dir)
  config_process.wait()
  if(config_process.returncode != 0):
    print("Testing failed.")
    return False
  print("Testing succeeded.")
  return True

def PackSources(_type):
  print("Packing sources.")
  options = ['cpack', '-G', _type, '--config', 'CPackSourceConfig.cmake']
  config_process = subprocess.Popen(options, cwd=build_dir)
  config_process.wait()
  if(config_process.returncode != 0):
    print("Packing sources of type " + _type + " failed.")
    return False
  print("Packing sources of type " + _type + " succeeded.")
  return True

def PackBinaries(_type, _build_dir = build_dir):
  print("Packing binaries.")
  options = ['cpack', '-G', _type, '-C', config]
  config_process = subprocess.Popen(options, cwd=_build_dir)
  config_process.wait()
  if(config_process.returncode != 0):
    print("Packing binaries of type " + _type + " failed.")
    return False
  print("Packing binaries of type " + _type + " succeeded.")
  return True

def SelecConfig():
  configs = {
    1: 'Debug',
    2: 'Release',
    3: 'RelWithDebInfo',
    4: 'MinSizeRel'}

  while True:
    print("Select build configuration:")
    for num, message in sorted(configs.iteritems()):
      print("%d: %s") % (num, message)

    # Get input and check validity
    try:
      answer = int(raw_input("Enter a value: "))
    except:
      continue
    if not answer in configs:
      continue

    # Affect global configuration variable
    global config
    config = configs[answer]
    return True

def FindGenerators():
  # Finds all generators outputted from cmake usage 
  process = subprocess.Popen(['cmake', '--help'], stdout=subprocess.PIPE)
  stdout = process.communicate()[0]
  sub_stdout = stdout[stdout.rfind('Generators'):]
  matches = re.findall(r"\s*(.+)\s*=.+", sub_stdout, re.MULTILINE)
  # Fills generators list
  global generators  
  for match in matches:
    generator_name = match.strip()
    # Appends also Win64/ARM option if generator is VS
    if " [arch]" in generator_name:
      gen_name = generator_name[0:len(generator_name) - 7]
      generators[len(generators)] = gen_name
      generators[len(generators)] = gen_name + " Win64"
      generators[len(generators)] = gen_name + " ARM"
    else:
      generators[len(generators)] = generator_name

def FindInCache(_regex):
  try:
    cache_file = open(cmake_cache_file)
  except:
    return None
  return re.search(_regex, cache_file.read())

def DetectGenerator():
  match = FindInCache(r"CMAKE_GENERATOR:INTERNAL=(.*)")
  if match:
    global generators
    global generator
    for num, message in sorted(generators.iteritems()):
      if match.group(1) == message:
        return message
  return 'default'

def SelecGenerator():
  global generators
  while True:
    print("Select generator:")
    for num, message in sorted(generators.iteritems()):
      print("%d: %s") % (num, message)

    # Get input and check validity
    try:
      answer = int(raw_input("Enter a value: "))
    except:
      continue
    if not answer in generators:
      continue
    
    # Check if this is the current generator
    current_generator = DetectGenerator()
    if current_generator == 'default':
      global generator
      generator = generators[answer]
      return True
    if current_generator != generators[answer]:
      print("Selected generator '%s' is different from the current one '%s'.")  % (generators[answer], current_generator)
      clean = raw_input("Do you want to clean build directory to apply the change? (y/n): ") == "y"
      if clean:
        generator = generators[answer]
        return CleanBuildDir()
    return True

def DetectTesting():
  global enable_testing
  enable_testing = os.path.isfile(ctest_cache_file)

def EnableTesting():
  global enable_testing
  while True:
    # Get input and check validity
    answer = raw_input("enable testing (y/n): ")
    if answer != 'y' and answer != 'n':
      continue
    wanted = (answer == 'y')
    
    # Get current state
    if (enable_testing != wanted):
      enable_testing = wanted
      print("Testing state has changed.")
      
    return True

def ClearScreen():
  os.system('cls' if os.name=='nt' else 'clear')

def Quit():
  sys.exit(0)
  return True

def main():

  # Checks CMake installation is correct.
  if not ValidateCMake():
    return

  # Emscripten is optional
  CheckEmscripten()

  # Detects available generators
  FindGenerators()

  # Detects testing state
  DetectTesting()

  # Update current generator
  print("DetectGenerator")
  global generator
  generator = DetectGenerator()

  options = {
    '1': ["Build", [MakeBuildDir, Configure, Build]],
    '2': ["Run unit tests", [MakeBuildDir, Configure, Build, Test]],
    '3': ["Execute CMake generation step (don't build)", [MakeBuildDir, Configure]],
    '4': ["Clean out-of-source build directory\n  ------------------", [CleanBuildDir]],
    '5': ["Pack binaries", [MakeBuildDir, Configure, Build, partial(PackBinaries, "ZIP"), partial(PackBinaries, "TBZ2")]],
    '6': ["Pack sources\n  ------------------", [MakeBuildDir, Configure, partial(PackSources, "ZIP"), partial(PackSources, "TBZ2")]],
    '7': ["Enable testing", [EnableTesting]],
    '8': ["Select build configuration", [SelecConfig]],
    '9': ["Select cmake generator\n  ------------------", [SelecGenerator]],
    'q': ["Quit\n------------------", [Quit]]}

  # Adds emscripten
  global emscripten_path
  if emscripten_path != None:
    options['1a'] = ["Build emscripten", [partial(MakeBuildDir, build_dir_cc), ConfigureCC, partial(Build, build_dir_cc)]]
    options['5a'] = ["Pack emscripten binaries", [MakeBuildDir, Configure, Build, partial(MakeBuildDir, build_dir_cc), ConfigureCC, partial(Build, build_dir_cc), partial(PackBinaries, "ZIP", build_dir_cc)]]

  while True:
    # Displays options
    ClearScreen()
    print("ozz CMake build helper tool")
    print("")
    print("Selected build configuration: %s") % config
    print("Selected generator: %s") % generator
    print("Testing enabled: %s") % enable_testing
    print("")
    print("Choose an option:")
    print("------------------")
    for key, message in sorted(options.iteritems()):
      print("  %s: %s") % (key, message[0])

    # Get input and check validity
    answer = raw_input("Enter a value: ")
    if not answer in options:
      continue

    # Execute command in a try catch to avoid crashes and allow retries.
    ClearScreen()
    try:
      for command in options[answer][1]:
        if command():
          print("\nExecution success.\n")
        else:
          print("\nExecution failed.\n")
          break
    except Exception as e:
      print("\nAn error occured during script execution: %s\n") % e

    raw_input("Press enter to continue...")

  return 0

if __name__ == '__main__':
  main()
