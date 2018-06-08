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
import fnmatch #Checks for matching expression in name
import time    #used for timing process in case one hangs without crashing
import platform #Used for determining running OS
import subprocess #Used for spawning processes
import sys        #system module
import argparse #Used for argument parsing
import traceback
import signal #used for handling ctrl+c keyboard interrupt
import xml.etree.ElementTree as ET  #used for parsing XML ubuntu project file


successfulBuilds = [] #holds all successfull builds
failedBuilds = [] #holds all failed builds

successfulTests = [] #holds all successfull tests
failedTests = [] #holds all failed tests

maxIdleTime = 20  #10 seconds of max idle time with cpu usage null

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
			print(test['name'])
	
	print ("")
	if len(failedTests) > 0:
		print ("Failed Tests list:")
		for test in failedTests:
			print(test['name'])

def FindMSBuild17():
	ls_output = ""
	msbuildPath = ""
	try:
		#open vswhere and parse the output
		proc = subprocess.Popen(["C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe","-latest","-requires" ,"Microsoft.Component.MSBuild"], stdout=subprocess.PIPE)
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

def AddTestingPreProcessor():
	fileList = ["Common_3/OS/Windows/WindowsBase.cpp","Common_3/OS/iOS/iOSBase.mm","Common_3/OS/macOS/macOSBase.mm","CommonXBOXOne_3/OS/XBOXOneBase.cpp", "Common_3/OS/Linux/LinuxBase.cpp"]
	
	for filename in fileList:
		if not os.path.exists(filename):
			continue
		with open(filename,'r+') as f:
			lines = f.readlines()
			for i, line in enumerate(lines):
				if line.startswith('#include') :
					lines[i]=line.replace(line,"#define AUTOMATED_TESTING 1\n" +line)
					break

			f.seek(0)
			for line in lines:
				f.write(line)

def RemoveTestingPreProcessor():
	fileList = ["Common_3/OS/Windows/WindowsBase.cpp","Common_3/OS/iOS/iOSBase.mm","Common_3/OS/macOS/macOSBase.mm","CommonXBOXOne_3/OS/XBOXOneBase.cpp", "Common_3/OS/Linux/LinuxBase.cpp"]
	
	for filename in fileList:
		if not os.path.exists(filename):
			continue
		with open(filename,'r+') as f:
			lines = f.readlines()
			f.seek(0)
			for line in lines:
				if "#define AUTOMATED_TESTING" not in line :
					f.write(line)
			f.truncate()

def ExecuteTimedCommand(cmdList,outStream=subprocess.PIPE):
	try:
		import psutil  #TODO: MAKE SURE THIS PACKAGE IS INSTALLED
		
		print("Executing command: " + ' '.join(cmdList))
		#print("Current Working Directory: " +  os.getcwd())

		#open subprocess without piping output
		# otherwise process blocks until we call communicate or wait
		proc = subprocess.Popen(cmdList)
		
		#get start time of process
		startTime = time.time()
		currentTime = 0
		
		#gets the psutil process which is used for cpu percentage
		psUtilProc = None
		try:
			#try to get psutil process, it can fail if process is done
			#time.sleep(0.1)
			psUtilProc = psutil.Process(proc.pid)
		except psutil.NoSuchProcess:
			#print("No such process")
			#print("Current Process: " + str(proc.pid))
			pass
		except psutil.ZombieProcess:
			#print("Zombie process")
			#print("Current Process: " + str(proc.pid))
			pass
		except psutil.AccessDenied:
			#print("Access Denied") #mac may cause issues here
			pass

		readCount = 0
		memoryPercent = 0
		while proc.poll() is None:
			#monitor cpu usage in case application hangs
			cpuUsage = 0
			memoryUsage = 0
			diskReadCount = 0
			#try catch because cpu_percent will throw exception if process has exited
			try:
				cpuUsage = psUtilProc.cpu_percent(interval=0.1)
				memoryUsage = psUtilProc.memory_percent()
				#apparently mac process does not have disk_io_counters
				if platform.system() == "Windows":
					diskReadCount = psUtilProc.io_counters().read_count
			except psutil.NoSuchProcess:
				#print("No such process")
				pass
				#break
			except psutil.AccessDenied:
				#print("Access Denied")
				pass
			except psutil.ZombieProcess:
				#print("Zombie process")
				pass
				#break
			
			#print('--------------------------------------------')
			#print('Cpu Usage:' + str(cpuUsage))
			#print('Disk Read Count:' + str(readCount))
			#print('Memory Usage:' + str(memoryUsage))
			#print('--------------------------------------------')
			
			#if cpu usage less than 1% then increment counter
			if cpuUsage < 1.0 and readCount <= diskReadCount and memoryUsage <= memoryPercent:
				currentTime = time.time() - startTime
			else:
				#process is active so we need to reset time that is used to detect
				#crashing process
				startTime = time.time()
			
			#update number of times read is called
			readCount = diskReadCount
			memoryPercent = memoryUsage

			#current 'idle' time has reached our max duration
			#then kill process
			if currentTime > maxIdleTime:
				proc.kill()
		
		#used to get return code
		rc = proc.wait()
		if rc != 0:			
			#print("Process returned error")
			return rc

	except Exception as ex:
		#traceback.print_exc()
		print("-------------------------------------")
		print("Failed executing command: " + ' '.join(cmdList))
		print(ex)
		print("-------------------------------------")
		return -1  #error return code
	
	print("Success")
	return 0 #success error code

