#!/usr/bin/python
# Copyright (c) 2017-2022 The Forge Interactive Inc.
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

import os
import os.path
from pathlib import Path
import shutil  #Used for deleting files within subdirectories
import fnmatch #Checks for matching expression in name
import time    #used for timing process in case one hangs without crashing
import platform #Used for determining running OS
import subprocess #Used for spawning processes
import sys        #system module
import argparse #Used for argument parsing
import traceback
import signal #used for handling ctrl+c keyboard interrupt
import xml.etree.ElementTree as ET  #used for parsing XML ubuntu project file
import shutil #used for deleting directories
from distutils.dir_util import copy_tree #used for copying directories
import re # Used for retrieving console IP
import json

successfulBuilds = [] #holds all successfull builds
failedBuilds = [] #holds all failed builds

successfulTests = [] #holds all successfull tests
failedTests = [] #holds all failed tests

maxIdleTime = 120  #2 minutes of max idle time with cpu usage null
benchmarkFiles = {}

sanitizer_UTs = {
	"06_MaterialPlayground",
	"09_LightShadowPlayground",
	"36_AlgorithmsAndContainers",
	"Visibility_Buffer",
	"Aura",
	"Ephemeris"
}

config_file_args = [
	('memtracking', 'ENABLE_MEMORY_TRACKING'),
	('logging', 'ENABLE_LOGGING'),
	('forge-scripting', 'ENABLE_FORGE_SCRIPTING'),
	('forge-ui', 'ENABLE_FORGE_UI'),
	('forge-fonts', 'ENABLE_FORGE_FONTS'),
	('zip-filesystem', 'ENABLE_ZIP_FILESYSTEM'),
	('screenshot', 'ENABLE_SCREENSHOT'),
	('profiler', 'ENABLE_PROFILER'),
]

def AddConfigFileArgs(parser):
	global config_file_args
	for argname, macroname in config_file_args:
		parser.add_argument(f'--{argname}', dest=macroname, default=None, action="store_true", help=f'Enables "{macroname}" pre processor define.')
		parser.add_argument(f'--no-{argname}', dest=macroname, action="store_false", help=f'Disables "{macroname}" pre processor define.')

config_file_options = []

def SaveConfigFileArgs(args):
	for _, macroname in config_file_args:
		arg_value = getattr(args, macroname)
		if arg_value != None:
			config_file_options.append((macroname, arg_value))

def FormatTime(timeSeconds):
	return time.strftime("%H:%M:%S", time.gmtime(timeSeconds))

def PrintResults():
	if len(successfulBuilds) > 0:
		print ("Successful Builds list:")
		for build in successfulBuilds:
			print(build['name'], build['conf'], build['platform'], 'in', FormatTime(build['time']))
	
	print ("")
	if len(failedBuilds) > 0:
		print ("Failed Builds list:")
		for build in failedBuilds:
			print(build['name'], build['conf'], build['platform'], 'in', FormatTime(build['time']))
			
	if len(successfulTests) > 0:
		print ("Successful tests list:")
		for test in successfulTests:
			if test['gpu'] == "":
				print(test['name'], 'in', FormatTime(test['time']))
			else:
				print(test['name'], test['gpu'], 'in', FormatTime(test['time']))
	
	print ("")
	if len(failedTests) > 0:
		print ("Failed Tests list:")
		for test in failedTests:
			timeStr = ""
			if test['gpu'] == "":
				print(test['name'], test['reason'], 'in', FormatTime(test['time']))
			else:
				print(test['name'], test['gpu'], test['reason'], 'in', FormatTime(test['time']))

def FindMSBuild17():
	ls_output = ""
	msbuildPath = ""
	try:
		#proc = subprocess.Popen(["C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe", "-latest", "-products", "*", "-requires" ,"Microsoft.Component.MSBuild", "-find", "MSBuild\**\Bin\MSBuild.exe"], stdout=subprocess.PIPE,stderr = subprocess.STDOUT, encoding='utf8')

		#open vswhere and parse the output
		proc = subprocess.Popen(["C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe", "-version", "[15.0,16.0)", "-requires" ,"Microsoft.Component.MSBuild", "-property", "installationPath"], stdout=subprocess.PIPE,stderr = subprocess.STDOUT, encoding='utf8')

		ls_output = proc.communicate()[0]
		# In case there is more than 1 Visual studio (community, professional, ...) installed on the machine use the first one
		ls_output = ls_output.split('\n')[0]

		#check if vswhere opened correctly
		if proc.returncode != 0:
			print("Could not find vswhere")
		elif ls_output != "":
			msbuildPath = ls_output.strip() + "/MSBuild/15.0/Bin/MSBuild.exe"

	except Exception as ex:
		#ERROR
		print(ex)
		print(ls_output)
		print("Could not find vswhere")
	
	return msbuildPath

#define string is a list of #defines separated by \n. 
#Example: #define ACTIVE_TESTING_GPU 1\n#define AUTOMATED_TESTING 1\n 
def AddPreprocessorToFile(filePath, defineString, stringToReplace):
	if not os.path.exists(filePath):
		return
	with open(filePath,'r+') as f:
			lines = f.readlines()
			for i, line in enumerate(lines):
				if line.startswith(stringToReplace) :
					if "#pragma" in stringToReplace:
						lines[i]=line.replace(line,line +defineString)
					else:
						lines[i]=line.replace(line,defineString + "\n" + line)
					break

			f.seek(0)
			for line in lines:
				f.write(line)

def RemovePreprocessorFromFile(filePath, definesList):
	if not os.path.exists(filePath):
		return
	with open(filePath,'r+') as f:
		lines = f.readlines()
		f.seek(0)
		for line in lines:
			writeLine = True

			for define in definesList:
				if define in line:
					writeLine = False
					break
			
			if writeLine == True:		
					f.write(line)
		
			f.truncate()

# options is a list of (string, bool) pairs
# Example: [('ENABLE_MEMORY_TRACKING', True), ('ENABLE_LOGGING', False)]
def AddConfigOptions(configPath, options):
	if len(options) == 0:
		return
	with open(configPath, 'a') as f:
		for name, enabled in options:
			if enabled:
				f.write(f'\n#ifndef {name}\n#define {name}\n#endif\n')
			else:
				f.write(f'\n#ifdef {name}\n#undef {name}\n#endif\n')

# options is a list of (string, bool) pairs
# Example: [('ENABLE_MEMORY_TRACKING', True), ('ENABLE_LOGGING', False)]
def RemoveConfigOptions(configPath, options):
	# exit if options is empty
	if not options:
		return
	with open(configPath, 'r+') as f:
		lines = f.readlines()
		# Remove options from the end of the file, so that file can act like a stack
		for i in reversed(range(len(lines))):
			line = ' ' + lines[i]
			remove_opt = None
			for j,(opt, _) in enumerate(options):
				# Check for the exact match of the option
				# For example without newline and space 
				# option ENABLE_AB would be triggered by option ENABLE_A
				opt = f" {opt}\n"
				
				if opt in line:
					# Format:
					#[i-2]:.*\n
					#[i-1]:#ifn?def {name}\n
					#[i  ]:#(define|undef) {name}\n
					#[i+1]:#endif\n
					lines[i-2] = lines[i-2][:-1]
					lines[i-1] = ''
					lines[i] = ''
					lines[i+1] = ''
					remove_opt = j
					break
			
			if remove_opt != None:
				options.pop(remove_opt)
				if not options:
					break
		
		f.seek(0)
		for line in lines:
			f.write(line)
		f.truncate()


def AddTestingPreProcessor(enabledGpuSelection):	
	if setDefines == True:
		print("Adding Automated testing preprocessor defines")
		macro = "#define AUTOMATED_TESTING 1"
		if enabledGpuSelection:
			macro += "\n#define ACTIVE_TESTING_GPU 1"
		AddPreprocessorToFile("Common_3/Application/Config.h", macro + "\n", "#pragma")

	if config_file_options:
		print('Adding config options preprocessor defines')
		AddConfigOptions("Common_3/Application/Config.h", config_file_options)
	

def RemoveTestingPreProcessor():
	testingDefines = ["#define AUTOMATED_TESTING", "#define ACTIVE_TESTING_GPU"]
	memTrackingDefines = ["#define USE_MEMORY_TRACKING"]
	if setDefines == True:
		print("Removing automated testing preprocessor defines")
		RemovePreprocessorFromFile("Common_3/Application/Config.h", testingDefines)

	if config_file_options:
		print('Removing config options preprocessor defines')
		RemoveConfigOptions("Common_3/Application/Config.h", config_file_options)
	
def ExecuteTimedCommand(cmdList, printStdout: bool):
	try:		
		if isinstance(cmdList, list): 
			print("Executing Timed command: " + ' '.join(cmdList))
		else:
			print("Executing Timed command: " + cmdList)		
		
		captureOutput = (printStdout == False)

		#10 minutes timeout
		proc = subprocess.run(cmdList, capture_output=captureOutput, timeout=maxIdleTime)

		return (proc.returncode, proc.stderr, proc.stdout)
	except subprocess.TimeoutExpired as timeout:
		print(timeout)
		print("App hanged and was forcibly closed.")
		return (-1, "Command killed due to Timeout".encode(encoding='utf8'), "".encode(encoding='utf8')) # error return code
	except Exception as ex:
		print("-------------------------------------")
		if isinstance(cmdList, list): 
			print("Failed Executing Timed command: " + ' '.join(cmdList))
		else:
			print("Failed Executing Timed command: " + cmdList)	
		print(ex)
		print("-------------------------------------")
		return (-1, f"{ex}".encode(encoding='utf8'), "".encode(encoding='utf8')) # error return code

def ExecuteCommandWOutput(cmdList, printException = True):
	try:
		print("")
		print("Executing command: " + ' '.join(cmdList))
		print("") 
		ls_lines = subprocess.check_output(cmdList).splitlines()
		return ls_lines
	except Exception as ex:
		if printException == True:
			print("-------------------------------------")
			print("Failed executing command: " + ' '.join(cmdList))
			print(ex)
			print("-------------------------------------")
		return ""  #return error
	
	return ""

def XBoxCommand(cmdList, verbose=True):
	if verbose:
		print("Executing command: " + ' '.join(cmdList))		
	try:
		ls_lines = subprocess.check_output(cmdList).splitlines()
		return [ line.decode('utf-8') for line in ls_lines]
	except Exception as ex:
		print("-------------------------------------")
		print("Failed Executing command: " + ' '.join(cmdList))
		print(ex)
		print("-------------------------------------")
		return [""] 

def ExecuteCommand(cmdList,outStream):
	try:
		print("")
		if isinstance(cmdList, list): 
			print("Executing command: " + ' '.join(cmdList))
		else:
			print("Executing command: " + cmdList)		
		print("") 
		proc = subprocess.Popen(cmdList, stdout=outStream)
		proc.wait()

		return (proc.returncode, proc.stderr, proc.stdout)
	except Exception as ex:
		print("-------------------------------------")
		if isinstance(cmdList, list): 
			print("Failed Executing command: " + ' '.join(cmdList))
		else:
			print("Failed Executing command: " + cmdList)		
		print(ex)
		print("-------------------------------------")
		return (-1, f"{ex}".encode(encoding='utf8'), "".encode(encoding='utf8')) # error return code

