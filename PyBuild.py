#!/usr/bin/python
# Copyright (c) 2018 Confetti Interactive Inc.
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

successfulBuilds = [] #holds all successfull builds
failedBuilds = [] #holds all failed builds

successfulTests = [] #holds all successfull tests
failedTests = [] #holds all failed tests

maxIdleTime = 45  #10 seconds of max idle time with cpu usage null

def PrintResults():
	if len(successfulBuilds) > 0:
		print ("Successful Builds list:")
		for build in successfulBuilds:
			print(build['name'], build['conf'], build['platform'])
	
	print ("")
	if len(failedBuilds) > 0:
		print ("Failed Builds list:")
		for build in failedBuilds:
			print(build['name'], build['conf'], build['platform'])
			
	if len(successfulTests) > 0:
		print ("Successful tests list:")
		for test in successfulTests:
			if test['gpu'] == "":
				print(test['name'])
			else:
				print(test['name'], test['gpu'])
	
	print ("")
	if len(failedTests) > 0:
		print ("Failed Tests list:")
		for test in failedTests:
			if test['gpu'] == "":
				print(test['name'], test['reason'])
			else:
				print(test['name'], test['gpu'], test['reason'])

def FindMSBuild17():
	ls_output = ""
	msbuildPath = ""
	try:
		#open vswhere and parse the output
		proc = subprocess.Popen(["C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe","-version","14","-requires" ,"Microsoft.Component.MSBuild"], stdout=subprocess.PIPE,stderr = subprocess.STDOUT,
    encoding='utf8')

		ls_output = proc.communicate()[0]
		#check if vswhere opened correctly
		if proc.returncode != 0:
			print("Could not find vswhere")
		else:			
			#parse vswhere output
			for line in ls_output.split("\n"):
				#if installation path was found then get path from it and add the MSBuild location
				if "installationPath:" in line:
					msbuildPath = line.split("installationPath:")[1].strip() + "/MSBuild/15.0/Bin/MSBuild.exe"
					break
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

def AddTestingPreProcessor(enabledGpuSelection):	
	if setDefines == True:
		print("Adding Automated testing preprocessor defines")
		macro = "#define AUTOMATED_TESTING 1"
		if enabledGpuSelection:
			macro += "\n#define ACTIVE_TESTING_GPU 1"
		AddPreprocessorToFile("Common_3/OS/Interfaces/IOperatingSystem.h", macro + "\n", "#pragma")
	
	if setMemTracker == True:
		print("Adding Memory tracking preprocessor defines")
		macro = "#define USE_MEMORY_TRACKING 1"
		AddPreprocessorToFile("Common_3/OS/Interfaces/IMemory.h", macro + "\n", "#pragma")
		AddPreprocessorToFile("Common_3/OS/MemoryTracking/MemoryTracking.cpp", macro, "#if")



def RemoveTestingPreProcessor():
	testingDefines = ["#define AUTOMATED_TESTING", "#define ACTIVE_TESTING_GPU"]
	memTrackingDefines = ["#define USE_MEMORY_TRACKING"]
	if setDefines == True:
		print("Removing automated testing preprocessor defines")
		RemovePreprocessorFromFile("Common_3/OS/Interfaces/IOperatingSystem.h", testingDefines)
	if setMemTracker == True:
		print("Removing memory tracking preprocessor defines")
		RemovePreprocessorFromFile("Common_3/OS/Interfaces/IMemory.h", memTrackingDefines)
		RemovePreprocessorFromFile("Common_3/OS/MemoryTracking/MemoryTracking.cpp", memTrackingDefines)
	

def ExecuteTimedCommand(cmdList,outStream=subprocess.PIPE):
	try:		
		print("Executing command: " + ' '.join(cmdList))

		#open subprocess without piping output
		# otherwise process blocks until we call communicate or wait
		proc = subprocess.Popen(cmdList, stdout=None, stderr=None)		
		
		#get start time of process
		startTime = time.time()
		
		"""Wait for a process to finish, or raise exception after timeout"""
		end = startTime + maxIdleTime
		interval = max(maxIdleTime / 1000.0, 0.005)
		
		#loop until time expires or process exits alone
		while True:
			result = proc.poll()
			if result is not None:
				break
			if time.time() >= end:
				print ("Killed Application. Something went wrong with the app, it was idle for too long.")
				proc.kill()
				proc.wait()
				break
			time.sleep(interval)
			
		rc = proc.returncode
		if rc != 0:			
			print("Process was killed or has crashed.")
			return rc

	except Exception as ex:
		print("-------------------------------------")
		print("Failed executing command: " + ' '.join(cmdList))
		print(ex)
		print("-------------------------------------")
		return -1  #error return code
	
	print("Success")
	return 0 #success error code