def ExecuteCommand(cmdList,outStream):
	try:
		print("Executing command: " + ' '.join(cmdList))
		proc = subprocess.Popen(cmdList, outStream)
		if outStream:
			ls_output = proc.communicate()[0]
			print(ls_output)
		else:
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

def ExecuteBuild(cmdList, fileName, configuration, platform):
	returnCode = ExecuteCommand(cmdList, subprocess.PIPE)
	
	if returnCode != 0:
		print("FAILED BUILDING ", fileName, configuration)
		failedBuilds.append({'name':fileName,'conf':configuration, 'platform':platform})
	else:
		successfulBuilds.append({'name':fileName,'conf':configuration, 'platform':platform})
	
	return returnCode

def ExecuteTest(cmdList, fileName, regularCommand):
	if regularCommand:
		returnCode = ExecuteCommand(cmdList, subprocess.PIPE)
	else:
		returnCode = ExecuteTimedCommand(cmdList,None)
	
	if returnCode != 0:
		print("FAILED TESTING ", fileName)
		print("Return code: ", returnCode)
		failedTests.append({'name':fileName})
	else:
		successfulTests.append({'name':fileName})
		
	return returnCode

def GetFilesPathByExtension(rootToSearch, extension, wantDirectory):
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

	return filesPathList

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
		#get working directory (excluding the xcodeproj in path)
		rootPath = os.sep.join(app.split(os.sep)[0:-1])
		filename = app.split(os.sep)[-1].split(os.extsep)[0]
		
		#save current work dir
		currDir = os.getcwd()
		#change dir to xcodeproj location
		os.chdir(rootPath)
		command = []
		retCode = -1

		if "_iOS" in filename:
			#if specific ios id was passed then run for that device
			#otherwise run on first device available
			print iosDeviceId
			if iosDeviceId == "-1" or iosDeviceId == "": 
				command = ["ios-deploy","--uninstall","-b",filename + ".app","-I"]
			else:
				command = ["ios-deploy","--uninstall","-b",filename + ".app","-I", "--id", iosDeviceId]
			
			retCode = ExecuteTest(command, filename, True)
		else:
			command = ["./" + filename + ".app/Contents/MacOS/" + filename]
			retCode = ExecuteTest(command, filename, False)
			
		if retCode != 0:
			errorOccured = True

		#set working dir to initial
		os.chdir(currDir)

	if errorOccured:
		return -1
	else:
		return 0