def SilentExecuteCommand(cmdList):
	try:
		print("")
		if isinstance(cmdList, list): 
			print("Executing command with no output: " + ' '.join(cmdList))
		else:
			print("Executing command with no output: " + cmdList)		
		print("") 
		proc = subprocess.Popen(cmdList, stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
		proc.wait()

		return (proc.returncode, proc.stderr, proc.stdout)
	except Exception as ex:
		print("-------------------------------------")
		if isinstance(cmdList, list): 
			print("Failed Executing command: " + ' '.join(cmdList))
		else:
			print("Failed Executing command: " + cmdList)		
		print(ex)
		print("-------------------------------------")
		return (-1, f"{ex}".encode(encoding='utf8'), "".encode(encoding='utf8')) # error return code
	
def ExecuteCommandErrorOnly(cmdList):
	try:
		print("")
		print("Executing command: " + ' '.join(cmdList))
		print("") 
		DEVNULL = open(os.devnull, 'w')
		proc = subprocess.Popen(cmdList, stdout=DEVNULL, stderr=subprocess.STDOUT)
		proc.wait()

		if proc.returncode != 0:
			return proc.returncode
	except Exception as ex:
		print("-------------------------------------")
		print("Failed executing command: " + ' '.join(cmdList))
		print(ex)
		print("-------------------------------------")
		return -1  #error return code
	
	return 0 #success error code

def CheckTest(test, fileName, gpuLine, check_ub_error, is_iOS, elapsedTime):
	returnCode = test[0]
	stdStreams = (test[1], test[2])

	# Sanitizer errors on iOS crash the program but don't output the error.
	# This checks that a runtime error is due to a sanitzer error and manually
	# outputs the sanitizer error type and call stack.
	if is_iOS and returnCode != 0 and stdStreams[1] != None:
		out = stdStreams[1].decode('utf-8')
		sanitizer_type = re.search("(AddressSanitizer|UndefinedBehavior)", out)
		if sanitizer_type: # error due to sanitizer
			stop_reason = re.search("stop reason = .*", out)
			call_stack = re.findall("frame #\d+: 0x\w+ .*", out)
			print(sanitizer_type.group(0) + " Error")
			print(stop_reason.group(0))
			count = 0
			for stack_frame in call_stack:
				if "frame #" + str(count) in stack_frame:
					print("\t" + stack_frame)
					count += 1
	# UB errors on macOS and Switch only output a runtime error message, but test still passes.
	# This forces it to fail.
	elif returnCode == 0 and check_ub_error:
		sanitizer_error_pattern = re.compile("(\s.* runtime error: .*\s)")
		for stdStream in stdStreams:
			if stdStream == None:
				continue
			match = sanitizer_error_pattern.search(stdStream.decode('utf-8'))
			if match:
				err = match.group(1)
				print("===============")
				print("SANITIZER ERROR")
				print(err.strip())
				print("===============")
				returnCode = -1
				break

	if returnCode != 0:
		print("FAILED TESTING ", fileName)
		print("Return code: ", returnCode)
		failedTests.append({'name':fileName, 'gpu':gpuLine, 'reason':'Runtime Failure', 'time':elapsedTime})
	else:
		successfulTests.append({'name':fileName, 'gpu':gpuLine, 'time':elapsedTime})

	return returnCode

def ExecuteBuild(cmdList, fileName, configuration, platform):
	startTime = time.time()
	returnCode = ExecuteCommand(cmdList, sys.stdout)[0]
	elapsedTime = time.time() - startTime
	
	if returnCode != 0:
		print("FAILED BUILDING ", fileName, configuration)
		failedBuilds.append({'name':fileName,'conf':configuration, 'platform':platform, 'time':elapsedTime})
	else:
		successfulBuilds.append({'name':fileName,'conf':configuration, 'platform':platform, 'time':elapsedTime})
	
	return returnCode

def ExecuteTest(cmdList, fileName, regularCommand, gpuLine = "", printStdout: bool = False, check_ub_error = False, is_iOS = False):
	returnCode = 0

	startTime = time.time()

	if regularCommand:
		cmdResult = ExecuteCommand(cmdList, None)
		elapsedTime = time.time() - startTime
		returnCode = CheckTest(cmdResult, fileName, gpuLine, check_ub_error, is_iOS, elapsedTime)
	else:
		cmdResult = ExecuteTimedCommand(cmdList, printStdout)
		elapsedTime = time.time() - startTime
		returnCode = CheckTest(cmdResult, fileName, gpuLine, check_ub_error, is_iOS, elapsedTime)

	if returnCode == 0:
		print("Success")

	return returnCode

def GetBundleIDFromIOSApp(filename):
	try:
		#need to parse xml configuration to get every project
		import json
		filename = filename + "/Info.plist"
		if not os.path.exists(filename):
			return ""

		fileContents = ExecuteCommandWOutput(["plutil", "-convert", "json", "-o", "-", "--",filename])
		fileContents = (b"".join(fileContents)).decode('utf-8')
		
		if fileContents == "":
			return fileContents
		plistJson = json.loads(fileContents)
		return plistJson["CFBundleIdentifier"]
	except Exception as ex:
		print("Failed retrieving plist file")
		print(ex)
		return ""

#Get list of folders in given root with the given name
#xan specific depth to look only under a limited amount of child dirs
#default depth value is -1 --> no limit on depth
def FindFolderPathByName(rootToSearch, name, depth = -1):
	folderPathList = []
	finalPath = rootToSearch
	# traverse root directory, and list directories as dirs and files as files
	for root, dirs, files in os.walk(finalPath):
		for dirName in fnmatch.filter(dirs, name):
			folderPathList.append(os.path.join(os.path.join(root,dirName)) + os.path.sep)
		if depth == 0:
			break
		depth = depth - 1
	return folderPathList

def GetFilesPathByExtension(rootToSearch, extension, wantDirectory, maxDepth=-1):
	filesPathList = []
	finalPath = rootToSearch
	# traverse root directory, and list directories as dirs and files as files
	for root, dirs, files in os.walk(finalPath):
		if wantDirectory:
			#Need to test that
			#for dirName in fnmatch.filter(dirs, "*."+extension):
			#	filesPathList.append(os.path.join(root,dirName)

			#in mac os the xcodeproj are not files but packages so they act as directories
			path = root.split(os.sep)
			#separating the root to get extentions will give us ['path_here'/01_Transformations, xcodeproj]
			pathLast =  path[-1]
			if pathLast:
				pathLastExt = pathLast.split(os.extsep)[-1]
				if pathLastExt == extension:
					filesPathList.append(root)
		else:
			for filename in fnmatch.filter(files, "*." + extension):
				filesPathList.append(os.path.join(root, filename))

		if maxDepth == 0:
			break
		maxDepth = maxDepth - 1

	return filesPathList

def GetMemLeakFile(exeFilePath):
	print (exeFilePath)
	exeFileWithoutExt = exeFilePath.split('.')[0]
	memLeakFile = exeFileWithoutExt + ".memleaks"
	return memLeakFile

def GetLogFile(exeFilePath):
	exeFileWithoutExt = exeFilePath.split('.')[0]
	logFile = exeFileWithoutExt + ".log"
	return logFile

def tryPrintLog(logFilePath):
	try:
		if os.path.exists(logFilePath):
			with open(logFilePath) as f:
				print(f.read())
	except TypeError:
		print("Failed to print log file")

"""
projRootFolder should be one of those:
	-Unit_Tests
	-Aura
	-VisibilityBuffer
This function will mark the first available gpu config as used (this should be called after a run)
It returns false if there are no gpu's left to test, true otherwise
If No GPu's are left then it will recover the file
"""

activeGpusConfiguration = """#
#<vendor_id>, <model_id>,<preset>,<device_name>, <revision_id>
0x10de; 0x1b80; Ultra; Nvidia Geforce GTX 1080; 0xa1
0x10de; 0x1402; Medium; Nvidia Geforce GTX 950; 0xa1
0x1002; 0x67df; High; Radeon (TM) RX 480 Graphics; 0xc7
0x1002; 0x687f; High; Radeon RX Vega; 0xc3
0x1002; 0x699f; Medium; Radeon RX550/550 Series; 0xc7
0x8086; 0x5912; Low; Intel(R) HD Graphics 630; 0x4
"""
originalActiveGpuConfigLines = []
def selectActiveGpuConfig(forgeDir, projRootFolder, projectName, runIndex):
	global activeGpusConfiguration
	global originalActiveGpuConfigLines
	#remove file extension from project name
	#print(projectName)
	projectName = os.path.splitext(projectName)[0]
	
	#need to have
	if "Unit_Tests" in projRootFolder:
		filename = "/Examples_3/"+projRootFolder+"/src/"+projectName+"/GPUCfg/activeTestingGpu.cfg"
	elif "Ephemeris" in projectName:
		filename = "/../Custom-Middleware/Ephemeris/src/EphemerisExample/GPUCfg/activeTestingGpu.cfg"
	else:
		filename = "/Examples_3/"+projRootFolder+"/src/GPUCfg/activeTestingGpu.cfg"

	filename = filename.replace("/",os.path.sep)

	filename = forgeDir + filename
	
	#create active gpu config if it doesn't exist
	#this is only valid for our internal testing rig
	#this way we can commit activeTestGpu files
	if not os.path.exists(filename):
		#check if GPUCfg folder exists, if not create it.
		dirName = os.path.dirname(filename)
		if not os.path.exists(dirName):
			os.makedirs(dirName)
		f = open(filename, "w+")
		f.write(activeGpusConfiguration)
		
	removedMatch = False
	foundMatch = False
	lineMatch = ""
	with open(filename,'r+') as f:
		lines = f.readlines()
		if runIndex == 0:
			originalActiveGpuConfigLines = []
			for i,line in enumerate(lines):
				originalActiveGpuConfigLines.append(line)
				if not line.startswith('#'):
					return {'running':True, 'lineMatch': line}
		
		for i, line in enumerate(lines):
			if not line.strip():
				continue
			if not line.startswith('#') and not removedMatch:
				lines[i]=line.replace(line,"# " +line)
				removedMatch = True
				continue
			if removedMatch and not line.startswith('#'):
				print("Found line", line)
				lineMatch = line
				foundMatch = True
				break		
		
		if foundMatch:
			f.seek(0)
			for line in lines:
				f.write(line)
		else:
			f.seek(0)
			f.truncate()
			for line in originalActiveGpuConfigLines:
				f.write(line)
				
	#if we are done then we can remove the file
	if not foundMatch and os.path.exists(filename):
		try:
			os.remove(filename)
		except OSError as e:  ## if failed, report it back to the user ##
			print(("Error: %s - %s." % (e.filename, e.strerror)))



	return {'running':foundMatch, 'lineMatch': lineMatch}

def TestXcodeProjects(iosTesting, macOSTesting, iosDeviceId, benchmarkFrames, sanitizer_pass):
	errorOccured = False

	projects = GetFilesPathByExtension("./Examples_3/","app", True)
	iosApps = []
	osxApps = []
	appsToTest = []
	
	config = "Debug" if sanitizer_pass else "Release"

	for proj in projects:
		if config in proj:
			#skip asset  pipeline tool during sanitizer
			if sanitizer_pass and "AssetPipeline" in proj:
				continue
			#we don't want to build Xbox one solutions when building PC
			if "_iOS" in proj:
				iosApps.append(proj)
			else :
				osxApps.append(proj)

	if iosTesting:
		appsToTest = iosApps
		
	if macOSTesting:
		appsToTest.extend(osxApps)

	# Delete old crash report files
	crashReportRoot = os.path.join(str(Path.home()), 'Library/Logs/DiagnosticReports')
	for reportFilename in os.listdir(crashReportRoot):
		fullPath = os.path.join(crashReportRoot, reportFilename)
		try:
			if os.path.isfile(fullPath) or os.path.islink(fullPath):
				os.unlink(fullPath)
			elif os.path.isdir(fullPath):
				shutil.rmtree(fullPath)
		except Exception as e:
			print(f"Failed to delete {fullPath} due to {e.message}")

	for app in appsToTest:
		leaksDetected = False
		#get working directory (excluding the xcodeproj in path)
		rootPath = os.sep.join(app.split(os.sep)[0:-1])
		filename = app.split(os.sep)[-1].split(os.extsep)[0]
		
		#save current work dir
		currDir = os.getcwd()
		#change dir to xcodeproj location
		os.chdir(rootPath)
		command = []
		retCode = -1
		is_iOS = "_iOS" in filename
		# get the memory leak file path
		memleakFile = GetMemLeakFile(filename.split("_iOS")[0])
		logFile = GetLogFile(filename.split("_iOS")[0])

		if is_iOS:
			has_device_id = not iosDeviceId == "-1" and not iosDeviceId == ""

			#if specific ios id was passed then run for that device
			#otherwise run on first device available
			#print iosDeviceId
			command = ["ios-deploy","--uninstall","-b",filename + ".app","-I", "-v"]
			if has_device_id:
				command.append("--id")
				command.append(iosDeviceId)
			
			#run with benchmarking mode if specified
			if benchmarkFrames > 0:
				command.append("-a")
				command.append("-b")
				command.append(str(benchmarkFrames))

			# force Metal validation layer for iOS
			command.append("-s METAL_DEVICE_WRAPPER_TYPE=1")

			retCode = ExecuteTest(command, filename, False,printStdout=True, is_iOS=True)

			bundleID = GetBundleIDFromIOSApp(filename + ".app")

			if bundleID != "":
				if os.path.exists("./Library"):
					shutil.rmtree("./Library")

				# Downloads log and prints it
				logDownloadCommand = ["ios-deploy","--bundle_id",bundleID,"--download=/Library/Application Support","--to","./"]
				if has_device_id:
					logDownloadCommand.append("--id")
					logDownloadCommand.append(iosDeviceId)
					
				logDownloaded = ExecuteCommand(logDownloadCommand, sys.stdout)[0]
				if logDownloaded == 0:
					iosLogFile = "Library/Application Support/" + logFile
					tryPrintLog(iosLogFile)
				else:
					print("[Error] Log file could not be downloaded for:" + bundleID)

				if retCode == 0:
					leaksDetected = FindMemoryLeaks("Library/Application Support/"+ memleakFile)										
					if benchmarkFrames > 0:
						benchmarkData = RecordBenchmarkFilePath("IOS", filename.split(".")[0].split("_iOS")[0], "Library/Application Support/")
						print(benchmarkData)
				
				#uninstall bundle if bundle id was found
				command = ["ios-deploy","--uninstall_only","--bundle_id",bundleID]
				ExecuteCommand(command, sys.stdout)
			else:
				print("[Error] Bundle ID NOT found:" + bundleID)
				
		else:
			command = ["./" + filename + ".app/Contents/MacOS/" + filename]
			#run with benchmarking mode if specified
			if benchmarkFrames > 0:
				command.append("-b")
				command.append(str(benchmarkFrames))
			
			retCode = ExecuteTest(command, filename, False, check_ub_error=True)

			tryPrintLog(logFile)
			if retCode != 0:
				loadedLog = False
				# It takes a bit of time to generate a crash report. 
				# Try every second, for 20 times. Worst case, we wait 20 seconds.
				for _ in range(20):
					time.sleep(1)
					for reportFilename in os.listdir(crashReportRoot):
						fullPath = os.path.join(crashReportRoot, reportFilename)
						try:
							if os.path.isfile(fullPath) and filename in reportFilename and reportFilename.endswith(".crash"):
								with open(fullPath) as f:
									log = f.read()
									begin = log.find('Crashed:') - 9
									end = log.find('\n\n', begin)
									log = log[begin:end]
									print("***\nCrash Report\n***")
									print(log)
									loadedLog = True
								break
						except Exception as e:
							print(f"Failed to print {fullPath} due to {e}")
					if loadedLog:
						break
				if not loadedLog:
					print("Failed to load crash report file")
			elif benchmarkFrames > 0:
				benchmarkData = RecordBenchmarkFilePath("macOS", filename.split(".")[0], ".")
				print(benchmarkData)

			leaksDetected = FindMemoryLeaks(memleakFile)
		
		if retCode == 0 and leaksDetected == True:
			lastSuccess = successfulTests.pop()
			failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Memory Leaks", 'time':lastSuccess['time']})
			errorOccured = True
			
		if retCode != 0:
			errorOccured = True

		#set working dir to initial
		os.chdir(currDir)

	if errorOccured:
		return -1
	else:
		return 0

#Helper to create xcodebuild command for given scheme, workspace(full path from current working directory) and configuration(Debug, Release)
#can filter out schemes based on what to skip and will return "" in those cases
def CreateXcodeBuildCommand(skipMacos, skipIos, skipIosCodeSigning,path,scheme,configuration, isWorkspace, ddPath, printBuildOutput, ubsan, asan, tsan):
	logLevel = "-quiet"
	if printBuildOutput:
		logLevel = "-hideShellScriptEnvironment"

	if isWorkspace:
		command = ["xcodebuild",logLevel,"-workspace",path,"-configuration",configuration,"build","-parallelizeTargets","-scheme",scheme]
	else:
		command = ["xcodebuild",logLevel,"-project",path,"-configuration",configuration,"build","-parallelizeTargets","-scheme",scheme]

	#use the -derivedDataPath flag only when custom location is specified by ddPath otherwise use the default location
	if ddPath != 'Null':
		command.append("-derivedDataPath")
		command.append(ddPath)

	if ubsan:
		command.append("-enableUndefinedBehaviorSanitizer")
		command.append("YES")

	if asan:
		command.append("-enableAddressSanitizer")
		command.append("YES")
	elif tsan:
		command.append("-enableThreadSanitizer")
		command.append("YES")

	if skipIosCodeSigning:
		command.extend([
		"CODE_SIGN_IDENTITY=\"\"",
		"CODE_SIGNING_REQUIRED=\"NO\"",
		"CODE_SIGN_ENTITLEMENTS=\"\"",
		"CODE_SIGNING_ALLOWED=\"NO\""])
	return command
		
def ListDirs(path):
	return [dir for dir in os.listdir(path) if os.path.isdir(os.path.join(path,dir))]


def BuildXcodeProjects(args):
	sanitizer_pass = (args.ubsan or args.asan or args.tsan) and not args.assetpipeline
	errorOccured = False
	buildConfigurations = ["Debug", "Release"]

	if args.skipdebugbuild:
		buildConfigurations.remove("Debug")
	if args.skipreleasebuild:
		buildConfigurations.remove("Release")

	#since our projects for macos are all under a macos Xcode folder we can search for
	#that specific folder name to gather source folders containing project/workspace for xcode
	#macSourceFolders = FindFolderPathByName("Examples_3/","macOS Xcode", -1)
	xcodeProjects = ["/Examples_3/Ephemeris/macOS Xcode/Ephemeris.xcodeproj", 
					"/Examples_3/Aura/macOS Xcode/Aura.xcodeproj",
					"/Examples_3/Visibility_Buffer/macOS Xcode/Visibility_Buffer.xcodeproj",
					"/Examples_3/Unit_Tests/macOS Xcode/Unit_Tests.xcworkspace"]

	if not sanitizer_pass:
		xcodeProjects.append("/Common_3/Tools/AssetPipeline/Apple/AssetPipelineCmd.xcodeproj")
	#if --assetpipeline fully ignore
	if args.assetpipeline and not sanitizer_pass:
		xcodeProjects = ["/Common_3/Tools/AssetPipeline/Apple/AssetPipelineCmd.xcodeproj"]


	#if derivedDataPath is not specified then use the default location
	if args.xcodederiveddatapath == 'Null':
		DDpath = args.xcodederiveddatapath
	else:
		#Custom Derived Data location relative to root of project
		DDpath = os.path.join(os.getcwd(), args.xcodederiveddatapath)
		if os.path.exists(DDpath):
			#delete the contents of subdirectories at the location
			shutil.rmtree(DDpath)
		#Create a custom directory
		os.mkdir(DDpath)

	for proj in xcodeProjects:
		#get working directory (excluding the xcodeproj in path)
		rootPath = os.getcwd() + os.sep.join(proj.split(os.sep)[0:-1])
		#save current work dir
		currDir = os.getcwd()
		#change dir to xcworkspace location
		os.chdir(rootPath)
		#create command for xcodebuild
		filenameWExt = proj.split(os.sep)[-1]
		filename = filenameWExt.split(os.extsep)[0]
		extension = filenameWExt.split(os.extsep)[1]

		isWorkspace = "xcworkspace" in extension

		if isWorkspace:
			if sanitizer_pass:
				if args.skipiosbuild:
					schemesList = ["SanitizerPass_macOS"]
				else:
					schemesList = ["SanitizerPass"]
			else:
				if args.skipiosbuild:
					schemesList = ["BuildAll_macOS"]
				elif args.skipmacosbuild:
					schemesList = ["BuildAll_iOS"]
				else:
					schemesList = ["BuildAll"]
		else:
			# if we're passing a project instead of a workspace
			# enumate available targets and use them as the scheme destination
			# use targets instead of schemes directly as a scheme might not be active / used by valid target.
			output = ExecuteCommandWOutput(f"xcodebuild -list -project {filenameWExt} -json".split(' '), False)
			output = (b"".join(output)).decode('utf-8')
			try:
				jsonOutput = json.loads(output)
				schemesList = jsonOutput["project"]['targets']
			except Exception as ex: 
				print(f"Error loading targets from {filenameWExt} with exception {ex}")
				schemesList = [filename]

		for conf in buildConfigurations:
			#will build all targets for vien project
			#canot remove ios / macos for now
			
			for scheme in schemesList:
				command = CreateXcodeBuildCommand(args.skipmacosbuild,args.skipiosbuild,args.skipioscodesigning,filenameWExt,scheme,conf,isWorkspace,DDpath,args.printbuildoutput,args.ubsan,args.asan,args.tsan)

				if scheme == "BuildAll" or scheme == "SanitizerPass":
					platformName = "macOS/iOS"
				elif "iOS" in scheme:
					platformName = "iOS"
				else:
					platformName = "macOS"

				sucess = ExecuteBuild(command, filename, conf, platformName)

				if sucess != 0:
					errorOccured = True
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0

#this needs the vulkan environment variables set up correctly
#if they are not in ~/.profile then they need to be set up for every subprocess
#If it is in ~/.profile then it needs to be maintaned by updating the version number in ~/.profile.
def BuildLinuxProjects(args):
	errorOccured = False
	
	projsToBuild = GetFilesPathByExtension("./Examples_3/","workspace", False)
	# Add Asset pipeline project
	if args.assetpipeline:
		#if argument is passed then we do it exlusively
		projsToBuild = GetFilesPathByExtension("./Common_3/Tools/AssetPipeline/","workspace", False)
	else:
		#otherwise just append to existing list of projects to build
		projsToBuild.extend(GetFilesPathByExtension("./Common_3/Tools/AssetPipeline/","workspace", False))

	for projectPath in projsToBuild:
		#get working directory (excluding the workspace in path)
		rootPath = os.sep.join(projectPath.split(os.sep)[0:-1])
		#save current work dir
		currDir = os.getcwd()
		buildSettings = currDir + "/Examples_3/Unit_Tests/UbuntuCodelite/build_settings.xml"
		#change dir to workspace location
		os.chdir(rootPath)
		configurations = ["Debug", "Release"]
		listOfDependenciesToSkip = [ "OSBase", "OS", "EASTL", "Renderer","SpirVTools","PaniniProjection",
									 "gainput","LuaManager","AssetPipeline", "astc-encoder", "ozz_base",
									 "ozz_animation","ozz_animation_offline" ]

		for conf in configurations:					
			#create command for xcodebuild
			#filename = projectPath.split(os.sep)[-1].split(os.extsep)[0]
			filename = projectPath.split(os.sep)[-1]
			
			#need to parse xml configuration to get every project
			xmlTree = ET.parse("./"+filename)
			xmlRoot = xmlTree.getroot()

			ubuntuProjects = []
			for child in xmlRoot:
				if child.tag == "Project":
					if child.attrib["Name"] not in listOfDependenciesToSkip: 
						ubuntuProjects.append(child.attrib["Name"])
			
			for proj in ubuntuProjects:
				command = ["codelite-make","-w",filename,"-p", proj,"-c",conf, "--settings="+buildSettings]
				#sucess = ExecuteBuild(command, filename+"/"+proj,conf, "Ubuntu")
				sucess = SilentExecuteCommand(command)[0]
				
				if sucess != 0:
					errorOccured = True
				
				command = ["/usr/bin/make","-j8", "-e","-f","Makefile"]
				sucess = ExecuteBuild(command, filename+"/"+proj,conf, "Ubuntu")
				
				if sucess != 0:
					errorOccured = True

		#set working dir to initial
		os.chdir(currDir)
	
	if errorOccured == True:
		return -1
	return 0

def MakeDir(path, dirName):
	try:
		os.makedirs(path, exist_ok = True)
		print(f"Directory {dirName} created successfully")
	except OSError as error:
		print(f"Directory {dirName} can not be created")
		print(error)

def TestAssetPipeline():

	errorOccured = False

	#manjaro/macos by default, no extension
	filename = "AssetPipelineCmd"	

	#this should not be hardcoded but the purpose of the test now its okay
	#also we should have non compressed formats as in. this will not work yet.
	#this could be passed down as args
	textureArgs = ["--in-dds", "--out-dds"]
	platformPath = "Win64/x64"
	#Build for Mac OS (Darwin system)
	if platform.system() == "Darwin":
		textureArgs = ["--in-dds", "--out-ktx"]
		platformPath = "Apple/Bin"
	elif platform.system().lower() == "linux" or platform.system().lower() == "linux2":
		platformPath = "Linux/Bin"
	else: #Windows
		filename = f"{filename}.exe"

	config = "Release"

	#absolute path to pipeline cmd
	#takes into account if we run from different directory
	pipelineCmd = os.path.join(os.getcwd(), os.path.normpath(f"Common_3/Tools/AssetPipeline/{platformPath}/{config}/{filename}"))

	#get parent dir path
	rootPath = os.sep.join(pipelineCmd.split(os.sep)[0:-1])
	exeLoc = "./" + filename

	#relative paths
	examplesRelativeResourcePath = "../../../../../../Examples_3/Unit_Tests/UnitTestResources"
	examplesRelativeProcessedPath = os.path.join(examplesRelativeResourcePath, "ProcessedFiles")
	
	#absolute paths
	examplesAbsoluteResourcePath = os.path.join(os.getcwd(), os.path.normpath("Examples_3/Unit_Tests/UnitTestResources"))
	examplesAbsoluteProcessedPath = os.path.join(examplesAbsoluteResourcePath, "ProcessedFiles")

	#create needed dirs if they don't exist.
	MakeDir(examplesAbsoluteProcessedPath, "ProcessedFiles")
	MakeDir(os.path.join(f"{examplesAbsoluteProcessedPath}/Animation"), "ProcessedFiles/Animation")
	MakeDir(os.path.join(f"{examplesAbsoluteProcessedPath}/Animation/stormtrooper"), "ProcessedFiles/Animation/stormtrooper")
	MakeDir(os.path.join(f"{examplesAbsoluteProcessedPath}/Animation/stormtrooper/animations"), "ProcessedFiles/Animation/stormtrooper/animations")
	MakeDir(os.path.join(f"{examplesAbsoluteProcessedPath}/Textures"), "ProcessedFiles/Textures")
	MakeDir(os.path.join(f"{examplesAbsoluteProcessedPath}/Meshes")  , "ProcessedFiles/Meshes")
	MakeDir(os.path.join(f"{examplesAbsoluteResourcePath}/ZipFiles") , "ZipFiles")

	# cook commands
	processAnimations = [exeLoc, "-pa", "--input", os.path.join(examplesRelativeResourcePath, os.path.normpath("Animation/")), "--output", os.path.join(examplesRelativeProcessedPath,"Animation"), "--force"]
	processTextures = [exeLoc, "-pt", "--input", os.path.join(examplesRelativeResourcePath,"Textures"), "--output", os.path.join(examplesRelativeProcessedPath,"Textures"), "--force"] + textureArgs
	processGltf = [exeLoc, "-pgltf", "--input", os.path.join(examplesRelativeResourcePath,"Meshes"), "--output", os.path.join(examplesRelativeProcessedPath,"Meshes"), "--force"]
	processWriteZipAll = [exeLoc, "-pwza", "--input", examplesRelativeProcessedPath, "--output", os.path.join(examplesRelativeResourcePath,"ZipFiles"), "--name", "28-ZipFileSystem.zip", "--force"]

	# Temporal untill we can process all textures at once
	tempProcessTexture = [ exeLoc, "-pt", "--input-file", os.path.join(examplesRelativeResourcePath, "Textures", "TheForge.png"), "--output", os.path.join(examplesRelativeProcessedPath, "Textures"), "--force"]
	tempProcessTextureDDS = tempProcessTexture + [ "--out-dds" ]
	tempProcessTextureKTX = tempProcessTexture + [ "--out-ktx" ]

	# Temporal untill we process all textures in the asset pipeline
	skyboxTextures = [
		"Skybox_right1",
		"Skybox_left2",
		"Skybox_top3",
		"Skybox_bottom4",
		"Skybox_front5",
		"Skybox_back6",
	]

	MakeDir(os.path.join(examplesAbsoluteProcessedPath, "Textures", "Skybox"), "Skybox")
	skyboxTexturesDestPath = os.path.join(examplesAbsoluteProcessedPath, "Textures", "Skybox")
	for skyboxTexture in skyboxTextures:
		texturePath = os.path.join(examplesAbsoluteResourcePath, "Textures", skyboxTexture)
		try:
			print(f"Copying Skybox texture: {texturePath} to {skyboxTexturesDestPath}")
			dstFilename = os.path.join(skyboxTexturesDestPath, skyboxTexture)
			shutil.copyfile(texturePath + ".dds", dstFilename + ".dds")
			shutil.copyfile(texturePath + ".ktx", dstFilename + ".ktx")
		except Exception as ex:
			print(f"Error Copying {texturePath} to {skyboxTexturesDestPath}")
			print(ex)
			errorOccured = True

	allCommands = [
		{"Command":processAnimations,"Label":"Process Animations"},
		#{"Command":processTextures,"Label":"Process Textures"},
		{"Command":processGltf,"Label":"Process GLTF"},
        {"Command":tempProcessTextureDDS, "Label":"Test Process Texture DDS"},
        {"Command":tempProcessTextureKTX, "Label":"Test Process Texture KTX"},

		# Zip is the last to cover all previously cooked assets
		{"Command":processWriteZipAll,"Label":"Write Zip"},
	]

	#relative to working directory below
	memleakFile = f"AssetPipeline.memleaks"
	logFile = f"AssetPipeline.log"
	
	for command in allCommands:
		leaksDetected = False
		
		#save current work dir
		currDir = os.getcwd()
		#change dir to xcodeproj location
		os.chdir(rootPath)
		retCode = -1
		
		#don't time test for now
		retCode = ExecuteTest(command["Command"], f"{filename} {command['Label']}", False, "", False)

		tryPrintLog(logFile)
		memleaksFilename = os.path.join(os.getcwd(),memleakFile)
		leaksDetected = FindMemoryLeaks(memleaksFilename)
		print("")
		
		if retCode == 0 and leaksDetected == True:
			lastSuccess = successfulTests.pop()
			failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Memory Leaks", 'time':lastSuccess['time']})
			errorOccured = True
			
		if retCode != 0:
			errorOccured = True

		#set working dir to initial
		os.chdir(currDir)

	# Copy stormtrooper animation to the Animations folder so that we test it when running UT28_Skinning
	try:
		src = os.path.join(examplesAbsoluteProcessedPath, "Animation", "stormtrooper")
		dst = os.path.join(examplesAbsoluteResourcePath, "Animation", "stormtrooper")
		print("Copying cooked stormtrooper animation files: ", src, " -> ", dst)
		shutil.copytree(src, dst, dirs_exist_ok = True)
	except Exception as ex:
		print(f"Couldn't copy stormtrooper converted files to test UT28_Skinning")
		print(ex)
		failedTests.append({'name':'CopyProcessedAnimationFiles', 'gpu':'', 'reason':"shutil.copytree failed", 'time':0})
		errorOccured = True
	
	if errorOccured == True:
		return -1
	return 0

#this needs the vulkan environment variables set up correctly
#if they are not in ~/.profile then they need to be set up for every subprocess
#If it is in ~/.profile then it needs to be maintaned by updating the version number in ~/.profile.
def TestLinuxProjects(benchmarkFrames):
	errorOccured = False
	
	projsToTest = GetFilesPathByExtension("./Examples_3/","workspace", False)
	for projectPath in projsToTest:
		#get working directory (excluding the workspace in path)
		rootPath = os.sep.join(projectPath.split(os.sep)[0:-1])
		#save current work dir
		currDir = os.getcwd()
		#change dir to workspace location
		os.chdir(rootPath)
		configurations = ["Release"]
		for conf in configurations:
			filename = projectPath.split(os.sep)[-1].split(os.extsep)[0]
			
			#need to parse xml configuration to get every project
			xmlTree = ET.parse("./"+filename + ".workspace")
			xmlRoot = xmlTree.getroot()

			projsToIgnore = [ "OSBase", "EASTL", "OS", "Renderer", "SpirVTools", "PaniniProjection", "gainput", "ozz_base", "ozz_animation", "ozz_animation_offline", "Assimp", "zlib", "LuaManager", "AssetPipeline", "AssetPipelineCmd", "MeshOptimizer", "19a_CodeHotReload_Game" ]

			ubuntuProjects = []
			ubuntuProjectDirs = []
			for child in xmlRoot:
				if child.tag == "Project" and child.attrib["Name"] not in projsToIgnore:
					ubuntuProjects.append(child.attrib["Name"])
					# get the directory where the project is stored, remove 'projectName.project' from the end
					ubuntuProjectDirs.append(child.attrib["Path"][:-len(child.attrib["Name"] + ".project")])
			
			for projIndex, proj in enumerate(ubuntuProjects):
				leaksDetected = False	
				exePath = os.path.join(os.getcwd(),ubuntuProjectDirs[projIndex],conf,proj)
				command = ["gdb", "-q", "-ex", "r", "-ex", "bt", "-ex", "print $_exitcode", "-batch", "-return-child-result", "--args", exePath] 
				
				#run with benchmarking mode if specified
				if benchmarkFrames > 0:
					command.append("-b")
					command.append(str(benchmarkFrames))
				
				retCode = ExecuteTest(command, proj, False, "", True)

				if retCode != 0:
					errorOccured = True
				
				if not errorOccured and benchmarkFrames > 0:
					benchmarkData = RecordBenchmarkFilePath("Manjaro", proj, os.path.join(os.getcwd(),proj,conf))
					print(benchmarkData)

				memleaksFilename = os.path.join(os.getcwd(),proj,conf,GetMemLeakFile(proj))
				leaksDetected = FindMemoryLeaks(memleaksFilename)
				
				if retCode == 0 and leaksDetected == True:
					lastSuccess = successfulTests.pop()
					failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Memory Leaks", 'time':lastSuccess['time']})
					errorOccured = True

		#set working dir to initial
		os.chdir(currDir)
	
	if errorOccured == True:
		return -1
	return 0

#renames log file to use GPU name, that way multiple runs generate different files
def AppendToLogFilename(logName, stringToAppend):
	logNameWithoutExt = logName.split('.')[0]
	#avoid path errors due to special chars
	stringToAppend = stringToAppend.replace(" ", "_").replace("/","-").replace("\\","-")
	newLogName = logNameWithoutExt + stringToAppend + ".log"
	if os.path.exists(logName):
		try:
			if os.path.exists(newLogName):
				print("Removing existing {0}".format(newLogName))
				os.remove(newLogName)
			os.rename(logName, newLogName)
		except Exception as ex: 
			print("Error Renaming {0} to {1}".format(logName, newLogName))
			print(ex)

def ReadWindowsLogAndLeaks(logFilename, memleakFile):
	logFile = GetLogFile(logFilename)
	if os.path.exists(logFile):
		with open(logFile) as f:
			for line in f:
				print(line, end="")
	
	return FindMemoryLeaks(memleakFile)

def TestWindowsProjects(useActiveGpuConfig, benchmarkFrames):
	errorOccured = False
	
	isWindows7 = int(platform.release()) < 10
	projects = GetFilesPathByExtension("./Examples_3","exe",False)
	fileList = []

	for proj in projects:
		#we don't want to build Xbox one solutions when building PC
		#we don't want to run ImageConvertTools when building PC
		#we don't want to run AssetPipelineCmd when building PC
		if "PC Visual Studio 2017" in proj and "Release" in proj and not "ImageConvertTools" in proj and not "AssetPipelineCmd" in proj :
			fileList.append(proj)

	for proj in fileList:
		leaksDetected = False
		#get current path for sln file
		#strip the . from ./ in the path
		#replace / by the os separator in case we need // or \\
		rootPath = os.getcwd() + proj.strip('.')
		rootPath = rootPath.replace("/",os.sep)
		#need to get root folder of path by stripping the filename from path
		rootPath = rootPath.split(os.sep)[0:-1]
		rootPath = os.sep.join(rootPath)

		#save root directory where python is executed from
		currDir = os.getcwd()
		#change working directory to sln file
		os.chdir(rootPath)

		filename = proj.split(os.sep)[-1]
		origFilename = filename
		command = [filename]

		#run with benchmarking mode if specified
		if benchmarkFrames > 0:
			command.append("-b")
			command.append(str(benchmarkFrames))

		if "ReleaseVk" in proj:
			filename = "VK_" + filename
		elif "Dx11" in proj:
			filename = "Dx11_" + filename
		elif "Dx12" in proj:
			filename = "Dx12_" + filename

		parentFolder = proj.split(os.sep)[1]
		
		# get the memory leak file path
		memleakFile = GetMemLeakFile(origFilename)
		# delete the memory leak file before execute the app
		if os.path.exists(memleakFile):
			os.remove(memleakFile)
		
		if useActiveGpuConfig == True and "hlslparser" not in proj:
			currentGpuRun = 0
			resultGpu = selectActiveGpuConfig(currDir, parentFolder,origFilename,currentGpuRun)
			while resultGpu['running'] == True:
				retCode = ExecuteTest(command, filename, False, resultGpu['lineMatch'])
				#rename log file to have gpu info in name
				leaksDetected = ReadWindowsLogAndLeaks(GetLogFile(origFilename), memleakFile)
				currentGpuRun += 1
				AppendToLogFilename(origFilename.split(".")[0] + ".log", resultGpu['lineMatch'].split(";")[3])
				resultGpu = selectActiveGpuConfig(currDir, parentFolder,origFilename,currentGpuRun)
		else:
			retCode = ExecuteTest(command, filename,False)
			leaksDetected = ReadWindowsLogAndLeaks(GetLogFile(origFilename), memleakFile)

		if retCode == 0 and leaksDetected == True:
			lastSuccess = successfulTests.pop()
			failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Memory Leaks", 'time':lastSuccess['time']})
			errorOccured = True

		if retCode != 0:
			errorOccured = True
		elif benchmarkFrames > 0:
			benchmarkData = RecordBenchmarkFilePath("Windows", origFilename.split(".")[0], ".")
			print(benchmarkData)
				
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0	

def TestXboxProjects(benchmarkFrames):
	errorOccured = False
	FNULL = open(os.devnull, 'w')
	
	#Get console IP
	consoleIP = ""
	gdkDir = os.environ['GameDK'] + 'bin/'
	command = [gdkDir+'xbconnect', '/QG']
	output = subprocess.Popen(command, stdin=None, stdout=subprocess.PIPE, stderr=FNULL)
	output = output.communicate()[0]
	connection = re.search(b'Connections at (.+?), ', output)
	if connection:
		consoleIP = connection.group(1)
		consoleIP = consoleIP.decode('utf-8')
	
	for output_line in output.split(b'\n'):
		if b"TITLE: Unreachable." in output_line:
			print ("Unable to connect to: "+consoleIP)
			return 1

	#Test for DashBoard
	pslist = subprocess.check_output([gdkDir+'xbtlist']).decode()
	isDashboard = 'Xbox.Dashboard' in pslist
	if not isDashboard:
		print("WARNING: as of \'May 2020 GXDK\' using DevHome in CI is known to be unstable (this can crash the devkit).\n"
		"Please set (in GDK Manager or web UI) \'Settings/Preference/Default Home Experience\' to \'Retail Home\'.")

	#get correct dashboard package name
	homePackageName = "Xbox.Dashboard_8wekyb3d8bbwe!Xbox.Dashboard.Application" if isDashboard == True else "Microsoft.Xbox.DevHome_8wekyb3d8bbwe!App"
	applist = subprocess.check_output([gdkDir+'xbapp', "list", "/includesystem"]).decode().splitlines()
	for line in applist:
		if isDashboard == True and "Dashboard" in line and "Application" in line:
			homePackageName = line.strip()
			print(line) 
			break
		elif isDashboard == False and "DevHome" in line and "Application" in line:
			homePackageName = line.strip()
			print(line) 
			break
	
	#Cleanup the shared folders before launching the test
	crashdump_path = '\\\\'+consoleIP+"\SystemScratch\LocalDumps"
	shaderFolder = 	'\\\\'+consoleIP+"\SystemScratch\CompiledShaders"
	cleanupList = ["CompiledShaders","PipelineCaches","SDF", "LocalDumps"]

	def cleanupSharedFiles():
		for dir in cleanupList:
			fullDir = '\\\\'+consoleIP+"\SystemScratch\\"+dir
			if os.path.exists(fullDir):
				shutil.rmtree(fullDir)
			os.makedirs(fullDir)

	#Clean all apps
	print ("Cleaning XBox apps and data (this will reboot the console)")
	command = [gdkDir+'xbcleanup','/U','/D','/P','/S','/C','/L']
	output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)
	print ("Done cleaning...")
	
	cleanupSharedFiles()

	command = [gdkDir+'xbconfig','EnableKernelDebugging=true',"/X"+consoleIP]
	output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)

	#Set console setting to genereate crash dumps
	command = [gdkDir+'xbconfig','CrashDumpType=mini',"/X"+consoleIP]
	output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)
	crashDumpCount = 0
	try:
		#Get count of existing crash dumps
		command = ["cmd", "/c", "dir", crashdump_path]
		output    = subprocess.check_output(command)
		output = output.split(b'.exe')
		crashDumpCount = len(output) - 1
	except Exception as ex:
		print(ex)
		crashDumpCount = 0
	
	#get paths for exe in Loose folder
	projects = GetFilesPathByExtension("Xbox/Examples_3", "exe", False)
	fileList = []
	filenameList = []
	for proj in projects:
		if "XBOX Visual Studio 2017" in proj:# and "Release" in proj:
			if "Loose" in proj and "Release" in proj:
				fileList.append(os.path.dirname(proj))
				filename = proj.split('\\')
				filename = filename[len(filename)-1]
				filenameList.append(filename)

	#Register Loose folders and store app names
	appRootList = []
	appNameList = []
	for filename in fileList:
		#register app on xbox
		print ("Registering Network Share: " + filename)
		command = [gdkDir+'xbapp', "registernetworkshare",filename]
		output = XBoxCommand(command, False)

		#Extract App Name from output
		appName = "InvalidAppName"
		for item in output:
			if "Game" in item:
				appName = item.strip()
				appNameList.append(appName)
				appRootList.append(filename)
				print ("Successfully registered: " + appName)
		if appName == "InvalidAppName":
			print ("Failed to register network share: " + filename)
			failedTests.append({'name':filename, 'gpu':"", 'reason':"Invalid app name", 'time':0.0})
			errorOccured = True
		print ("")

	#Launch the registered apps
	for filename, appRoot, appName in zip(filenameList, appRootList, appNameList):
		filenameWithoutExe = filename.split('.exe')[0]

		#wait for home to run. something is probably wrong by that point
		command = [gdkDir+'xbapp', "query","/X"+consoleIP, homePackageName]
		commandLaunchHome = [gdkDir+'xbapp', "launch","/X"+consoleIP, homePackageName]
		timeout = time.time() + float(60)
		backAtHome = False
		print("Waiting for Dashboard to run.")
		while (backAtHome != True) and time.time() < timeout:
			output = XBoxCommand(command, False)
			if 'running' in output[0]:
				backAtHome = True
			else:
				XBoxCommand(commandLaunchHome, True)
				time.sleep(0.5)

		if backAtHome == False:
			print("Xbox did not go back to home before launching.")
			print("Aborting test as it will fail.")
			return -1
		print("Dashboard running")

		command = [gdkDir+'xbapp',"launch","/X"+consoleIP, appName]

		if benchmarkFrames > 0:
			command.append("-b")
			command.append(str(benchmarkFrames))

		startTime = time.time()
		output = XBoxCommand(command)
		print(output)
		#Make sure app launches
		if len(output) < 2 or not "successfully" in output[1]:
			errorOccured = True
			command = [gdkDir+'xbapp',"terminate","/X"+consoleIP, appName]
			output = XBoxCommand(command)
			print ("The operation failed")
			failedTests.append({'name':appName, 'gpu':"", 'reason':"Failed to launch app", 'time':time.time()-startTime})
			continue


		command = [gdkDir+'xbapp',"query","/X"+consoleIP, appName]
		isRunning = int(1)
		testingComplete = False
		dumpFilePath = "Invalid"

		#Check if app terminatese or times out
		print("Waiting for App to terminate")
		timeout = time.time() + float(maxIdleTime)
		while (isRunning != 0 or b"0x8000000A" in output) and time.time() < timeout:
			output = XBoxCommand(command, False)
			if 'unknown' in output[0]:
				print("App successfully terminated.")
				isRunning = 0
			else:
				time.sleep(0.7)

		elapsedTime = time.time() - startTime

		# Timeout Error
		if isRunning != 0:
			errorOccured = True
			print ("Timeout: " + appName + "\n")
			command = [gdkDir+'xbapp',"terminate","/X"+consoleIP, appName]
			output = XBoxCommand(command)
			failedTests.append({'name':appName, 'gpu':"", 'reason':"Runtime failure", 'time':elapsedTime})
		else:
			testingComplete = True
			
			#Wait for crash dump folder to be discoverable and get count of crash dumps
			command = ["cmd", "/c", "dir", crashdump_path]
			rc = 1
			#Create a crashdump directory if it doesn't exist
			if not os.path.exists(crashdump_path):
				os.mkdir(crashdump_path)    
			while rc != 0:
				rc = subprocess.call(command, stdin=None, stdout=FNULL, stderr=FNULL)
			output = XBoxCommand(command, False)

			# Check if a new crash dump was generated
			currentCrashDumpCount = len( [line for line in output if '.exe' in line] )
			if (currentCrashDumpCount > crashDumpCount):
				crashDumpCount = len(output) - 1
				# Check if it is the current test
				dumpFiles = GetFilesPathByExtension(crashdump_path, "dmp", False)
				dumpFilePath = "Invalid"
				for dump in dumpFiles:
					if filenameWithoutExe in dump:
						dumpFilePath = dump
						testingComplete = False
						break

			# get the memory leak file path
			systemScratchPath = '\\\\'+consoleIP+'\\'+'SystemScratch'+'\\'
			memleakPath = systemScratchPath + filenameWithoutExe + ".memleaks"

			if testingComplete and benchmarkFrames > 0:
				benchmarkData = RecordBenchmarkFilePath("Xbox", filenameWithoutExe, systemScratchPath)
				print(benchmarkData)
			
			leaksDetected = FindMemoryLeaks(memleakPath)
			
			if testingComplete and leaksDetected == True:
				errorOccured = True
				failedTests.append({'name':appName, 'gpu':"", 'reason':"Memory Leaks", 'time':elapsedTime})
			elif testingComplete:
				print ("Successfully ran " + appName + "\n")
				successfulTests.append({'name': appName, 'gpu': "", 'time':elapsedTime})
			else:
				errorOccured = True
				print ("Application Terminated Early: " + appName + "\n")
				failedTests.append({'name': appName, 'gpu': "", 'reason': "Runtime failure", 'time':elapsedTime})
		
		logFilePath = f"\\\\{consoleIP}\\SystemScratch\\" + filenameWithoutExe + ".log"
		if os.path.exists(logFilePath) and os.path.isfile(logFilePath):
			with open(logFilePath) as f:
				print(f.read())

		def toCommandString(command) -> str:
			quotedCommand = [f'"{x}"' for x in command]
			return f"& {' '.join(quotedCommand)}"

		if testingComplete == False:
			def ExecuteCdb(commands: str, timeout: int, parseParagraph: bool = False, prefixToParse: str = None) -> str:
				cdbPath = os.path.join("C:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64\\kd.exe")
				gdkSymbolPath = os.path.join(os.getenv("GXDKLatest"), "gameKit\\symbols")
				cdbCommand = [cdbPath, "-z", dumpFilePath, "-y", gdkSymbolPath, "-i", appRoot, "-c", f"{commands};qd"]
				print(f"Executing CDB Command: {toCommandString(cdbCommand)}")
				cdbOutput = subprocess.run(cdbCommand, capture_output=True, universal_newlines=True, timeout=timeout).stdout
				if parseParagraph and prefixToParse != None:
					parseBegin = cdbOutput.find(prefixToParse)
					if parseBegin >= 0:
						parseBegin += len(prefixToParse)
						parseEnd = cdbOutput.find("\n\n", parseBegin)
					if parseBegin == -1 or parseEnd == -1:
						print(f"Failed to find '{prefixToParse}' in the text below:\n{cdbOutput}")
						return None
					return cdbOutput[parseBegin:parseEnd]
				return cdbOutput
			
			print("Stack Trace:")
			stackOutput = ExecuteCdb("!sym noisy;.lines -e;.reload /f;!analyze -v", 60, True, "STACK_TEXT:")
			print(stackOutput)

		cleanupSharedFiles()

	#Copy crash dumps to PC and delete shared folders from the console
	try:
		command = ["xcopy", crashdump_path, "C:\\dumptemp\\", "/s", "/e"]
		output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)
		copy_tree("C:\\dumptemp", "C:\\XboxOneCrashDumps")
		shutil.rmtree("C:\\dumptemp")
		shutil.rmtree(crashdump_path)
		os.makedirs(crashdump_path)
	except Exception as ex:
		print("Error ocurred copying crash dumps back to C:\\dumptemp")
		print(ex)
	FNULL.close()

	if errorOccured == True:
		return -1
	return 0

