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
import fnmatch #Checks for matching expression in name
import time    #used for timing process in case one hangs without crashing
import platform #Used for determining running OS
import subprocess #Used for spawning processes
import sys        #system module
import argparse #Used for argument parsing
import psutil  #TODO: MAKE SURE THIS PACKAGE IS INSTALLED (Use setupTools ?)
import traceback

successfulBuilds = [] #holds all successfull builds
failedBuilds = [] #holds all failed builds

successfulTests = [] #holds all successfull tests
failedTests = [] #holds all failed tests

maxIdleTime = 10  #10 seconds of max idle time with cpu usage null

def PrintResults():
	if len(successfulBuilds) > 0:
		print ("Successful Builds list:")
		for build in successfulBuilds:
			print(build['name'], build['conf'])
	
	print ("")
	if len(failedBuilds) > 0:
		print ("Failed Builds list:")
		for build in failedBuilds:
			print(build['name'], build['conf'])
			
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

def ExecuteTimedCommand(cmdList,outStream=subprocess.PIPE):
	try:
		print("Executing command: " + ' '.join(cmdList))

		#open subprocess without piping output
		# otherwise process blocks until we call communicate or wait
		proc = subprocess.Popen(cmdList)

		#get start time of process
		startTime = time.time()
		currentTime = 0
		
		readCount = 0
		while proc.poll() is None:
			#gets the psutil process which is used for cpu percentage
			psUtilProc = None
			try:
				#try to get psutil process, it can fail if process is done
				psUtilProc = psutil.Process(proc.pid)
			except psutil.NoSuchProcess:
				break

			#monitor cpu usage in case application hangs
			cpuUsage = 0
			diskReadCount = 0
			#try catch because cpu_percent will throw exception if process has exited
			try:
				cpuUsage = psUtilProc.cpu_percent(interval=0.05)
				#apparently mac process does not have disk_io_counters
				if platform.system() == "Windows":
					diskReadCount = psUtilProc.io_counters().read_count
			except psutil.NoSuchProcess:
				break
			except psutil.AccessDenied:
				break

			#if cpu usage less than 1% then increment counter
			if cpuUsage < 1.0 and readCount <= diskReadCount:
				currentTime = time.time() - startTime
			else:
			#process is active so we need to reset time that is used to detect
			#crashing process
				startTime = time.time()
			
			#update number of times read is called
			readCount = diskReadCount

			#current 'idle' time has reached our max duration
			#then kill process
			if currentTime > maxIdleTime:
				proc.kill()
		
		#used to get return code
		rc = proc.wait()

		if rc != 0:
			return rc

	except Exception as ex:
		#traceback.print_exc()
		print("-------------------------------------")
		print("Failed executing command: " + ' '.join(cmdList))
		print(ex)
		print("-------------------------------------")
		return -1  #error return code
	
	return 0 #success error code

def ExecuteCommand(cmdList,outStream=subprocess.PIPE):
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

def ExecuteBuild(cmdList, fileName, configuration):
	returnCode = ExecuteCommand(cmdList)
	
	if returnCode != 0:
		print("FAILED BUILDING ", fileName, configuration)
		failedBuilds.append({'name':fileName,'conf':configuration})
	else:
		successfulBuilds.append({'name':fileName,'conf':configuration})
	
	return returnCode

def ExecuteTest(cmdList, fileName, regularCommand):
	if regularCommand:
		returnCode = ExecuteCommand(cmdList, subprocess.PIPE)
	else:
		returnCode = ExecuteTimedCommand(cmdList,None)
	
	if returnCode != 0:
		print("FAILED TESTING ", fileName)
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

def TestXcodeProjects(iosTesting, macOSTesting):
	errorOccured = False

	projects = GetFilesPathByExtension("./Examples_3/Unit_Tests/macOS Xcode/Bin/Release/","app", True)
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
		print(rootPath)
		if "_iOS" in filename:
			command = ["ios-deploy","-b",filename + ".app","-I", "-a","--testing"]
			retCode = ExecuteTest(command, filename, True)
		else:
			command = ["./" + filename + ".app/Contents/MacOS/" + filename,"--testing"]
			retCode = ExecuteTest(command, filename, False)
			
		if retCode != 0:
			errorOccured = True

		#set working dir to initial
		os.chdir(currDir)

	if errorOccured:
		return -1
	else:
		return 0

def BuildXcodeProjects():
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
			command = ["xcodebuild","-scheme", filename,"-configuration",conf,"build"]
			sucess = ExecuteBuild(command, filename,conf)
			if sucess != 0:
				errorOccured = True

			#assuming every xcodeproj has an iOS equivalent. 
			#TODO: Search to verify file exists
			if filename != "Visibility_Buffer":
				filename = filename +"_iOS" 
				command = ["xcodebuild","-scheme", filename,"-configuration",conf,"build","-allowProvisioningDeviceRegistration"]
				sucess = ExecuteBuild(command, filename,conf)
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
		if "PC Visual Studio 2017" and "Release" in proj:
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

		command = ["./"+filename,"--testing"]

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

def BuildWindowsProjects():
	errorOccured = False
	msBuildPath = FindMSBuild17()

	if msBuildPath == "":
		print("Could not find MSBuild 17, Is Visual Studio 17 installed ?")
		sys.exit(-1)
	
	projects = GetFilesPathByExtension("./Examples_3","sln",False)
	fileList = []

	for proj in projects:
		#we don't want to build Xbox one solutions when building PC
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
		configurations = ["DebugDx", "ReleaseDx", "DebugVk", "ReleaseVk"]
		filename = proj.split(os.sep)[-1]
		#hard code the configurations for Aura for now as it's not implemented for Vulkan runtime
		if filename == "Aura.sln":
			configurations = ["DebugDx", "ReleaseDx"]
		
		for conf in configurations:
			command = [msBuildPath ,filename,"/p:Configuration="+conf,"/p:Platform=x64","/m","/nr:false","/clp:Summary","/verbosity:minimal"]
			retCode = ExecuteBuild(command, filename,conf)
			
			if retCode != 0:
				errorOccured = True
				
		os.chdir(currDir)

	if errorOccured == True:
		return -1
	return 0	

def MainLogic():

	#TODO: Maybe use simpler library for args
	parser = argparse.ArgumentParser(description='Process the Forge builds')
	parser.add_argument('--testing', action="store_true", help='Test the apps on current platform')
	parser.add_argument('--ios', action="store_true", help='Needs --testing. Enable iOS testing')
	parser.add_argument('--macos', action="store_true", help='Needs --testing. Enable macOS testing')

	#TODO: remove the test in parse_args
	#arguments = parser.parse_args()
	arguments = parser.parse_args()

	#change path to scripts location
	os.chdir(sys.path[0])
	returnCode = 0

	if arguments.testing:
		#Build for Mac OS (Darwin system)
		if platform.system() == "Darwin":
			returnCode = TestXcodeProjects(arguments.ios, arguments.macos)
		elif platform.system() == "Windows":
			returnCode = TestWindowsProjects()
	else:
		#Build for Mac OS (Darwin system)
		if platform.system() == "Darwin":
			returnCode = BuildXcodeProjects()
		elif platform.system() == "Windows":
			returnCode = BuildWindowsProjects()

	PrintResults()

	#return for jenkins
	sys.exit(returnCode)

if __name__ == "__main__":
	MainLogic()
