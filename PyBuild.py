#!/usr/bin/python
import os
import platform
import subprocess
import sys
import shlex

successfulBuilds = []
failedBuilds = []


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

# def FindMSBuild15():
	# import _winreg
	# msBuildPath = ""
	# #get registry for MSBUILD location
	# aReg = _winreg.ConnectRegistry(None,_winreg.HKEY_LOCAL_MACHINE)
	# aKey =  _winreg.OpenKey(aReg,r'SOFTWARE\Microsoft\MSBuild\ToolsVersions\14.0')
	# #potential issue , what if more than 1024 entries and MSBuildToolsPath comes after all those entries?
	# #shouldn't hard code that, MUST find better way :)
	# for i in range(1024):
		# try:
			# asubkey_name= _winreg.EnumValue(aKey,i)        
			# if asubkey_name[0] == "MSBuildToolsPath":  
				# msBuildPath =  asubkey_name[1] + 'MSBuild.exe'
				# break
		# except EnvironmentError as ex:
			# print ex
			# break

	# return msBuildPath

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

def ExecuteBuildCommand(cmdList, fileName, configuration):
	ls_output = ""
	errorOccured = False
	try:
		proc = subprocess.Popen(cmdList, stdout=subprocess.PIPE)
		ls_output = proc.communicate()[0]
		print(ls_output)
		if proc.returncode != 0:
			print("FAILED BUILDING ", fileName, configuration)
			errorOccured = True
			failedBuilds.append({'name':fileName,'conf':configuration})
		else:
			successfulBuilds.append({'name':fileName,'conf':configuration})
	except Exception as ex:
		print(ex)
		print("Failed executing command: " + cmdList)
		print (ls_output)
		errorOccured = True
	
	return errorOccured

def BuildXcodeProjects():
	errorOccured = False
	# traverse root directory, and list directories as dirs and files as files
	for root, dirs, files in os.walk("./Examples_3/"):
		path = root.split(os.sep)
		#in mac os the xcodeproj are not files but packages so they act as directories
		fileExt = root.split(os.extsep)
		#separating the root to get extentions will give us ['path_here'/01_Transformations, xcodeproj]
		for ext in fileExt:
			if ext == 'xcodeproj':
				#get working directory (excluding the xcodeproj in path)
				rootPath = os.sep.join(root.split(os.sep)[0:-1])
				#save current work dir
				currDir = os.getcwd()
				#change dir to xcodeproj location
				os.chdir(rootPath)
				configurations = ["Debug", "Release"]
				for conf in configurations:					
					#create command for xcodebuild
					filename = root.split(os.sep)[-1].split(os.extsep)[0]
					command = ["xcodebuild","-scheme", filename,"-configuration",conf,"build"]
					ls_output = ""
					err = ExecuteBuildCommand(command, filename,conf)
					if err:
						errorOccured = True
					#assuming every xcodeproj has an iOS equivalent. 
					#TODO: Search to verify file exists
					if filename != "Visibility_Buffer":
						filename = filename +"_iOS" 
						command = ["xcodebuild","-scheme", filename,"-configuration",conf,"build"]
						ls_output = ""
						err = ExecuteBuildCommand(command, filename,conf)
						if err:
							errorOccured = True
					
				#set working dir to initial
				os.chdir(currDir)
	
	if errorOccured == True:
		return -1
	return 0
	
def BuildWindowsProjects():
	errorOccured = False
	msBuildPath = ""

	msBuildPath = FindMSBuild17()
	if msBuildPath == "":
		print("Could not find MSBuild 17, Is Visual Studio 17 installed ?")
		sys.exit(-1)
	
	# traverse root directory, and list directories as dirs and files as files
	for root, dirs, files in os.walk("./Examples_3/"):
		path = root.split(os.sep)
		for indFile in files:
			foundPC =False
			for direct in path:
				if direct.find("PC") != -1:
					foundPC = True
			if foundPC == False:
				break
			fileExt = indFile.split(os.extsep,1)
			for ext in fileExt:
				if ext == 'sln':
					#get current path for sln file
					#strip the . from ./ in the path
					#replace / by the os separator in case we need // or \\
					rootPath = os.getcwd() + root.strip('.')
					rootPath = rootPath.replace("/",os.sep)
					#save root directory where python is executed from
					currDir = os.getcwd()
					#change working directory to sln file
					os.chdir(rootPath)
					configurations = ["DebugDx", "ReleaseDx", "DebugVk", "ReleaseVk"]
					
					#hard code the configurations for Aura for now as it's not implemented for Vulkan runtime
					if indFile == "Aura.sln":
						configurations = ["DebugDx", "ReleaseDx"]
					
					for conf in configurations:
						command = [msBuildPath ,indFile,"/p:Configuration="+conf,"/p:Platform=x64","/m","/nr:false","/clp:Summary","/verbosity:minimal"]
						ls_output = ""
						err = ExecuteBuildCommand(command, indFile,conf)
						
						if err:
							errorOccured = True
							
					os.chdir(currDir)
	
	if errorOccured == True:
		return -1
	return 0

	
	
#change path to scripts location
os.chdir(sys.path[0])
returnCode = 0

#Build for Mac OS (Darwin system)
if platform.system() == "Darwin":
	returnCode = BuildXcodeProjects()
elif platform.system() == "Windows":
	returnCode = BuildWindowsProjects()

PrintResults()
#return success
sys.exit(returnCode)