def TestNintendoSwitchProjects(benchmarkFrames, sanitizer_pass):
	errorOccured = False
	
	switchToolsDir = os.environ['NINTENDO_SDK_ROOT'] + '/Tools/CommandLineTools/'
	controlTargetExe = os.path.join(switchToolsDir, "ControlTarget.exe")
	runOnTargetExe = os.path.join(switchToolsDir, "RunOnTarget.exe")
	dumpDirectory = os.path.join(str(Path.home()), "PyBuildSwitchDumps")
	os.makedirs(dumpDirectory, exist_ok=True)

	#get paths for exe in Loose folder
	projects = GetFilesPathByExtension("./Switch/Examples_3","nspd",True)
	fileList = []

	config = "Debug" if sanitizer_pass else "Release"

	for proj in projects:
		if "NX Visual Studio 2017" in proj and config in proj:
			fileList.append(proj)

	#Launch the deployed apps
	for proj in fileList:
		readLogProc = subprocess.Popen([controlTargetExe, "read-log"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)

		filename = proj.split(os.path.sep)[-1]

		#Get Contents Dir as this where we output stuff
		outputDir = os.path.dirname(os.path.dirname(proj))+f"{os.path.sep}Contents"
		appName = os.path.splitext(os.path.basename(proj))[0]

		command = runOnTargetExe + ' "'+proj+'"'
		if benchmarkFrames > 0:
			command += f' -- -b {benchmarkFrames}'
		command +=' --failure-timeout '+str(maxIdleTime)+' --pattern-failure-exit "Assert|Break|Panic|Halt|Fatal|GpuCoreDumper"' + f' -- "{dumpDirectory}"'
		if "Debug" in proj:
			filename = "Debug_"+filename
		else:
			filename = "Release_"+filename
		retCode = ExecuteTest(command, filename, False, check_ub_error=True)

		command = [controlTargetExe, "terminate"]
		ExecuteCommand(command, None)
		if retCode != 0:
			command = [controlTargetExe, "reset"]
			ExecuteCommand(command, None)
			errorOccured = True
		
		try:
			log = str(readLogProc.communicate(timeout=5)[0])
		except subprocess.TimeoutExpired:
			readLogProc.kill()
			log = str(readLogProc.communicate()[0])
		print(log)

		if retCode != 0:
			try:
				print("***\nStack Trace\n***")
				nssPath = proj.rsplit(".", 1)[0] + ".nss"
				nssPath = os.path.abspath(nssPath)
				addr2line = os.path.join(os.environ['NINTENDO_SDK_ROOT'], 'Compilers/NX/nx/aarch64/bin/aarch64-nintendo-nx-elf-addr2line.exe')
				dumpFilePath = os.path.join(dumpDirectory, os.path.basename(proj).rsplit(".", 1)[0] + "_stack_trace.txt")
				with open(os.path.join(dumpFilePath)) as f:
					numLines = int(f.readline())
					for _ in range(numLines):
						offset = f.readline().strip()
						if offset == "??":
							print("?? (unknown symbol)")
						else:
							subprocess.run([addr2line, "-a", "-f", "-C", "-i", "-p", "-e", f"{nssPath}", offset])
				print("\n\n")
			except Exception as e:
				print(f"Failed to print stack trace due to {e}")
		elif benchmarkFrames > 0:
				benchmarkData = RecordBenchmarkFilePath("Switch", appName, outputDir)
				print(benchmarkData)

	if errorOccured == True:
		return -1
	return 0

def TestOrbisProjects(benchmarkFrames, sanitizer_pass):
	errorOccured = False
	FNULL = open(os.devnull, 'w')
	
	orbis_server = ET.parse(os.environ["LOCALAPPDATA"] + "/SCE/ORTM/Server.xml")
	cache_data = orbis_server.getroot().find("Cache").find("CacheData")

	target_ip = ""
	for target_item in cache_data.findall("item"):
		default = target_item.find("Default")
		if default.text == "1":
			target_ip = target_item.find("Host").text
			break

	#get paths for exe in Loose folder
	projects = GetFilesPathByExtension("PS4/Examples_3","elf",False)
	fileList = []
	errorOccured = False

	config = "Debug" if sanitizer_pass else "Release"

	for proj in projects:
		if "PS4 Visual Studio 2017" in proj and config in proj:
			fileList.append(proj)

	for filename in fileList:
		workingDir = os.path.dirname(filename)
		appName = os.path.splitext(os.path.basename(filename))[0]
		memleakFile = os.path.join(workingDir, appName + ".memleaks")
		logFilePath = os.path.join(workingDir, appName + ".log")
		# delete the memory leak file before execute the app
		if os.path.exists(memleakFile):
			os.remove(memleakFile)
		
		command = ["orbis-run.exe", "/elf", filename]
		
		if sanitizer_pass:
			command.append("UBSAN_OPTIONS=ps4_break_on_error=true")

		if benchmarkFrames > 0:
			command.append("-b")
			command.append(f"{benchmarkFrames}")
		
		retCode = ExecuteTest(command, appName, False)

		logFileData = ""
		try:
			with open(logFilePath) as f:
				logFileData = f.read()
				print(logFileData)
		except Exception as e:
			print(f"Failed to load log file due to {e}")

		leaksDetected = FindMemoryLeaks(memleakFile)
		if retCode == 0 and leaksDetected == True:
			lastSuccess = successfulTests.pop()
			failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Memory Leaks", 'time':lastSuccess['time']})
			errorOccured = True

		if retCode != 0:
			processIdBegin = logFileData.find("Process ID: ")
			if processIdBegin >= 0:
				processIdStr = logFileData[processIdBegin + 12:processIdBegin + 20]
				processId = int(processIdStr, base=16)

				# Find the most recent orbisdmp file that matches process id
				coredumpRoot = os.path.join("O:\\", target_ip, "data", "sce_coredumps")
				coredumpDirectories = [d for d in os.listdir(coredumpRoot) if os.path.isdir(os.path.join(coredumpRoot, d))]
				coredumpDirectories.sort(key=lambda x: int(x[len(x) - 10:], 16) if '0x' in x[len(x) - 10:] else int(x[len(x) - 10:]), reverse=True)
				latestCoredumpDirectories = coredumpDirectories[:5]
				targetDumpFile = None
				for d in latestCoredumpDirectories:
					dumpFiles = list(Path(os.path.join(coredumpRoot, d)).glob("*.orbisdmp"))
					if len(dumpFiles) > 0:
						dumpFile = dumpFiles[0]
						dumpProcessId = int(dumpFile.name[23:31], base=16)
						if processId == dumpProcessId:
							targetDumpFile = dumpFile
							break
				
				# Analyze coredump
				if targetDumpFile != None:
					print("\n***\nCrash Log\n***\n")
					print(f"Dump file path: {targetDumpFile}")
					try:
						tempScriptFilename = "orbis_coreview_temp_script.txt"
						with open(tempScriptFilename, "w+") as f:
							f.write(f'corefile load "{str(targetDumpFile.absolute())}"\n')
							f.write("thread list\n")
							f.write("process analysis\n")
							f.write("stack list\n")
							f.write(".quit")
						coreviewPath = os.path.join(os.environ["SCE_ROOT_DIR"], "ORBIS", "Tools", "Debugger", "bin", "x64", "orbis-coreview.exe")
						coreviewResult = subprocess.run([coreviewPath, "--nologo", "--script", tempScriptFilename], capture_output=True)
						crashLog = coreviewResult.stdout
						crashLog = crashLog.decode(encoding='utf-16')
						print(crashLog)
						os.remove(tempScriptFilename)
					except Exception as e:
						print(f"Failed write a temporary coreview script file due to {e}")
				else:
					print("Couldn't find core dump file")

			errorOccured = True
		elif benchmarkFrames > 0:
				benchmarkData = RecordBenchmarkFilePath("Orbis", appName, workingDir)
				print(benchmarkData)

	if errorOccured:
		return -1

	return 0

def TestProsperoProjects(benchmarkFrames, sanitizer_pass):
	errorOccured = False
	FNULL = open(os.devnull, 'w')
	
	#get paths for exe in Loose folder
	projects = GetFilesPathByExtension("Prospero/Examples_3","elf",False)
	fileList = []
	errorOccured = False

	config = "Debug" if sanitizer_pass else "Release"

	for proj in projects:
		if "Prospero Visual Studio 2017" in proj and config in proj:
			fileList.append(proj)

	for filename in fileList:
		workingDir = os.path.dirname(filename)
		appName = os.path.splitext(os.path.basename(filename))[0]
		memleakFile = os.path.join(workingDir, appName + ".memleaks")
		logFilePath = os.path.join(workingDir, appName + ".log")
		elfName = appName + ".elf"
		dumpDir = os.path.join(workingDir, "coredump")
		# delete the memory leak file before execute the app
		if os.path.exists(memleakFile):
			os.remove(memleakFile)
		elfArg = "/elf \"" + filename + "\""
		command = "prospero-run.exe /debug " + elfArg
		if benchmarkFrames > 0:
			command += f" -b {benchmarkFrames}"
		retCode = ExecuteTest(command, appName, False)

		logFileData = ""
		try:
			with open(logFilePath) as f:
				logFileData = f.read()
				print(logFileData)
		except Exception as e:
			print(f"Failed to load log file due to {e}")

		leaksDetected = FindMemoryLeaks(memleakFile)
		if retCode == 0 and leaksDetected == True:
			lastSuccess = successfulTests.pop()
			failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Memory Leaks", 'time':lastSuccess['time']})
			errorOccured = True

		if retCode != 0:
			if not os.path.exists(dumpDir):
				os.mkdir(dumpDir)
			processDmpCmd = [ "prospero-ctrl", "process-dump", "trigger", elfName, dumpDir, "FULL" ]
			ExecuteCommand(processDmpCmd, None)

			targetDumpFile = None
			dumpFiles = list(Path(dumpDir).glob("*.prosperodmp"))
			if len(dumpFiles) > 0:
				dumpFile = dumpFiles[0]
				targetDumpFile = dumpFile
			
			# Analyze coredump
			if targetDumpFile != None:
				print("\n***\nCrash Log\n***\n")
				print(f"Dump file path: {targetDumpFile}")
				try:
					tempScriptFilename = "prospero_coreview_temp_script.txt"
					with open(tempScriptFilename, "w+") as f:
						f.write(f'corefile load "{str(targetDumpFile.absolute())}"\n')
						f.write("thread list\n")
						f.write("process analysis\n")
						f.write("stack list\n")
						f.write(".quit")
					coreviewPath = os.path.join(os.environ["SCE_ROOT_DIR"], "Prospero", "Tools", "Debugger", "bin", "x64", "prospero-coreview.exe")
					coreviewResult = subprocess.run([coreviewPath, "--nologo", "--script", tempScriptFilename], capture_output=True)
					crashLog = coreviewResult.stdout
					crashLog = crashLog.decode(encoding='utf-16')
					print(crashLog)
					os.remove(tempScriptFilename)
				except Exception as e:
					print(f"Failed write a temporary coreview script file due to {e}")
			else:
				print("Couldn't find core dump file")
				
			processKillCmd = [ "prospero-ctrl", "process", "kill", "/process:" + elfName ]
			ExecuteCommand(processKillCmd, None)

			errorOccured = True
		elif benchmarkFrames > 0:
				benchmarkData = RecordBenchmarkFilePath("Prospero", appName, workingDir)
				print(benchmarkData)

	if errorOccured:
		return -1

	return 0

def GetAndroidKeyEventCmd(key):
	return ["adb", "shell","input" ,"keyevent", key]

def AndroidADBCheckRunningProcess(adbCommand, processName, packageName):
	output = processName
	waitingForExit = True
	counter = 0
	while waitingForExit == True:
		output = ExecuteCommandWOutput(adbCommand, False)
		output = (b"".join(output)).decode('utf-8')
		print(output)
		
		# try to match the number of leaks. if this doesn't match a valid ressult then no leaks were detected.
		runningMatch = re.findall(r"(S|D)\s+com.forge.unittest", output, re.MULTILINE | re.IGNORECASE)
		print(runningMatch)

		if len(runningMatch) > 0:
			value = runningMatch[0]
			#try to print only section containing the source of leaks
			if value == "D":
				return True
			if counter > 50:
				ExecuteCommand(GetAndroidKeyEventCmd("96"), None)
				counter = 0
		
		if processName not in output:
			waitingForExit = False

		counter = counter + 1

	return True

def TestAndroidProjects(benchmarkFrames, quest):
	errorOccured = False

	lowEndExamples = ["Transformations", "FontRendering", "ZipFileSystem", "UserInterface", "EntityComponentSystem"]
    
	solutionPath = "./Examples_3/Unit_Tests/Android_VisualStudio2017" if not quest else "./Examples_3/Unit_Tests/Quest_VisualStudio2017"
	solutionDir = "Android_VisualStudio2017" if not quest else "Quest_VisualStudio2017"

	projects = GetFilesPathByExtension(solutionPath,"apk",False)
	fileList = []

	for proj in projects:
		if solutionDir in proj and "Release" in proj and "Packaging" not in proj and "debug" not in proj.lower():
			fileList.append(proj)
	
	for proj in fileList:
		leaksDetected = False
		#get current path for sln file
		#strip the . from ./ in the path
		#replace / by the os separator in case we need // or \\
		rootPath = os.getcwd() + proj.strip('.')
		rootPath = rootPath.replace("/",os.sep)
		#need to get root folder of path by stripping the filename from path
		rootPath = rootPath.split(os.sep)[0:-1]
		rootPath = os.sep.join(rootPath)

		#save root directory where python is executed from
		currDir = os.getcwd()
		
		apkName = proj.split(os.sep)[-1]
		filenameNoExt = apkName.split('.')[0]
		fullAppName = "com.forge.unittest." + filenameNoExt
		nativeActivity = "/com.forge.unittest.ForgeBaseActivity"
		#change working directory to sln file
		os.chdir(rootPath)
		#origFilename = filename
		#unlockScreenCommand = ["adb","shell", "input", "keyevent", "82"]
		uninstallCommand = ["adb", "uninstall",fullAppName]
		grepPSCommand = ["adb", "shell", "ps","| grep", fullAppName]
		installCommand = ["adb", "install", "-r", apkName]
		runCommand = ["adb", "shell", "am", "start", "-W"] 
		if benchmarkFrames > 0:
			runCommand.append("-e")
			runCommand.append("-b")
			runCommand.append(f"{benchmarkFrames}")
		runCommand += ["-n", fullAppName + nativeActivity]
		stopAppCommand = ["adb", "shell", "am", "force-stop" , fullAppName]
		logCatCommand = ["adb", "logcat","-d", "-s", "The-Forge", "the-forge-app"]
		clearLogCatCommand = ["adb", "logcat", "-c"]
		unlockScreenCommand = ["adb", "shell","input" ,"keyevent", "82", "&&", "adb", "shell", "input" ,"keyevent", "66"]
		lockScreenCommand = ["adb", "shell","input" ,"keyevent", "82", "&&", "adb", "shell", "input" ,"keyevent", "26", "&&", "adb", "shell", "input" ,"keyevent", "26"]
		disableProximitySensorCommand = ["adb", "shell","am" ,"broadcast", "-a", "com.oculus.vrpowermanager.prox_close"]
		enableProximitySensorCommand = ["adb", "shell","am" ,"broadcast", "-a", "com.oculus.vrpowermanager.automation_disable"]
			
		def LockScreenCmd():
			ExecuteCommand(GetAndroidKeyEventCmd("82"), None)
			ExecuteCommand(GetAndroidKeyEventCmd("26"), None)
			if quest:
				ExecuteCommand(enableProximitySensorCommand, None)
			
		def UnlockScreenCmd():
			ExecuteCommand(GetAndroidKeyEventCmd("26"), None)
			ExecuteCommand(GetAndroidKeyEventCmd("82"), None)
			if quest:
				ExecuteCommand(disableProximitySensorCommand, None)
		
		retCode = ExecuteCommand(uninstallCommand, None)[0]
		retCode = ExecuteCommand(installCommand, sys.stdout)[0]
		ExecuteCommand(clearLogCatCommand, None)
		UnlockScreenCmd()
		retCode = ExecuteTest(runCommand, filenameNoExt, True)
		AndroidADBCheckRunningProcess(grepPSCommand, filenameNoExt, apkName)
		time.sleep(2)
		output = ExecuteCommandWOutput(logCatCommand)	
		output = (b"\n".join(output).decode('utf-8'))	
		print(output)

		if "Success terminating application" not in output:
			retCode = 1
			lastSuccess = successfulTests.pop()
			failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Runtime Failure", 'time':lastSuccess['time']})
		else : retCode = 0

		ExecuteCommand(stopAppCommand, sys.stdout)
		LockScreenCmd()

		if retCode == 0:
			pullUTFiles = ["adb", "pull",f"/sdcard/Android/data/{fullAppName}/files"]
			ExecuteCommand(pullUTFiles, sys.stdout)
			#get log file to extract actuall app name unrelated to apk name.
			logFiles = GetFilesPathByExtension("./files", "log", False, 1)
			appName = filenameNoExt
			if len(logFiles) > 0:
				logFilePath = logFiles[0]
				#remove extension and get base name of path (everything to left of last // gets remoed)
				appName = os.path.basename(os.path.normpath(logFilePath.split('.log')[0]))				
			#get memleak file
			memleakFiles = GetFilesPathByExtension("./files", "memleaks", False, 1)
			if len(memleakFiles) > 0:
				memleakFile = memleakFiles[0]
				leaksDetected = FindMemoryLeaks(memleakFile)
				if leaksDetected == True:
					lastSuccess = successfulTests.pop()
					failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Memory Leaks", 'time':lastSuccess['time']})
					retCode = 1
			else:
				print("Couldn't find mem leak file")
			print()
			
			benchmarkData = RecordBenchmarkFilePath("Android", appName, "./files")
			print(benchmarkData)
		
		
		if retCode != 0:
			print("Error while running " + filenameNoExt + " (possibly overheating issue)")
			errorOccured = True
				
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0	

#this needs the JAVA_HOME environment variable set up correctly
def BuildAndroidProjects(skipDebug, skipRelease, printMSBuild, quest):
	errorOccured = False
	msBuildPath = FindMSBuild17()
	androidConfigurations = ["Debug", "Release"]
	androidPlatform = ["Android-arm64-v8a"] # We don't test ARM platform

	if skipDebug:
		androidConfigurations.remove("Debug")
	
	if skipRelease:
		androidConfigurations.remove("Release")

	if msBuildPath == "":
		print("Could not find MSBuild 17, Is Visual Studio 17 installed ?")
		sys.exit(-1)
		
	solutionPath = "./Examples_3/Unit_Tests/Android_VisualStudio2017" if not quest else "./Examples_3/Unit_Tests/Quest_VisualStudio2017"
	platformName = "Android" if not quest else "Quest"
	
	if quest:
		if os.path.isdir("C:\\ovr_sdk_mobile_1.50.0"):
			copy_tree("C:\\ovr_sdk_mobile_1.50.0", "Common_3\\OS\\ThirdParty\\OpenSource\\ovr_sdk_mobile")
		if os.path.isdir("C:\\Vulkan-ValidationLayer-1.2.182.0"):
			copy_tree("C:\\Vulkan-ValidationLayer-1.2.182.0", "Common_3\\ThirdParty\\OpenSource\\Vulkan-ValidationLayer-1.2.182.0")
		

	projects = GetFilesPathByExtension("./Jenkins/","buildproj",False)
	fileList = []
	for proj in projects:
		if platformName in proj:
			fileList.append(proj)
	
	#if MSBuild tasks were not found then parse all projects
	if len(fileList) == 0:
		fileList = GetFilesPathByExtension(solutionPath,"sln",False)

	msbuildVerbosity = "/verbosity:minimal"
	msbuildVerbosityClp = "/clp:ErrorsOnly;WarningsOnly;Summary"
	
	if printMSBuild: 
		msbuildVerbosity = "/verbosity:normal"
		msbuildVerbosityClp = "/clp:Summary;PerformanceSummary"
				
	for proj in fileList:
		#get current path for sln file
		#strip the . from ./ in the path
		#replace / by the os separator in case we need // or \\
		rootPath = os.getcwd() + proj.strip('.')
		rootPath = rootPath.replace("/",os.sep)
		#need to get root folder of path by stripping the filename from path
		rootPath = rootPath.split(os.sep)[0:-1]
		rootPath = os.sep.join(rootPath)
				
		#save root directory where python is executed from
		currDir = os.getcwd()
		#change working directory to sln file
		os.chdir(rootPath)

		#strip extension
		filename = proj.split(os.sep)[-1]
		
		for platform in androidPlatform:
			if ".sln" in proj:
				for conf in androidConfigurations:
					command = [msBuildPath ,filename,"/p:Configuration="+conf,"/p:Platform=" + platform,"/nr:false",msbuildVerbosityClp,msbuildVerbosity,"/t:Build"]
					#print(command)
					retCode = ExecuteBuild(command, filename, conf, platform)
			elif ".buildproj" in proj:
				command = [msBuildPath, filename, "/p:Platform=" + platform, "/m","/nr:false",msbuildVerbosityClp,msbuildVerbosity,"/t:Build"]
				retCode = ExecuteBuild(command, filename, "All Configurations", platform)

		if retCode != 0:
			errorOccured = True
		
		os.chdir(currDir)
	
	if errorOccured == False:
		# Verify APKs
		print("Validating APKs...")
		apks = GetFilesPathByExtension(solutionPath,"apk",False)
		for apk in apks:
			apkCmd = [os.environ['ANDROID_SDK_ROOT'] + "/cmdline-tools/latest/bin/apkanalyzer.bat", "apk", "summary", apk]
			retCode = ExecuteCommand(apkCmd, sys.stdout)[0]
			if retCode != 0:
				print("Failed validating " + apk + ", retCode = " + str(retCode))
				errorOccured = True

	if errorOccured == True:
		return -1
	return 0  

def FilterSolutionProjects(UT_solution):
	dependencies_filter = {
		"OS",
		"gainputstatic",
		"LuaManager",
		"ozz_animation",
		"ozz_animation_offline",
		"ozz_base",
		"RendererVulkan",   # switch
		"RendererGnm",      # orbis
		"RendererProspero", # prospero
		"SpirvTools"
	}
	hash_pattern = "\{\w{8}-\w{4}-\w{4}-\w{4}-\w{12}\}"

	proj_pattern = re.compile('Project\("' + hash_pattern + '"\) = "(\w+)", ".+\.vcxproj", "(' + hash_pattern + ')"')
	# after the | each platform has a different alphanumerical pattern
	proj_filter_hash_patten = re.compile('(' + hash_pattern + ')\.(Debug|Release)\|\w+\.B')

	f = open(UT_solution)
	sln = f.readlines()
	f.close()
	
	proj_hashes = set()
	# get project hashes
	for line in sln:
		match = proj_pattern.search(line)
		if match: # line that might contain a project hash
			proj = match.group(1)
			# if true, it is a project we are interested in for the sanitizer pass
			# so save its hash
			if proj in sanitizer_UTs or proj in dependencies_filter:
				proj_hashes.add(match.group(2))

	filtered_sln = ""
	# filter out projects that are not in the proj_hashes set
	for line in sln:
		match = proj_filter_hash_patten.search(line)
		if match: # we are in a location of interest in the sln file
			hash = match.group(1)
			# if it's a hash we have, copy it to the new sln file
			if hash in proj_hashes:
				filtered_sln += line
		else: # not an important location in sln, just copy
			filtered_sln += line

	f = open(UT_solution[:-4] + "_Sanitizer_Pass.sln", "w")
	f.write(filtered_sln)
	f.close()

def RemoveSanitizerSolution(arguments):
	if arguments.orbis:
		solutions = GetFilesPathByExtension("./PS4/Examples_3/Unit_Tests","sln",False)
	elif arguments.prospero:
		solutions = GetFilesPathByExtension("./Prospero/Examples_3/Unit_Tests","sln",False)
	else:
		solutions = GetFilesPathByExtension("./Switch/Examples_3/Unit_Tests","sln",False)

	for sln in solutions:
		if "Sanitizer_Pass" in sln:
			os.remove(sln)

def BuildWindowsProjects(args):
	sanitizer_pass = args.switchNX and args.ubsan and not args.assetpipeline
	errorOccured = False
	msBuildPath = FindMSBuild17()
	if msBuildPath == "":
		print("Could not find MSBuild 17, Is Visual Studio 17 installed ?")
		sys.exit(-1)

	pcConfigurations = ["Debug", "Release"]
	pcPlatform = "x64"
	isWindows7 = int(platform.release()) < 10
	
	if args.skipdebugbuild:
		pcConfigurations.remove("Debug")
		
	if args.skipreleasebuild:
		pcConfigurations.remove("Release")

	switchPlatform = "NX64"

	if isWindows7:
		print("Detected Windows 7")
		if "DebugDx" in pcConfigurations : pcConfigurations.remove("DebugDx")
		if "ReleaseDx" in pcConfigurations : pcConfigurations.remove("ReleaseDx")
		if "Debug" in pcConfigurations : pcConfigurations.remove("Debug")
		if "Release" in pcConfigurations : pcConfigurations.remove("Release")
		skipAura = True
	

	xboxPlatform = "Gaming.Xbox.XboxOne.x64"

	projects = GetFilesPathByExtension("./Jenkins/","buildproj",False)
	if args.assetpipeline:
		projects = [path for path in projects if"assetpipeline" in path.lower()]
	
	#if MSBuild tasks were not found then parse all projects
	if len(projects) == 0:
		projects = GetFilesPathByExtension("./Examples_3/","sln",False)

	fileList = []
	msbuildVerbosity = "/verbosity:minimal"
	msbuildVerbosityClp = "/clp:ErrorsOnly;WarningsOnly;Summary"
	
	if args.printbuildoutput: 
		msbuildVerbosity = "/verbosity:normal"
		msbuildVerbosityClp = "/clp:Summary;PerformanceSummary"

	if not args.xboxonly and not args.switchNX:
		# if true, filter everything that doesn't contain Win7 or contains HLSLParser
		# else, filter all platforms which aren't W10
		if isWindows7:
			filter_pattern = "^((?!Win7).)*$|HLSLParser"
		elif args.assetpipeline:
			filter_pattern = "^AssetPipeline"
		else:
			filter_pattern = "Android|Orbis|Prospero|Switch|Xbox|XBOXOne|Win7|Quest"

		if args.skipaura:
			filter_pattern += "|Aura"
		
		proj_filters = re.compile(filter_pattern)

		for proj in projects:
			match = proj_filters.search(proj)
			if match:
				continue

			fileList.append(proj)


	if args.xbox:
		for proj in projects:
			if args.skipaura == True and "Aura" in proj:
				continue
			if "Xbox" in proj or "XBOXOne" in proj:
				fileList.append(proj)
				
	if args.switchNX:
		if sanitizer_pass:
			for proj in projects:
				if "Sanitizer_Pass_Switch" in proj:
					fileList.append(proj)
					break

			vcxprojects = GetFilesPathByExtension("./Switch/Examples_3/","vcxproj",False)
			AddSanitizers(vcxprojects, args.ubsan, False, isSwitch=True)	
			solutions = GetFilesPathByExtension("./Switch/Examples_3/Unit_Tests","sln",False)
			FilterSolutionProjects(solutions[0])
		else:
			for proj in projects:
				if "Switch" in proj and not "Sanitizer_Pass" in proj:
					fileList.append(proj)
	
	for proj in fileList:
		if "orbis" in proj.lower():
			continue
		elif "prospero" in proj.lower():
			continue
		#get current path for sln file
		#strip the . from ./ in the path
		#replace / by the os separator in case we need // or \\
		rootPath = os.getcwd() + proj.strip('.')
		rootPath = rootPath.replace("/",os.sep)
		#need to get root folder of path by stripping the filename from path
		rootPath = rootPath.split(os.sep)[0:-1]
		rootPath = os.sep.join(rootPath)

		#save root directory where python is executed from
		currDir = os.getcwd()
		#change working directory to sln file
		os.chdir(rootPath)

		configurations = pcConfigurations
		
		#strip extension
		filename = proj.split(os.sep)[-1]
		
		#if filename == "VisibilityBuffer.sln" or filename == "Ephemeris.sln":
		#	if "DebugDx11" in configurations : configurations.remove("DebugDx11")
		#	if "ReleaseDx11" in configurations : configurations.remove("ReleaseDx11")
		#elif filename == "HLSLParser.sln":
		#	configurations = ["Debug", "Release"]
			
		if "Xbox" in proj or "XBOXOne" in proj:
			currPlatform = xboxPlatform
		elif "Switch" in proj or "NX Visual Studio 2017" in proj:
			currPlatform = switchPlatform
		else:
			currPlatform = pcPlatform

		#for conf in configurations:
		if ".sln" in filename:
			for conf in configurations:
				if args.switchNX:
					command = [msBuildPath ,filename,"/p:Configuration="+conf,"/p:Platform=" + currPlatform,"/p:BuildInParallel=true","/nr:false",msbuildVerbosityClp,msbuildVerbosity,"/t:Build"]
				else:
					command = [msBuildPath ,filename,"/p:Configuration="+conf,"/p:Platform=" + currPlatform,"/m","/p:BuildInParallel=true","/nr:false",msbuildVerbosityClp,msbuildVerbosity,"/t:Build"]

				if isWindows7:
					command.append("/p:WindowsTargetPlatformVersion=8.1")
				retCode = ExecuteBuild(command, filename,conf, currPlatform)
		else:
			command = [msBuildPath ,filename,"/p:Platform=" + currPlatform,"/m", "/nr:false",msbuildVerbosityClp,msbuildVerbosity,"/t:Build"]

			if isWindows7:
				command.append("/p:WindowsTargetPlatformVersion=8.1")
			retCode = ExecuteBuild(command, filename,"All Configurations", currPlatform)
		
		if retCode != 0:
			errorOccured = True
				
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0	

def BuildOrbisProjects(skipDebug, skipRelease, printMSBuild, ubsan, asan):
	sanitizer_pass = ubsan or asan
	errorOccured = False
	msBuildPath = FindMSBuild17()

	configurations = ["Debug", "Release"]
	platform = "ORBIS"
	
	if skipDebug:
		configurations.remove("Debug")
		
	if skipRelease:
		configurations.remove("Release")

	#xboxConfigurations = ["Debug","Release"]

	if msBuildPath == "":
		print("Could not find MSBuild 17, Is Visual Studio 17 installed ?")
		sys.exit(-1)

	projects = GetFilesPathByExtension("./Jenkins/","buildproj",False)
	
	#if MSBuild tasks were not found then parse all projects
	if len(projects) == 0:
		projects = GetFilesPathByExtension("./Examples_3/","sln",False)

	fileList = []
	msbuildVerbosity = "/verbosity:minimal"
	msbuildVerbosityClp = "/clp:ErrorsOnly;WarningsOnly;Summary"
	
	if printMSBuild:
		msbuildVerbosity = "/verbosity:normal"
		msbuildVerbosityClp = "/clp:Summary;PerformanceSummary"

	if sanitizer_pass:
		for proj in projects:
			if "Sanitizer_Pass_Orbis" in proj:
				fileList.append(proj)
				break
		
		vcxprojects = GetFilesPathByExtension("./PS4/Examples_3/","vcxproj",False)
		AddSanitizers(vcxprojects, ubsan, asan, isOrbis=True)
		solutions = GetFilesPathByExtension("./PS4/Examples_3/Unit_Tests","sln",False)
		FilterSolutionProjects(solutions[0])
	else:
		for proj in projects:
			if "Orbis" in proj and not "Sanitizer_Pass" in proj:
				fileList.append(proj)
				
	for proj in fileList:
		#get current path for sln file
		#strip the . from ./ in the path
		#replace / by the os separator in case we need // or \\
		rootPath = os.getcwd() + proj.strip('.')
		rootPath = rootPath.replace("/",os.sep)
		#need to get root folder of path by stripping the filename from path
		rootPath = rootPath.split(os.sep)[0:-1]
		rootPath = os.sep.join(rootPath)

		#save root directory where python is executed from
		currDir = os.getcwd()
		#change working directory to sln file
		os.chdir(rootPath)

		#strip extension
		filename = proj.split(os.sep)[-1]
		
		#hard code the configurations for Aura for now as it's not implemented for Vulkan runtime
		if filename == "VisibilityBuffer.sln":
			if "DebugDx11" in configurations : configurations.remove("DebugDx11")
			if "ReleaseDx11" in configurations : configurations.remove("ReleaseDx11")
		elif filename == "HLSLParser.sln":
			configurations = ["Debug", "Release"]
			
		#for conf in configurations:
		if ".sln" in filename:
			for conf in configurations:
				command = [msBuildPath ,filename,"/p:Configuration="+conf,"/p:Platform=" + platform,"/m","/p:BuildInParallel=true","/nr:false",msbuildVerbosityClp,msbuildVerbosity,"/t:Build"]
				retCode = ExecuteBuild(command, filename,conf, platform)
		else:
			command = [msBuildPath ,filename,"/p:Platform=" + platform,"/m", "/nr:false",msbuildVerbosityClp,msbuildVerbosity,"/t:Build"]
			retCode = ExecuteBuild(command, filename,"All Configurations", platform)
		
		if retCode != 0:
			errorOccured = True
				
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0

def AddSanitizers(vcxprojects, ubsan, asan, isSwitch = False, isOrbis = False):
	sanitizer_flags = ""

	if ubsan and asan:
		sanitizer_flags = "<AdditionalOptions>-fsanitize=undefined -fsanitize=address %(AdditionalOptions)</AdditionalOptions>\n"
	elif ubsan:
		if isSwitch: # add an extra flag to make UB errors unrecoverable, which halts execution.
			sanitizer_flags = "<AdditionalOptions>-fsanitize=undefined -fno-sanitize-recover=undefined %(AdditionalOptions)</AdditionalOptions>\n"
		else:
			sanitizer_flags = "<AdditionalOptions>-fsanitize=undefined %(AdditionalOptions)</AdditionalOptions>\n"
	else:
		sanitizer_flags = "<AdditionalOptions>-fsanitize=address %(AdditionalOptions)</AdditionalOptions>\n"

	for vcxproj in vcxprojects:
		if "Libraries" in vcxproj: 
			continue

		proj = vcxproj.split("\\")

		if proj[-1][:-8] not in sanitizer_UTs:
			continue

		output = ""
		f = open(vcxproj)
		for line in f:
			output += line
			line = line.strip()
			if line == "<ClCompile>":
				output += sanitizer_flags
			elif isSwitch and line == "<Link>":
				output += sanitizer_flags
		f.close()

		f = open(vcxproj, "w")
		f.write(output)
		f.close()

def RemoveSanitizers(arguments):
	vcxprojects = None

	if arguments.orbis:
		vcxprojects = GetFilesPathByExtension("./PS4/Examples_3/", "vcxproj", False)
	elif arguments.prospero:
		vcxprojects = GetFilesPathByExtension("./Prospero/Examples_3/", "vcxproj", False)
	elif arguments.switchNX:
		vcxprojects = GetFilesPathByExtension("./Switch/Examples_3/", "vcxproj", False)

	if vcxprojects:
		for vcxproj in vcxprojects:
			if "Libraries" in vcxproj: 
				continue

			proj = vcxproj.split("\\")

			if proj[-1][:-8] not in sanitizer_UTs:
				continue

			output = ""
			f = open(vcxproj)
			for line in f:
				if line.startswith("<AdditionalOptions>-fsanitize"):
					continue
				output += line
			f.close()

			f = open(vcxproj, "w")
			f.write(output)
			f.close()

def BuildProsperoProjects(skipDebug, skipRelease, printMSBuild, ubsan, asan):
	sanitizer_pass = ubsan or asan
	errorOccured = False
	msBuildPath = FindMSBuild17()

	configurations = ["Debug", "Release"]
	platform = "Prospero"
	
	if skipDebug:
		configurations.remove("Debug")
		
	if skipRelease:
		configurations.remove("Release")

	#xboxConfigurations = ["Debug","Release"]

	if msBuildPath == "":
		print("Could not find MSBuild 17, Is Visual Studio 17 installed ?")
		sys.exit(-1)

	projects = GetFilesPathByExtension("./Jenkins/","buildproj",False)
	
	#if MSBuild tasks were not found then parse all projects
	if len(projects) == 0:
		projects = GetFilesPathByExtension("./Prospero/Examples_3/","sln",False)

	fileList = []
	msbuildVerbosity = "/verbosity:minimal"
	msbuildVerbosityClp = "/clp:ErrorsOnly;WarningsOnly;Summary"
	
	if printMSBuild:
		msbuildVerbosity = "/verbosity:normal"
		msbuildVerbosityClp = "/clp:Summary;PerformanceSummary"

	if sanitizer_pass:
		for proj in projects:
			if "Sanitizer_Pass_Prospero" in proj:
				fileList.append(proj)
				break
		
		vcxprojects = GetFilesPathByExtension("./Prospero/Examples_3/","vcxproj",False)
		AddSanitizers(vcxprojects, ubsan, asan)
		solutions = GetFilesPathByExtension("./Prospero/Examples_3/Unit_Tests","sln",False)
		FilterSolutionProjects(solutions[0])
	else:
		for proj in projects:
			if "Prospero" in proj and not "Sanitizer_Pass" in proj:
				fileList.append(proj)
				
	for proj in fileList:
		#get current path for sln file
		#strip the . from ./ in the path
		#replace / by the os separator in case we need // or \\
		rootPath = os.getcwd() + proj.strip('.')
		rootPath = rootPath.replace("/",os.sep)
		#need to get root folder of path by stripping the filename from path
		rootPath = rootPath.split(os.sep)[0:-1]
		rootPath = os.sep.join(rootPath)

		#save root directory where python is executed from
		currDir = os.getcwd()
		#change working directory to sln file
		os.chdir(rootPath)

		#strip extension
		filename = proj.split(os.sep)[-1]
		
		if filename == "VisibilityBuffer.sln":
			if "DebugDx11" in configurations : configurations.remove("DebugDx11")
			if "ReleaseDx11" in configurations : configurations.remove("ReleaseDx11")
		elif filename == "HLSLParser.sln":
			configurations = ["Debug", "Release"]
			
		#for conf in configurations:
		if ".sln" in filename:
			for conf in configurations:
				command = [msBuildPath ,filename,"/p:Configuration="+conf,"/p:Platform=" + platform,"/m","/p:BuildInParallel=false","/nr:false",msbuildVerbosityClp,msbuildVerbosity,"/t:Build"]
				retCode = ExecuteBuild(command, filename,conf, platform)
		else:
			command = [msBuildPath ,filename,"/p:Platform=" + platform,"/m", "/nr:false",msbuildVerbosityClp,msbuildVerbosity,"/t:Build"]
			retCode = ExecuteBuild(command, filename,"All Configurations", platform)
		
		if retCode != 0:
			errorOccured = True
				
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0

#check memory leak file using regex
#searchs for %d memory leaks found:
#if it finds that string it will print the contents of the leaks file
#then returns True
#otherwise if no leaks found or the file doesn't exist return false 
def FindMemoryLeaks(memLeakLog):
	if not os.path.exists(memLeakLog):
		print("Could not find the memory leak log file.")
		print(memLeakLog)
		return False
	
	lines = []
	with open(memLeakLog,'rt') as f:
		lines = f.readlines()
	lineContents = "".join(lines)
	# try to match the number of leaks. if this doesn't match a valid ressult then no leaks were detected.
	leaksMatch = re.findall(r"^(\d+)\s+memory leak+(s)? found:$", lineContents, re.MULTILINE | re.IGNORECASE)
	if len(leaksMatch) > 0:
		# find all text with ----- that separates the different memleaks sections
		iteratorMatches = re.finditer(r"(----*.*?)$", lineContents, re.MULTILINE | re.IGNORECASE)
		iterList = list(iteratorMatches)
		print("Detected the following leaks")
		#try to print only section containing the source of leaks
		if len(iterList) > 3:
			print (lineContents[iterList[2].start(0):iterList[3].end(0)])
		else:
		#if failed to get correct section then print whole file
			print (lineContents)
		return True
	
	print("No Leaks detected.")
	return False

def RecordBenchmarkFilePath(platform, appName, pathToSearch ):
	profiles = GetFilesPathByExtension(pathToSearch,"profile", False, 1)
	targetProfiles = []
	for profile in profiles:
		if appName in profile:
			targetProfiles.append(profile)
			targetProfiles.sort(reverse=True)

	if len(targetProfiles) == 0:
		return None

	lines = []
	with open(targetProfiles[0],'rt') as f:
		lines = f.readlines()
	lineContents = "".join(lines)
	if platform not in benchmarkFiles:
		benchmarkFiles[platform] = []
	benchmarkFiles[platform].append( {"Application":appName, "Data":lineContents} )
	return lineContents

def CheckDiffContents(relevantChangeStr, targetBranch="origin/master"):
	output = ExecuteCommandWOutput(["git", "diff", targetBranch]) + ['\n'] + ExecuteCommandWOutput(["git", "submodule", "foreach", "--recursive", "git", "diff", targetBranch])

	retValue = 0
	currentDiff = ""
	for line in output:
		try:
			utfLine = line.decode('utf-8')

			if utfLine.startswith('diff'):
				currentDiff = utfLine

			if utfLine.startswith('+') or utfLine.startswith('-'):
				#filter out diffs on the actual jenkinsfile
				if "JenkinsPipeline_Gitlab" in currentDiff:
					continue
				if relevantChangeStr in utfLine:
					print("{} is found in diff against {}".format(relevantChangeStr, targetBranch))
					print("Relevant {} line: \n {}".format(currentDiff, utfLine))
					retValue = 1

		except:
			continue

	return retValue
	
def CleanupHandler(signum, frame):
	global setDefines
	global setMemTracker
	global sanitizers_cleanup
	global arguments
	print("Bye.")

	#need to change to rootpath otherwise
	#os won't find the files to modify
	os.chdir(sys.path[0])

	if setDefines == True or setMemTracker == True:
		#Remove all defines for automated testing
		print("Removing defines that got added for automated testing")
		RemoveTestingPreProcessor()

	if sanitizers_cleanup:
		RemoveSanitizers(arguments)
		RemoveSanitizerSolution(arguments)
	
	exit(1)

def SanitizersSupported(arguments, darwin):
	if arguments.asan and arguments.tsan:
		print("Error: asan and tsan cannot be enabled at the same time.")
		return False

	if arguments.orbis and arguments.tsan:
		print("Error: tsan not supported on PS4.")
		return False

	if arguments.prospero and arguments.tsan:
		print("Error: tsan not supported on PS5.")
		return False

	if darwin and arguments.tsan and not arguments.skipiosbuild:
		print("Error: tsan not supported on iOS.")
		return False

	if arguments.switchNX:
		if arguments.asan:
			print("Error: asan not supported on Switch.")
			return False

		if arguments.tsan:
			print("Error: tsan not supported on Switch.")
			return False
	
	return True	

#create global variable for interrupt handler
setDefines = False
setMemTracker = False
sanitizers_cleanup = False
arguments = None

def MainLogic():
	global setDefines
	global setMemTracker
	global maxIdleTime
	global sanitizers_cleanup
	global arguments
	#TODO: Maybe use simpler library for args
	parser = argparse.ArgumentParser(description='Process the Forge builds')
	parser.add_argument('--clean', action="store_true", help='If enabled, will delete all unversioned and untracked files/folder excluding the Art folder.')
	parser.add_argument('--prebuild', action="store_true", help='If enabled, will run PRE_BUILD if assets do not exist.')
	parser.add_argument('--forceprebuild', action="store_true", help='If enabled, will call PRE_BUILD even if assets exist.')
	parser.add_argument('--xbox', action="store_true", help='Enable xbox building')
	parser.add_argument('--xboxonly', action="store_true", help='Enable xbox building')
	parser.add_argument('--switchNX', action="store_true", help='Enable Switch building')
	parser.add_argument('--orbis', action="store_true", default=False, help='Enable orbis building')
	parser.add_argument('--prospero', action="store_true", default=False, help='Enable prospero building')
	parser.add_argument("--skipiosbuild", action="store_true", default=False, help='Disable iOS building')
	parser.add_argument("--skipmacosbuild", action="store_true", default=False, help='Disable Macos building')
	parser.add_argument("--skipioscodesigning", action="store_true", default=False, help='Disable iOS code signing during build stage')
	parser.add_argument('--testing', action="store_true", help='Test the apps on current platform')
	parser.add_argument('--ios', action="store_true", help='Needs --testing. Enable iOS testing')
	parser.add_argument("--iosid", type=str, default="-1", help='Use a specific ios device. Id taken from ios-deploy --detect.')
	parser.add_argument('--macos', action="store_true", help='Needs --testing. Enable macOS testing')
	parser.add_argument('--android', action="store_true", help='Enable android building')
	parser.add_argument('--quest', action="store_true", help='Enable Quest building')
	parser.add_argument('--defines', action="store_true", help='Enables pre processor defines for automated testing.')
	parser.add_argument('--gpuselection', action="store_true", help='Enables pre processor defines for using active gpu determined from activeTestingGpu.cfg.')
	parser.add_argument('--timeout',type=int, default="45", help='Specify timeout, in seconds, before app is killed when testing. Default value is 45 seconds.')
	parser.add_argument('--skipdebugbuild', action="store_true", help='If enabled, will skip Debug build.')
	parser.add_argument('--skipreleasebuild', action="store_true", help='If enabled, will skip Release build.')
	parser.add_argument('--printbuildoutput', action="store_true", help='If enabled, will print output of project builds.')
	parser.add_argument('--skipaura', action="store_true", help='If enabled, will skip building aura.')
	parser.add_argument('--skipdx11', action="store_true", help='If enabled, will skip building DX11.')
	parser.add_argument('--xcodederiveddatapath', type=str, default='Null', help = 'Uses a specific path relative to root of project for derived data. If null then it uses the default location for derived data')
	parser.add_argument('--preserveworkingdir', action="store_true", help='If enabled, will keep working directory as is instead of changing it to path of PyBuild.')
	parser.add_argument('--defineonly', action="store_true", help='If enabled, will set defines and exit.')
	parser.add_argument('--buildshaders', default=False, action="store_true", help='If enabled, will set defines and exit.')
	parser.add_argument("--checkdiffcontents", type=str, default="-1", help='Check for given string in diff of MR. If found then a comment will be posted on Gitlab.')
	parser.add_argument("--ubsan", action="store_true", help='Enable the Undefined Behavior sanitizer. Can be paired with one of [--asan, --tsan] but not both.')
	parser.add_argument("--asan", action="store_true", help='Enable the Address sanitizer. Can be paired with --ubsan only.')
	parser.add_argument("--tsan", action="store_true", help='Enable the Thread sanitizer. Can be paired with --ubsan only.')
	parser.add_argument('--benchmark',type=int, default="-1", help='Specify number of frames to benchmark. Default value is -1 which means no benchmarking.')
	parser.add_argument("--assetpipeline", action="store_true", default=False, help='Build the asset pipeline exclusively, on macos, windows and manjaro')

	AddConfigFileArgs(parser)

	#TODO: remove the test in parse_args
	arguments = parser.parse_args()
	systemOS = platform.system()

	# filter out arguments
	sanitizer_pass = arguments.ubsan or arguments.asan or arguments.tsan
	sanitizers_cleanup = sanitizer_pass and (arguments.orbis or arguments.prospero or arguments.switchNX) and not arguments.testing
	
	#early out for unsupported options
	if arguments.assetpipeline:
		def errorMessageUnsupported(message):
			print(message)
			sys.exit(-1)
		if arguments.xbox:
			errorMessageUnsupported("--assetpipeline command invalid with --xbox ")
		if arguments.switchNX:
			errorMessageUnsupported("--assetpipeline command invalid with --switchNX ")
		if arguments.orbis:
			errorMessageUnsupported("--assetpipeline command invalid with --orbis ")
		if arguments.prospero:
			errorMessageUnsupported("--assetpipeline command invalid with --prospero ")
		if arguments.android:
			errorMessageUnsupported("--assetpipeline command invalid with --android ")
		if arguments.quest:
			errorMessageUnsupported("--assetpipeline command invalid with --quest ")
		if sanitizer_pass:
			errorMessageUnsupported("--assetpipeline command invalid with sanitizer options ")

	if sanitizer_pass and not SanitizersSupported(arguments, systemOS == "Darwin"):
		sys.exit(-1) # fail Jenkins if sanitizers not supported
	
	if arguments.buildshaders:
		_filedir = os.path.dirname(os.path.abspath(__file__))
		process = subprocess.Popen('python -u ' + os.path.join(_filedir, 'PyBuildShaders.py'))
		process.communicate()
		if process.returncode != 0:
			return process.returncode
		return

	if not arguments.checkdiffcontents == "-1" and not arguments.checkdiffcontents=="":
		os.chdir(sys.path[0])
		exit(CheckDiffContents(arguments.checkdiffcontents))

	
	#if we want to run based on active gpu config
	#we need defines macros
	if arguments.gpuselection:
		arguments.defines = True
		
	#add cleanup handler in case app gets interrupted
	#keyboard interrupt
	#removing defines
	signal.signal(signal.SIGINT, CleanupHandler)

	#change path to scripts location
	if not arguments.preserveworkingdir:
		os.chdir(sys.path[0])
	
	returnCode = 0
		
	
	if (arguments.xbox is not True and arguments.xboxonly is not True) or "GXDKLatest" not in os.environ:
		arguments.xbox = False
		arguments.xboxonly = False
	
	#if we doing xbox only make sure the --xbox argument is enabled.
	if arguments.xboxonly:
		arguments.xbox = True 

	setDefines = arguments.defines
	SaveConfigFileArgs(arguments)
	if setDefines == True or config_file_options:
		AddTestingPreProcessor(arguments.gpuselection)

	# workaround to just set defines and avoid the signal handler to revert those changes
	if arguments.defineonly:
		signal.signal(signal.SIGINT, signal.default_int_handler)
		sys.exit(0)

	#PRE_BUILD step
	#if only the prebuild argument is provided but Art folder exists then PRE_BUILd isn't run
	#if only the forceprebuild argument is provided PRE_BUILD runs even if art folder exists
	#this is good for jenkins as we don't want to call PRE_BUILD if art asset exists
	if arguments.prebuild ==  True or arguments.forceprebuild == True:
		if os.path.isdir("./Art") == False or arguments.forceprebuild == True:
			if systemOS == "Windows":
				ExecuteCommand(["PRE_BUILD.bat"], sys.stdout)
			else:
				ExecuteCommand(["sh","PRE_BUILD.command"], sys.stdout)
	
	#exclusing asset pipeline testing
	if arguments.assetpipeline and  arguments.testing:
		returnCode = TestAssetPipeline()		
	elif arguments.testing:
		platformName = "MacOS"
		maxIdleTime = max(arguments.timeout,1)
		assetPipelineReturnCode = 0
		#Build for Mac OS (Darwin system)
		if systemOS == "Darwin":
			if not sanitizer_pass:
				assetPipelineReturnCode = TestAssetPipeline()
			returnCode = TestXcodeProjects(arguments.ios, arguments.macos, arguments.iosid, arguments.benchmark, sanitizer_pass)
		elif systemOS == "Windows":
			if arguments.orbis == True:
				platformName = "Orbis"
				returnCode = TestOrbisProjects(arguments.benchmark, sanitizer_pass)
			elif arguments.prospero == True:
				platformName = "Prospero"
				returnCode = TestProsperoProjects(arguments.benchmark, sanitizer_pass)
			elif arguments.xbox == True:
				platformName = "Xbox"
				returnCode = TestXboxProjects(arguments.benchmark)
			elif arguments.switchNX == True:
				platformName = "Switch"
				returnCode = TestNintendoSwitchProjects(arguments.benchmark, sanitizer_pass)
			elif arguments.android == True:
				platformName = "Android"
				returnCode = TestAndroidProjects(arguments.benchmark, False)
			elif arguments.quest == True:
				platformName = "Quest"
				returnCode = TestAndroidProjects(arguments.benchmark, True)
			else:
				platformName = "Windows"
				assetPipelineReturnCode = TestAssetPipeline()
				returnCode = TestWindowsProjects(arguments.gpuselection, arguments.benchmark)
		elif systemOS.lower() == "linux" or systemOS.lower() == "linux2":
			platformName = "Manjaro"
			assetPipelineReturnCode = TestAssetPipeline()
			returnCode = TestLinuxProjects(arguments.benchmark)
		
		#make sure both tests pass
		if assetPipelineReturnCode != 0:
			returnCode = assetPipelineReturnCode

		if arguments.benchmark > 0:
			filename = f"{platformName}Benchmarks.results"
			if os.path.exists(filename):
				os.remove(filename)			
			with open(filename, 'w',  encoding='utf-8') as outfile:
				json.dump(benchmarkFiles, outfile, indent=4)
	else:
		#Clean before Building removing everything but the art folder
		if arguments.clean == True:
			print("Cleaning the repo")
			os.environ["GIT_ASK_YESNO"] = "false"
			ExecuteCommand(["git", "clean" , "--exclude=Art","--exclude=/**/OpenSource/*", "--exclude=/**/Android-arm64-v8a/**", "-fdx"],sys.stdout)
			ExecuteCommand(["git", "submodule", "foreach", "--recursive","git clean -fdfx"], sys.stdout)
		#Build for Mac OS (Darwin system)
		if systemOS== "Darwin":
			returnCode = BuildXcodeProjects(arguments)
		elif systemOS == "Windows":
			if arguments.quest:
				returnCode = BuildAndroidProjects(arguments.skipdebugbuild, arguments.skipreleasebuild, arguments.printbuildoutput, True)
			elif arguments.android:
				returnCode = BuildAndroidProjects(arguments.skipdebugbuild, arguments.skipreleasebuild, arguments.printbuildoutput, False)
			elif arguments.orbis:
				returnCode = BuildOrbisProjects(arguments.skipdebugbuild, arguments.skipreleasebuild, arguments.printbuildoutput, arguments.ubsan, arguments.asan)
			elif arguments.prospero:
				returnCode = BuildProsperoProjects(arguments.skipdebugbuild, arguments.skipreleasebuild, arguments.printbuildoutput, arguments.ubsan, arguments.asan)
			else:
				returnCode = BuildWindowsProjects(arguments)
		elif systemOS.lower() == "linux" or systemOS.lower() == "linux2":
			returnCode = BuildLinuxProjects(arguments)

	PrintResults()
	
	#Clean up 
	if arguments.defines:
		print("Removing defines that got added for automated testing")
		RemoveTestingPreProcessor()

	if sanitizers_cleanup:
		RemoveSanitizers(arguments)
		RemoveSanitizerSolution(arguments)
	
	#return for jenkins
	sys.exit(returnCode)

if __name__ == "__main__":
	MainLogic()