def BuildXcodeProjects(skipIosBuild, skipIosCodeSigning):
	errorOccured = False
	
	projsToBuild = GetFilesPathByExtension("./Examples_3/","xcodeproj", True)
	for projectPath in projsToBuild:
		#get working directory (excluding the xcodeproj in path)
		rootPath = os.sep.join(projectPath.split(os.sep)[0:-1])
		#save current work dir
		currDir = os.getcwd()
		#change dir to xcodeproj location
		os.chdir(rootPath)
		configurations = ["Debug", "Release"]
		for conf in configurations:					
			#create command for xcodebuild
			filename = projectPath.split(os.sep)[-1].split(os.extsep)[0]
			command = ["xcodebuild","clean","-quiet","-scheme", filename,"-configuration",conf,"build"]
			sucess = ExecuteBuild(command, filename,conf, "macOS")
			if sucess != 0:
				errorOccured = True

			#assuming every xcodeproj has an iOS equivalent. 
			#TODO: Search to verify file exists
			if filename != "Visibility_Buffer" and skipIosBuild == False:
				filename = filename + "_iOS" 
				command = ["xcodebuild","clean","-quiet","-scheme", filename,"-configuration",conf,"build"]
				if skipIosCodeSigning:
					command.extend([
					"CODE_SIGN_IDENTITY=\"\"",
					"CODE_SIGNING_REQUIRED=\"NO\"",
					"CODE_SIGN_ENTITLEMENTS=\"\"",
					"CODE_SIGNING_ALLOWED=\"NO\""])
				sucess = ExecuteBuild(command, filename,conf, "iOS")
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
					if child.attrib["Name"] != "OSBase" and child.attrib["Name"] != "OS" and child.attrib["Name"] != "Renderer" and  child.attrib["Name"] != "SpirVTools" and child.attrib["Name"] != "PaniniProjection":
						ubuntuProjects.append(child.attrib["Name"])
			
			for proj in ubuntuProjects:
				command = ["codelite-make","-w",filename,"-p", proj,"-c",conf]
				#sucess = ExecuteBuild(command, filename+"/"+proj,conf, "Ubuntu")
				sucess = ExecuteCommand(command, subprocess.PIPE)
				
				if sucess != 0:
					errorOccured = True
				
				command = ["make", "clean"]
				sucess = ExecuteCommand(command, subprocess.PIPE)

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
		configurations = ["Debug", "Release"]
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
					if child.attrib["Name"] != "OSBase" and child.attrib["Name"] != "OS" and child.attrib["Name"] != "Renderer" and  child.attrib["Name"] != "SpirVTools" and child.attrib["Name"] != "PaniniProjection":
						ubuntuProjects.append(child.attrib["Name"])
			
			for proj in ubuntuProjects:
				exePath = os.path.join(os.getcwd(),proj,conf,proj)
				command = [exePath]
				sucess = ExecuteTest(command, proj ,False)

				if sucess != 0:
					errorOccured = True

		#set working dir to initial
		os.chdir(currDir)
	
	if errorOccured == True:
		return -1
	return 0

def TestWindowsProjects():
	errorOccured = False
	
	projects = GetFilesPathByExtension("./Examples_3","exe",False)
	fileList = []

	for proj in projects:
		#we don't want to build Xbox one solutions when building PC
		if "PC Visual Studio 2017" in proj and "Release" in proj:
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

		filename = proj.split(os.sep)[-1]

		command = [filename]

		if "ReleaseVk" in proj:
			filename = "VK_" + filename
		else:
			filename = "DX_" + filename

		retCode = ExecuteTest(command, filename,False)
		
		if retCode != 0:
			errorOccured = True
				
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0	

def BuildWindowsProjects(xboxDefined):
	errorOccured = False
	msBuildPath = FindMSBuild17()

	pcConfigurations = ["DebugDx", "ReleaseDx", "DebugVk", "ReleaseVk"]
	pcPlatform = "x64"
	
	xboxConfigurations = ["Debug","Release"]
	xboxPlatform = "Durango"
	
	if msBuildPath == "":
		print("Could not find MSBuild 17, Is Visual Studio 17 installed ?")
		sys.exit(-1)
	
	projects = GetFilesPathByExtension("./Examples_3","sln",False)
	fileList = []

	for proj in projects:
		#we don't want to build Xbox one solutions when building PC
		if xboxDefined:
			if "PC Visual Studio 2017" in proj or "XBOXOne" in proj:
				fileList.append(proj)
		else:
			if "PC Visual Studio 2017" in proj:
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
		platform = pcPlatform
		
		filename = proj.split(os.sep)[-1]
		#hard code the configurations for Aura for now as it's not implemented for Vulkan runtime
		if filename == "Aura.sln" or filename == 'Unit_Tests_Raytracing.sln':
			configurations = ["DebugDx", "ReleaseDx"]
			
		if "XBOXOne" in proj:
			configurations = xboxConfigurations
			platform = xboxPlatform
				
		for conf in configurations:
			command = [msBuildPath ,filename,"/p:Configuration="+conf,"/p:Platform=" + platform,"/m","/nr:false","/clp:ErrorsOnly;Summary","/verbosity:minimal","/t:Rebuild"]
			retCode = ExecuteBuild(command, filename,conf, platform)
			
			if retCode != 0:
				errorOccured = True
				
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0	