def ExecuteCommandWOutput(cmdList):
	try:
		print("")
		print("Executing command: " + ' '.join(cmdList))
		print("") 
		ls_lines = subprocess.check_output(cmdList).splitlines()
		return ls_lines
	except Exception as ex:
		print("-------------------------------------")
		print("Failed executing command: " + ' '.join(cmdList))
		print(ex)
		print("-------------------------------------")
		return ""  #error return code
	
	return "" #success error code

def ExecuteCommand(cmdList,outStream):
	try:
		print("")
		print("Executing command: " + ' '.join(cmdList))
		print("") 
		proc = subprocess.Popen(cmdList, stdout=outStream)
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
	
def ExecuteBuildAndroid(cmdList, fileName, configuration, platform):
	returnCode = ExecuteCommand(cmdList, sys.stdout)
	
	if returnCode != 0:
		print("FAILED BUILDING ", fileName, configuration)
		failedBuilds.append({'name':fileName,'conf':configuration, 'platform':platform})
	else:
		successfulBuilds.append({'name':fileName,'conf':configuration, 'platform':platform})
	
	return returnCode
	
def ExecuteBuild(cmdList, fileName, configuration, platform):
	returnCode = ExecuteCommand(cmdList, sys.stdout)
	
	if returnCode != 0:
		print("FAILED BUILDING ", fileName, configuration)
		failedBuilds.append({'name':fileName,'conf':configuration, 'platform':platform})
	else:
		successfulBuilds.append({'name':fileName,'conf':configuration, 'platform':platform})
	
	return returnCode

def ExecuteTest(cmdList, fileName, regularCommand, gpuLine = ""):
	if regularCommand:
		returnCode = ExecuteCommand(cmdList, None)
	else:
		returnCode = ExecuteTimedCommand(cmdList,None)
	
	if returnCode != 0:
		print("FAILED TESTING ", fileName)
		print("Return code: ", returnCode)
		failedTests.append({'name':fileName, 'gpu':gpuLine, 'reason':"Runtime Failure"})
	else:
		successfulTests.append({'name':fileName, 'gpu':gpuLine})
		
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
			for filename in fnmatch.filter(files, "*."+extension):
				filesPathList.append(os.path.join(root,filename))

		if maxDepth == 0:
			break
		maxDepth = maxDepth - 1

	return filesPathList