def CleanupHandler(signum, frame):
	global setDefines
	print("Bye.")

	#need to change to rootpath otherwise
	#os won't find the files to modify
	os.chdir(sys.path[0])

	if setDefines == True:
		#Remove all defines for automated testing
		print("Removing defines that got added for automated testing")
		RemoveTestingPreProcessor()
	
	exit(1)

#create global variable for interrupt handler
setDefines = False

def MainLogic():
	global setDefines
	#TODO: Maybe use simpler library for args
	parser = argparse.ArgumentParser(description='Process the Forge builds')
	parser.add_argument('--prebuild', action="store_true", help='If enabled, will run PRE_BUILD if assets do not exist.')
	parser.add_argument('--forceprebuild', action="store_true", help='If enabled, will call PRE_BUILD even if assets exist.')
	parser.add_argument('--xbox', action="store_true", help='Enable xbox building')
	parser.add_argument("--skipiosbuild", action="store_true", default=False, help='Disable iOS building')
	parser.add_argument("--skipioscodesigning", action="store_true", default=False, help='Disable iOS code signing')
	parser.add_argument('--testing', action="store_true", help='Test the apps on current platform')
	parser.add_argument('--ios', action="store_true", help='Needs --testing. Enable iOS testing')
	parser.add_argument("--iosid", type=str, default="-1", help='Use a specific ios device. Id taken from ios-deploy --detect.')
	parser.add_argument('--macos', action="store_true", help='Needs --testing. Enable macOS testing')
	parser.add_argument('--defines', action="store_true", help='Enables pre processor defines for automated testing.')

	#TODO: remove the test in parse_args
	#arguments = parser.parse_args()
	arguments = parser.parse_args()
	
	#add cleanup handler in case app gets interrupted
	#keyboard interrupt
	#removing defines
	signal.signal(signal.SIGINT, CleanupHandler)

	#change path to scripts location
	os.chdir(sys.path[0])
	returnCode = 0
	
	if arguments.xbox is not True or "XboxOneXDKLatest" not in os.environ:
		arguments.xbox = False
	
	setDefines = arguments.defines
	if setDefines == True:
		print("Adding defines for automated testing")
		AddTestingPreProcessor()

	#PRE_BUILD step
	#if only the prebuild argument is provided but Art folder exists then PRE_BUILd isn't run
	#if only the forceprebuild argument is provided PRE_BUILD runs even if art folder exists
	#this is good for jenkins as we don't want to call PRE_BUILD if art asset exists
	if arguments.prebuild ==  True or arguments.forceprebuild == True:
		if os.path.isdir("./Art") == False or arguments.forceprebuild == True:
			if platform.system() == "Windows":
				ExecuteCommand(["PRE_BUILD.bat"], subprocess.PIPE)
			else:
				ExecuteCommand(["sh","PRE_BUILD.sh"], subprocess.PIPE)
	
	systemOS = platform.system()
	print systemOS
	if arguments.testing:
		#Build for Mac OS (Darwin system)
		if systemOS == "Darwin":
			returnCode = TestXcodeProjects(arguments.ios, arguments.macos, arguments.iosid)
		elif systemOS == "Windows":
			returnCode = TestWindowsProjects()
		elif systemOS.lower() == "linux" or systemOS.lower() == "linux2":
			returnCode = TestLinuxProjects()
	else:
		#Build for Mac OS (Darwin system)
		if systemOS== "Darwin":
			returnCode = BuildXcodeProjects(arguments.skipiosbuild, arguments.skipioscodesigning)
		elif systemOS == "Windows":
			returnCode = BuildWindowsProjects(arguments.xbox)
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