def GetMemLeakFile(exeFilePath):
	print (exeFilePath)
	exeFileWithoutExt = exeFilePath.split('.')[0]
	memLeakFile = exeFileWithoutExt + ".memleaks"
	return memLeakFile

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
#<vendor_id>, <model_id>, <sli_mode>
0x10de; 0x1b81; false; Nvidia Geforce GTX 1070;
0x10de; 0x1402; false; Nvidia Geforce GTX 950;
0x1002; 0x687f; false; AMD Vega;
0x1002; 0x67df; false; AMD Radeon RX 480;
"""
originalActiveGpuConfigLines = []
def selectActiveGpuConfig(forgeDir, projRootFolder, projectName, runIndex):
	global activeGpusConfiguration
	global originalActiveGpuConfigLines
	#remove file extension from project name
	projectName = os.path.splitext(projectName)[0]
	
	#need to have
	if "Aura" in projectName or "Visibility" in projectName:
		filename = "/Examples_3/"+projRootFolder+"/src/GPUCfg/activeTestingGpu.cfg"
	else:
		filename = "/Examples_3/"+projRootFolder+"/src/"+projectName+"/GPUCfg/activeTestingGpu.cfg"
	
	filename = forgeDir + filename
	
	#create active gpu config if it doesn't exist
	#this is only valid for our internal testing rig
	if not os.path.exists(filename):
		with open(filename, 'w+') as f:
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

def TestXcodeProjects(iosTesting, macOSTesting, iosDeviceId):
	errorOccured = False

	projects = GetFilesPathByExtension("./Examples_3/","app", True)
	iosApps = []
	osxApps = []
	appsToTest = []
	
	for proj in projects:
		if "Release" in proj:
			#we don't want to build Xbox one solutions when building PC
			if "_iOS" in proj:
				iosApps.append(proj)
			else :
				osxApps.append(proj)

	if iosTesting:
		appsToTest = iosApps
		
	if macOSTesting:
		appsToTest.extend(osxApps)

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
		# get the memory leak file path
		memleakFile = GetMemLeakFile(filename)

		if "_iOS" in filename:
			#if specific ios id was passed then run for that device
			#otherwise run on first device available
			#print iosDeviceId
			if iosDeviceId == "-1" or iosDeviceId == "": 
				command = ["ios-deploy","--uninstall","-b",filename + ".app","-I"]
			else:
				command = ["ios-deploy","--uninstall","-b",filename + ".app","-I", "--id", iosDeviceId]
			
			retCode = ExecuteTest(command, filename, True)
			if retCode == 0:
				bundleID = GetBundleIDFromIOSApp(filename + ".app")
				if bundleID != "":
					command = ["ios-deploy","--bundle_id",bundleID,"--download=/Documents/"+memleakFile,"--to","./"]
					memleakDownloaded = ExecuteCommand(command, sys.stdout)
					if memleakDownloaded == 0:
						print("Memleaks file downloaded for:" + bundleID)
						leaksDetected = FindMemoryLeaks("Documents/"+ memleakFile)
					else:
						print("[Error] Memleaks file could not be downloaded for:" + bundleID)
				else:
					print("[Error] Bundle ID NOT found:" + bundleID)
				
		else:
			command = ["./" + filename + ".app/Contents/MacOS/" + filename]
			retCode = ExecuteTest(command, filename, False)
			leaksDetected = FindMemoryLeaks(memleakFile)
		
		if retCode == 0 and leaksDetected == True:
			lastSuccess = successfulTests.pop()
			failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Memory Leaks"})
			errorOccured = True
			
		if retCode != 0:
			errorOccured = True

		#set working dir to initial
		os.chdir(currDir)

	if errorOccured:
		return -1
	else:
		return 0

def GetXcodeSchemes(targetPath, getMacOS, getIOS):
	command = ["xcodebuild", "-list"]
	if ".xcworkspace" in targetPath:
		command.append("-workspace")
		command.append(targetPath)
	elif ".xcodeproj" in targetPath:
		command.append("-project")
		command.append(targetPath)

	schemesList = ExecuteCommandWOutput(command)
	parsedSchemes = []
	filteredSchemes = []
	#try to detect any error and return null if detected
	#also detect any informational line (such as List of Schemes:)
	#As fas as I've seen the information lines all have : at the end
	schemesStartFound = False
	for line in schemesList:
		if b"error" in line:
			print("Error retrieving the schemes from: " + targetPath)
			print(line)
			return []
		if b"Schemes:" in line:
			schemesStartFound = True
			continue
		
		buildAllFound = False
		if schemesStartFound:
			line = line.strip()
			if line.isspace() or not line:
				break
			if b":" in line:
				break
			if b"BuildAll" in line:
				buildAllFound = True
			#add scheme
			parsedSchemes.append(line)

	buildBothOS = getMacOS and getIOS
	for scheme in parsedSchemes:
		
		#current scheme is build all but we not building both platforms
		#filter it out
		if b"BuildAll" in scheme and not buildBothOS:
			continue
		#building both platforms and we found a build all scheme
		#filter all the other schemes out
		if not b"BuildAll" in scheme and buildBothOS and buildAllFound:
			continue
		#filter macos scheme if necessary
		if not getMacOS and "iOS" not in scheme:
			continue
		#filter ios scheme if necessary
		if not getIOS and "iOS" in scheme:
			continue
		
		filteredSchemes.append(scheme)

	return filteredSchemes

#Helper to create xcodebuild command for given scheme, workspace(full path from current working directory) and configuration(Debug, Release)
#can filter out schemes based on what to skip and will return "" in those cases
def CreateXcodeBuildCommand(skipMacos, skipIos, skipIosCodeSigning,path,scheme,configuration, isWorkspace, ddPath, printBuildOutput):
	logLevel = "-quiet"
	if printBuildOutput:
		logLevel = "-hideShellScriptEnvironment"

	if isWorkspace and "BuildAll" in scheme:
		#build all projects in workspace using special BuildAll scheme. enables more parallel builds
		command = ["xcodebuild",logLevel,"-workspace",path,"-configuration",configuration,"build","-scheme","BuildAll", "-parallelizeTargets"]
	elif isWorkspace and scheme != "":
	 	command = ["xcodebuild",logLevel,"-workspace",path,"-configuration",configuration,"build","-parallelizeTargets", "-scheme",scheme]
	elif not isWorkspace:
		#if filtering platforms then we build using schemes
		if scheme != "" and (skipMacos or skipIos):
			command = ["xcodebuild",logLevel,"-project",path,"-configuration",configuration,"build", "-parallelizeTargets", "-scheme", scheme]
		else:
			#otherwise build all targets of projects in parallel
			command = ["xcodebuild",logLevel,"-project",path,"-configuration",configuration,"build","-scheme", scheme, "-parallelizeTargets"]
	else:
		return ""

	#use the -derivedDataPath flag only when custom location is specified by ddPath otherwise use the default location
	if ddPath != 'Null':
		command.append("-derivedDataPath")
		command.append(ddPath)

	if skipIosCodeSigning:
		command.extend([
		"CODE_SIGN_IDENTITY=\"\"",
		"CODE_SIGNING_REQUIRED=\"NO\"",
		"CODE_SIGN_ENTITLEMENTS=\"\"",
		"CODE_SIGNING_ALLOWED=\"NO\""])
	return command
		
def ListDirs(path):
    return [dir for dir in os.listdir(path) if os.path.isdir(os.path.join(path,dir))]



def BuildXcodeProjects(skipMacos, skipIos, skipIosCodeSigning, skipDebugBuild, skipReleaseBuild, printXcodeBuild, derivedDataPath):
	errorOccured = False
	buildConfigurations = ["Debug", "Release"]
	if skipDebugBuild:
		buildConfigurations.remove("Debug")
	if skipReleaseBuild:
		buildConfigurations.remove("Release")

	#since our projects for macos are all under a macos Xcode folder we can search for
	#that specific folder name to gather source folders containing project/workspace for xcode
	#macSourceFolders = FindFolderPathByName("Examples_3/","macOS Xcode", -1)
	xcodeProjects = ["/Examples_3/Visibility_Buffer/macOS Xcode/Visibility_Buffer.xcodeproj", 
				"/Examples_3/Unit_Tests/macOS Xcode/Unit_Tests.xcworkspace"]

	#if derivedDataPath is not specified then use the default location
	if derivedDataPath == 'Null':
		DDpath = derivedDataPath
	else:
		#Custom Derived Data location relative to root of project
		DDpath = os.path.join(os.getcwd(), derivedDataPath)
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

		#get and filter xcode schemes
		schemesList = GetXcodeSchemes(filenameWExt,not skipMacos, not skipIos)
		#if building both iOS and macOS then build them in parallel
		#by building whole project instead of schemes
		if "xcodeproj" in extension and not (skipMacos or skipIos):
			#no need for any schemes we will build whole project
			schemesList = [filename]
		else:
			for scheme in schemesList:
				if b"BuildAll" in scheme:
					#remove all other schemes
					schemesList = ["BuildAll"]
					break
		for conf in buildConfigurations:
			#will build all targets for vien project
			#canot remove ios / macos for now
			
			for scheme in schemesList:
				command = CreateXcodeBuildCommand(skipMacos, skipIos, skipIosCodeSigning, filenameWExt,scheme,conf, "xcworkspace" in extension, DDpath, printXcodeBuild)
				platformName = "macOS/iOS"
				if "iOS" in scheme:
					platformName = "iOS"
				elif "BuildAll" not in scheme:
					platformName = "macOS"

				#just switch otu filename and scheme in case we are building BuildAll
				#display the project name intead.
				if "BuildAll" in scheme:
					sucess = ExecuteBuild(command, filename, conf, platformName)
				else:
					sucess = ExecuteBuild(command, filename + "/" + scheme, conf, platformName)

				if sucess != 0:
					errorOccured = True
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0

#this needs the vulkan environment variables set up correctly
#if they are not in ~/.profile then they need to be set up for every subprocess
#If it is in ~/.profile then it needs to be maintaned by updating the version number in ~/.profile.
def BuildLinuxProjects():
	errorOccured = False
	
	projsToBuild = GetFilesPathByExtension("./Examples_3/","workspace", False)
	for projectPath in projsToBuild:
		#get working directory (excluding the workspace in path)
		rootPath = os.sep.join(projectPath.split(os.sep)[0:-1])
		#save current work dir
		currDir = os.getcwd()
		#change dir to workspace location
		os.chdir(rootPath)
		configurations = ["Debug", "Release"]
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
					if child.attrib["Name"] != "OSBase" and child.attrib["Name"] != "EASTL" and child.attrib["Name"] != "OS" and child.attrib["Name"] != "Renderer" and  child.attrib["Name"] != "SpirVTools" and child.attrib["Name"] != "PaniniProjection" and child.attrib["Name"] != "gainput" and child.attrib["Name"] != "ozz_base" and child.attrib["Name"] != "ozz_animation" and child.attrib["Name"] != "Assimp" and child.attrib["Name"] != "zlib" and child.attrib["Name"] != "LuaManager" and child.attrib["Name"] != "AssetPipeline" and child.attrib["Name"] != "AssetPipelineCmd" and child.attrib["Name"] != "ozz_animation_offline":
						ubuntuProjects.append(child.attrib["Name"])
			
			for proj in ubuntuProjects:
				command = ["codelite-make","-w",filename,"-p", proj,"-c",conf]
				#sucess = ExecuteBuild(command, filename+"/"+proj,conf, "Ubuntu")
				sucess = ExecuteCommand(command, sys.stdout)
				
				if sucess != 0:
					errorOccured = True
				
				command = ["make", "-s"]
				sucess = ExecuteBuild(command, filename+"/"+proj,conf, "Ubuntu")
				
				if sucess != 0:
					errorOccured = True

		#set working dir to initial
		os.chdir(currDir)
	
	if errorOccured == True:
		return -1
	return 0


#this needs the vulkan environment variables set up correctly
#if they are not in ~/.profile then they need to be set up for every subprocess
#If it is in ~/.profile then it needs to be maintaned by updating the version number in ~/.profile.
def TestLinuxProjects():
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
			#create command for xcodebuild
			filename = projectPath.split(os.sep)[-1].split(os.extsep)[0]
			#filename = projectPath.split(os.sep)[-1]
			
			#need to parse xml configuration to get every project
			xmlTree = ET.parse("./"+filename + ".workspace")
			xmlRoot = xmlTree.getroot()

			ubuntuProjects = []
			for child in xmlRoot:
				if child.tag == "Project":
					if child.attrib["Name"] != "OSBase" and child.attrib["Name"] != "EASTL" and child.attrib["Name"] != "OS" and child.attrib["Name"] != "Renderer" and  child.attrib["Name"] != "SpirVTools" and child.attrib["Name"] != "PaniniProjection" and child.attrib["Name"] != "gainput" and child.attrib["Name"] != "ozz_base" and child.attrib["Name"] != "ozz_animation" and child.attrib["Name"] != "Assimp" and child.attrib["Name"] != "zlib" and child.attrib["Name"] != "LuaManager" and child.attrib["Name"] != "AssetPipeline" and child.attrib["Name"] != "AssetPipelineCmd" and child.attrib["Name"] != "ozz_animation_offline":
						ubuntuProjects.append(child.attrib["Name"])
			
			for proj in ubuntuProjects:
				leaksDetected = False	
				exePath = os.path.join(os.getcwd(),proj,conf,proj)
				command = [exePath]
				retCode = ExecuteTest(command, proj ,False)

				if retCode != 0:
					errorOccured = True
				
				memleaksFilename = os.path.join(os.getcwd(),proj,conf,GetMemLeakFile(proj))
				leaksDetected = FindMemoryLeaks(memleaksFilename)
				
				if retCode == 0 and leaksDetected == True:
					lastSuccess = successfulTests.pop()
					failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Memory Leaks"})
					errorOccured = True

		#set working dir to initial
		os.chdir(currDir)
	
	if errorOccured == True:
		return -1
	return 0

def TestWindowsProjects(useActiveGpuConfig):
	errorOccured = False
	
	projects = GetFilesPathByExtension("./Examples_3","exe",False)
	fileList = []

	for proj in projects:
		#we don't want to build Xbox one solutions when building PC
		if "PC Visual Studio 2017" in proj and "Release" in proj:
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

		if "ReleaseVk" in proj:
			filename = "VK_" + filename
		elif "Dx11" in proj:
			filename = "Dx11_" + filename
		else:
			filename = "Dx12_" + filename

		parentFolder = proj.split(os.sep)[1]
		
		# get the memory leak file path
		memleakFile = GetMemLeakFile(origFilename)
		# delete the memory leak file before execute the app
		if os.path.exists(memleakFile):
			os.remove(memleakFile)
		
		if useActiveGpuConfig == True not in parentFolder:
			currentGpuRun = 0
			resultGpu = selectActiveGpuConfig(currDir, parentFolder,origFilename,currentGpuRun)
			while resultGpu['running'] == True:
				retCode = ExecuteTest(command, filename, False, resultGpu['lineMatch'])
				currentGpuRun += 1
				resultGpu = selectActiveGpuConfig(currDir, parentFolder,origFilename,currentGpuRun)
		else:
			retCode = ExecuteTest(command, filename,False)
		
		leaksDetected = FindMemoryLeaks(memleakFile)
		if retCode == 0 and leaksDetected == True:
			lastSuccess = successfulTests.pop()
			failedTests.append({'name':lastSuccess['name'], 'gpu':lastSuccess['gpu'], 'reason':"Memory Leaks"})
			errorOccured = True
		
		if retCode != 0:
			errorOccured = True
				
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0	

def TestXboxProjects():
	errorOccured = False
	FNULL = open(os.devnull, 'w')
	
	#Get console IP
	consoleIP = ""
	xdkDir = os.environ['DURANGOXDK'] + 'bin/'
	command = [xdkDir+'xbconnect', '/QG']
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

	crashdump_path = '\\\\'+consoleIP+"\TitleScratch\LocalDumps"
	
	#Clean all apps
	print ("Cleaning XBox apps and data (this will reboot the console)")
	command = [xdkDir+'xbcleanup', '/U /D /P /C /L']
	output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)
	print ("Done cleaning...")

	#Set console setting to genereate crash dumps
	command = [xdkDir+'xbconfig','CrashDumpType=mini',"/X"+consoleIP]
	output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)


	#Get count of existing crash dumps
	command = ["cmd", "/c", "dir", crashdump_path]
	output	= subprocess.check_output(command)
	output = output.split(b'.exe')
	crashDumpCount = len(output) - 1
	
	#get paths for exe in Loose folder
	projects = GetFilesPathByExtension("Xbox/Examples_3","exe",False)
	fileList = []
	for proj in projects:
		if "XBOXOne Visual Studio 2017" in proj:# and "Release" in proj:
			if "Loose" in proj:
				fileList.append(os.path.dirname(proj))

	#Deploy Loose folders and store app names
	appList = []
	for filename in fileList:
		#deploy app to xbox
		command = [xdkDir+'xbapp',"deploy",filename,"/X"+consoleIP]
		print ("Deploying: " + filename)
		output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)
		output = output.decode('utf-8')
		#Extract App Name from output
		appName = "InvalidAppName"
		for item in output.split("\n"):
			if "App" in item:
				appName = item.strip()
				appList.append(appName)
				print ("Successfully deployed: " + appName)
		if appName == "InvalidAppName":
			print ("Failed to deploy: " + filename)
			failedTests.append({'name':filename, 'gpu':"", 'reason':"Invalid app name"})
			errorOccured = True
		print ("")

	#Launch the deployed apps
	for appName in appList:
		command = [xdkDir+'xbapp',"launch","/X"+consoleIP, appName]
		print ("Executing command: " + ' '.join(command))
		output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)

		#Make sure app launches
		isRunning = int(0)
		command = [xdkDir+'xbapp',"query","/X"+consoleIP, appName]
		output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)
		for s in output.split():
			if s.isdigit():
				isRunning = int(s)
				print ("The operation completed successfully")

		if isRunning == 0:
			errorOccured = True
			print ("The operation failed")
			failedTests.append({'name':appName, 'gpu':"", 'reason':"Failed to launch app"})
			continue

		#Check if app terminatese or times out
		timeout = time.time() + float(maxIdleTime)
		while isRunning != 0 and time.time() < timeout:
			output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)
			for s in output.split():
				if s.isdigit():
					isRunning = int(s)

		# Timeout Error
		if isRunning != 0:
			errorOccured = True
			print ("Timeout: " + appName + "\n")
			command = [xdkDir+'xbapp',"terminate","/X"+consoleIP, appName]
			output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)
			failedTests.append({'name':appName, 'gpu':"", 'reason':"Runtime failure"})
		else:
			testingComplete = True
			
			#Wait for crash dump folder to be discoverable and get count of crash dumps
			command = ["cmd", "/c", "dir", crashdump_path]
			rc = 1
			while rc != 0:
				rc = subprocess.call(command, stdin=None, stdout=FNULL, stderr=FNULL)
			output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)
			output = output.decode('utf-8').split('.exe')
			
			#Check if a new crash dump was generated
			if (len(output) - 1 > crashDumpCount):
				crashDumpCount = len(output) - 1
				testingComplete = False
			
			if testingComplete:
				print ("Successfully ran " + appName + "\n")
				successfulTests.append({'name':appName, 'gpu':""})
			else:
				errorOccured = True
				print ("Application Terminated Early: " + appName + "\n")
				failedTests.append({'name':appName, 'gpu':"", 'reason':"Runtime failure"})

	#Copy crash dumps to PC and delete them from the console
	command = ["xcopy", crashdump_path, "C:\\dumptemp\\", "/s", "/e"]
	output = subprocess.check_output(command, None, stderr = subprocess.STDOUT)
	copy_tree("C:\\dumptemp", "C:\\XboxOneCrashDumps")
	shutil.rmtree("C:\\dumptemp")
	shutil.rmtree(crashdump_path)
	os.makedirs(crashdump_path)
	
	FNULL.close()

	if errorOccured == True:
		return -1
	return 0
	

def GetImmediateSubdirectories(dir):
	return [os.path.join(dir, name) for name in os.listdir(dir) if os.path.isdir(os.path.join(dir, name))]
	
def DirtyTrickAndroid(root_src_dir, root_target_dir, operation, ignore_dirs, ignore_files):
	for src_dir, dirs, files in os.walk(root_src_dir):
		dst_dir = src_dir.replace(root_src_dir, root_target_dir)
		dir_ignore = False
		for ig_dir in ignore_dirs:
			if(ig_dir in dst_dir):
				dir_ignore = True
		if(dir_ignore):
			continue
		if not os.path.exists(dst_dir):
			os.mkdir(dst_dir)
		for file_ in files:
			if (file_ in ignore_files):
				continue
			src_file = os.path.join(src_dir, file_)
			dst_file = os.path.join(dst_dir, file_)
			if os.path.exists(dst_file):
				os.remove(dst_file)
			if operation is 'copy':
				shutil.copy(src_file, dst_dir)
			elif operation is 'move':
				shutil.move(src_file, dst_dir)

#this needs the JAVA_HOME environment variable set up correctly
def BuildAndroidProjects():
    errorOccured = False
    msBuildPath = FindMSBuild17()

    androidConfigurations = ["Debug", "Release"]
    androidPlatform = ["ARM64"]

    if msBuildPath == "":
        print("Could not find MSBuild 17, Is Visual Studio 17 installed ?")
        sys.exit(-1)

    projects = GetFilesPathByExtension("./Examples_3/Unit_Tests/Android_VisualStudio2017/","sln",False)
    #print(projects)
    fileList = projects
    msbuildVerbosity = "/verbosity:minimal"
    msbuildVerbosityClp = "/clp:ErrorsOnly;WarningsOnly;Summary"
                
    for proj in fileList:
        #get current path for sln file
        #strip the . from ./ in the path
        #replace / by the os separator in case we need // or \\
        rootPath = os.getcwd() + proj.strip('.')
        rootPath = rootPath.replace("/",os.sep)
        #need to get root folder of path by stripping the filename from path
        rootPath = rootPath.split(os.sep)[0:-1]
        rootPath = os.sep.join(rootPath)
        
        configurations = androidConfigurations
        
        #strip extension
        filename = proj.split(os.sep)[-1]
       
        
        for platform in androidPlatform:
            for conf in androidConfigurations:
                command = [msBuildPath ,filename,"/p:Configuration="+conf,"/p:Platform=" + platform,"/nr:false",msbuildVerbosityClp,msbuildVerbosity,"/t:Build"]
                #print(command)
                retCode = ExecuteBuild(command, filename, conf, platform)
        
        
        if retCode != 0:
            errorOccured = True
                

    if errorOccured == True:
        return -1
    return 0  
	
def BuildWindowsProjects(xboxDefined, xboxOnly, skipDebug, skipRelease, printMSBuild, skipAura):
	errorOccured = False
	msBuildPath = FindMSBuild17()

	pcConfigurations = ["DebugDx", "ReleaseDx", "DebugVk", "ReleaseVk", "DebugDx11", "ReleaseDx11"]
	pcPlatform = "x64"
	
	if skipDebug:
		pcConfigurations.remove("DebugDx")
		pcConfigurations.remove("DebugVk")
		pcConfigurations.remove("DebugDx11")
		
	if skipRelease:
		pcConfigurations.remove("ReleaseDx")
		pcConfigurations.remove("ReleaseVk")
		pcConfigurations.remove("ReleaseDx11")

	#xboxConfigurations = ["Debug","Release"]
	xboxPlatform = "Durango"

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

	if not xboxOnly:
		for proj in projects:
			if skipAura == True and "Aura" in proj:
				continue
			if "Android" in proj:
				continue
			#we don't want to build Xbox one solutions when building PC
			if "Xbox" not in proj and "XBOXOne" not in proj:
				fileList.append(proj)

	if xboxDefined:
		for proj in projects:
			if skipAura == True and "Aura" in proj:
				continue
			if "Xbox" in proj or "XBOXOne" in proj:
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
		
		configurations = pcConfigurations
		
		#strip extension
		filename = proj.split(os.sep)[-1]
		
		#hard code the configurations for Aura for now as it's not implemented for Vulkan runtime
		if filename == "Aura.sln":
			if "DebugVk" in configurations : configurations.remove("DebugVk")
			if "ReleaseVk" in configurations : configurations.remove("ReleaseVk")
			if "DebugDx11" in configurations : configurations.remove("DebugDx11")
			if "ReleaseDx11" in configurations : configurations.remove("ReleaseDx11")
		elif filename == "VisibilityBuffer.sln":
			if "DebugDx11" in configurations : configurations.remove("DebugDx11")
			if "ReleaseDx11" in configurations : configurations.remove("ReleaseDx11")
			
		if "Xbox" in proj or "XBOXOne" in proj:
			platform = xboxPlatform
		else:
			platform = pcPlatform
				
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
	leaksMatch = re.findall(r"^(\d+)\s+memory leaks found:$", lineContents, re.MULTILINE | re.IGNORECASE)
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
	
	
def CleanupHandler(signum, frame):
	global setDefines
	global setMemTracker
	print("Bye.")

	#need to change to rootpath otherwise
	#os won't find the files to modify
	os.chdir(sys.path[0])

	if setDefines == True or setMemTracker == True:
		#Remove all defines for automated testing
		print("Removing defines that got added for automated testing")
		RemoveTestingPreProcessor()
	
	exit(1)

#create global variable for interrupt handler
setDefines = False
setMemTracker = False

def MainLogic():
	global setDefines
	global setMemTracker
	global maxIdleTime
	#TODO: Maybe use simpler library for args
	parser = argparse.ArgumentParser(description='Process the Forge builds')
	parser.add_argument('--clean', action="store_true", help='If enabled, will delete all unversioned and untracked files/folder excluding the Art folder.')
	parser.add_argument('--prebuild', action="store_true", help='If enabled, will run PRE_BUILD if assets do not exist.')
	parser.add_argument('--forceprebuild', action="store_true", help='If enabled, will call PRE_BUILD even if assets exist.')
	parser.add_argument('--xbox', action="store_true", help='Enable xbox building')
	parser.add_argument('--xboxonly', action="store_true", help='Enable xbox building')
	parser.add_argument("--skipiosbuild", action="store_true", default=False, help='Disable iOS building')
	parser.add_argument("--skipmacosbuild", action="store_true", default=False, help='Disable Macos building')
	parser.add_argument("--skipioscodesigning", action="store_true", default=False, help='Disable iOS code signing during build stage')
	parser.add_argument('--testing', action="store_true", help='Test the apps on current platform')
	parser.add_argument('--ios', action="store_true", help='Needs --testing. Enable iOS testing')
	parser.add_argument("--iosid", type=str, default="-1", help='Use a specific ios device. Id taken from ios-deploy --detect.')
	parser.add_argument('--macos', action="store_true", help='Needs --testing. Enable macOS testing')
	parser.add_argument('--android', action="store_true", help='Enable android building')
	parser.add_argument('--defines', action="store_true", help='Enables pre processor defines for automated testing.')
	parser.add_argument('--memtracking', action="store_true", help='Enables pre processor defines for memory tracking.')
	parser.add_argument('--gpuselection', action="store_true", help='Enables pre processor defines for using active gpu determined from activeTestingGpu.cfg.')
	parser.add_argument('--timeout',type=int, default="45", help='Specify timeout, in seconds, before app is killed when testing. Default value is 45 seconds.')
	parser.add_argument('--skipdebugbuild', action="store_true", help='If enabled, will skip Debug build.')
	parser.add_argument('--skipreleasebuild', action="store_true", help='If enabled, will skip Release build.')
	parser.add_argument('--printbuildoutput', action="store_true", help='If enabled, will print output of project builds.')
	parser.add_argument('--skipaura', action="store_true", help='If enabled, will skip building aura.')
	parser.add_argument('--xcodederiveddatapath', type=str, default='Null', help = 'Uses a specific path relative to root of project for derived data. If null then it uses the default location for derived data')
	#TODO: remove the test in parse_args
	arguments = parser.parse_args()
	
	#if we want to run based on active gpu config
	#we need defines macros
	if arguments.gpuselection:
		arguments.defines = True
		

	#add cleanup handler in case app gets interrupted
	#keyboard interrupt
	#removing defines
	signal.signal(signal.SIGINT, CleanupHandler)

	#change path to scripts location
	print (sys.path[0])
	os.chdir(sys.path[0])
	returnCode = 0
	
	if (arguments.xbox is not True and arguments.xboxonly is not True) or "XboxOneXDKLatest" not in os.environ:
		arguments.xbox = False
		arguments.xboxonly = False
	
	#if we doing xbox only make sure the --xbox argument is enabled.
	if arguments.xboxonly:
		arguments.xbox = True

	setDefines = arguments.defines
	setMemTracker = arguments.memtracking
	if setDefines == True or setMemTracker == True:
		AddTestingPreProcessor(arguments.gpuselection)

	#PRE_BUILD step
	#if only the prebuild argument is provided but Art folder exists then PRE_BUILd isn't run
	#if only the forceprebuild argument is provided PRE_BUILD runs even if art folder exists
	#this is good for jenkins as we don't want to call PRE_BUILD if art asset exists
	if arguments.prebuild ==  True or arguments.forceprebuild == True:
		if os.path.isdir("./Art") == False or arguments.forceprebuild == True:
			if platform.system() == "Windows":
				ExecuteCommand(["PRE_BUILD.bat"], sys.stdout)
			else:
				ExecuteCommand(["sh","PRE_BUILD.command"], sys.stdout)
	
	systemOS = platform.system()
	if arguments.testing:
		maxIdleTime = max(arguments.timeout,1)
		#Build for Mac OS (Darwin system)
		if systemOS == "Darwin":
			returnCode = TestXcodeProjects(arguments.ios, arguments.macos, arguments.iosid)
		elif systemOS == "Windows":
			if arguments.xbox == True:
				returnCode = TestXboxProjects()
			else:
				returnCode = TestWindowsProjects(arguments.gpuselection)
		elif systemOS.lower() == "linux" or systemOS.lower() == "linux2":
			returnCode = TestLinuxProjects()
	else:
		#Clean before Building removing everything but the art folder
		if arguments.clean == True:
			print("Cleaning the repo")
			ExecuteCommand(["git", "clean" , "--exclude=Art","--exclude=/**/OpenSource/*", "-fdx"],sys.stdout)
			ExecuteCommand(["git", "submodule", "foreach", "--recursive","git clean -fdfx"], sys.stdout)
		#Build for Mac OS (Darwin system)
		if systemOS== "Darwin":
			returnCode = BuildXcodeProjects(arguments.skipmacosbuild,arguments.skipiosbuild, arguments.skipioscodesigning, arguments.skipdebugbuild, arguments.skipreleasebuild, arguments.printbuildoutput, arguments.xcodederiveddatapath)
		elif systemOS == "Windows":
			if arguments.android:
				returnCode = BuildAndroidProjects()
			else:
				returnCode = BuildWindowsProjects(arguments.xbox, arguments.xboxonly, arguments.skipdebugbuild, arguments.skipreleasebuild, arguments.printbuildoutput, arguments.skipaura)
		elif systemOS.lower() == "linux" or systemOS.lower() == "linux2":
			returnCode = BuildLinuxProjects()

	PrintResults()
	
	#Clean up 
	if arguments.defines:
		print("Removing defines that got added for automated testing")
		RemoveTestingPreProcessor()
	
	#return for jenkins
	sys.exit(returnCode)

if __name__ == "__main__":
	MainLogic()
