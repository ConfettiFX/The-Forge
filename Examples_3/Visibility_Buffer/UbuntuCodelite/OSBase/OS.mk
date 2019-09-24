##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=OS
ConfigurationName      :=Release
WorkspacePath          :=/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite
ProjectPath            :=/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite/OSBase
IntermediateDirectory  :=./Release
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=Confetti
Date                   :=16/09/19
CodeLitePath           :=/home/confetti/.codelite
LinkerName             :=/usr/bin/g++
SharedObjectLinkerName :=/usr/bin/g++ -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.i
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputFile             :=$(IntermediateDirectory)/lib$(ProjectName).a
Preprocessors          :=$(PreprocessorSwitch)VULKAN $(PreprocessorSwitch)NDEBUG 
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="OS.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). 
IncludePCH             := 
RcIncludePath          := 
Libs                   := 
ArLibs                 :=  
LibPath                := $(LibraryPathSwitch). 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /usr/bin/ar rcu
CXX      := /usr/bin/g++
CC       := /usr/bin/gcc
CXXFLAGS :=  -std=c++14  $(Preprocessors)
CFLAGS   :=   $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Camera_CameraController.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_FileSystem.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_ThreadSystem.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_Timer.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Input_InputSystem.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Logging_Log.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_MemoryTracking_MemoryTracking.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxBase.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxFileSystem.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxLog.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxThread.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxTime.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_TinyEXR_tinyexr.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Middleware_3_Text_Fontstash.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_AppUI.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_ImguiGUIDriver.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_demo.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_draw.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_widgets.cpp$(ObjectSuffix) \
	



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: $(IntermediateDirectory) $(OutputFile)

$(OutputFile): $(Objects)
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(AR) $(ArchiveOutputSwitch)$(OutputFile) @$(ObjectsFileList) $(ArLibs)
	@$(MakeDirCommand) "/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite/.build-release"
	@echo rebuilt > "/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite/.build-release/OS"

MakeIntermediateDirs:
	@test -d ./Release || $(MakeDirCommand) ./Release


./Release:
	@test -d ./Release || $(MakeDirCommand) ./Release

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Camera_CameraController.cpp$(ObjectSuffix): ../../../../Common_3/OS/Camera/CameraController.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Camera_CameraController.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Camera/CameraController.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Camera_CameraController.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Camera_CameraController.cpp$(DependSuffix): ../../../../Common_3/OS/Camera/CameraController.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Camera_CameraController.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Camera_CameraController.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Camera/CameraController.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Camera_CameraController.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Camera/CameraController.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Camera_CameraController.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Camera/CameraController.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_FileSystem.cpp$(ObjectSuffix): ../../../../Common_3/OS/Core/FileSystem.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_FileSystem.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Core/FileSystem.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_FileSystem.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_FileSystem.cpp$(DependSuffix): ../../../../Common_3/OS/Core/FileSystem.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_FileSystem.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_FileSystem.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Core/FileSystem.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_FileSystem.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Core/FileSystem.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_FileSystem.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Core/FileSystem.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_ThreadSystem.cpp$(ObjectSuffix): ../../../../Common_3/OS/Core/ThreadSystem.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_ThreadSystem.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Core/ThreadSystem.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_ThreadSystem.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_ThreadSystem.cpp$(DependSuffix): ../../../../Common_3/OS/Core/ThreadSystem.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_ThreadSystem.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_ThreadSystem.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Core/ThreadSystem.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_ThreadSystem.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Core/ThreadSystem.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_ThreadSystem.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Core/ThreadSystem.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_Timer.cpp$(ObjectSuffix): ../../../../Common_3/OS/Core/Timer.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_Timer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Core/Timer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_Timer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_Timer.cpp$(DependSuffix): ../../../../Common_3/OS/Core/Timer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_Timer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_Timer.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Core/Timer.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_Timer.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Core/Timer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Core_Timer.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Core/Timer.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Input_InputSystem.cpp$(ObjectSuffix): ../../../../Common_3/OS/Input/InputSystem.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Input_InputSystem.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Input/InputSystem.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Input_InputSystem.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Input_InputSystem.cpp$(DependSuffix): ../../../../Common_3/OS/Input/InputSystem.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Input_InputSystem.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Input_InputSystem.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Input/InputSystem.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Input_InputSystem.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Input/InputSystem.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Input_InputSystem.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Input/InputSystem.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Logging_Log.cpp$(ObjectSuffix): ../../../../Common_3/OS/Logging/Log.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Logging_Log.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Logging/Log.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Logging_Log.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Logging_Log.cpp$(DependSuffix): ../../../../Common_3/OS/Logging/Log.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Logging_Log.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Logging_Log.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Logging/Log.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Logging_Log.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Logging/Log.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Logging_Log.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Logging/Log.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_MemoryTracking_MemoryTracking.cpp$(ObjectSuffix): ../../../../Common_3/OS/MemoryTracking/MemoryTracking.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_MemoryTracking_MemoryTracking.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/MemoryTracking/MemoryTracking.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_MemoryTracking_MemoryTracking.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_MemoryTracking_MemoryTracking.cpp$(DependSuffix): ../../../../Common_3/OS/MemoryTracking/MemoryTracking.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_MemoryTracking_MemoryTracking.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_MemoryTracking_MemoryTracking.cpp$(DependSuffix) -MM ../../../../Common_3/OS/MemoryTracking/MemoryTracking.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_MemoryTracking_MemoryTracking.cpp$(PreprocessSuffix): ../../../../Common_3/OS/MemoryTracking/MemoryTracking.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_MemoryTracking_MemoryTracking.cpp$(PreprocessSuffix) ../../../../Common_3/OS/MemoryTracking/MemoryTracking.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxBase.cpp$(ObjectSuffix): ../../../../Common_3/OS/Linux/LinuxBase.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxBase.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Linux/LinuxBase.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxBase.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxBase.cpp$(DependSuffix): ../../../../Common_3/OS/Linux/LinuxBase.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxBase.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxBase.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Linux/LinuxBase.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxBase.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Linux/LinuxBase.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxBase.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Linux/LinuxBase.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxFileSystem.cpp$(ObjectSuffix): ../../../../Common_3/OS/Linux/LinuxFileSystem.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxFileSystem.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Linux/LinuxFileSystem.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxFileSystem.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxFileSystem.cpp$(DependSuffix): ../../../../Common_3/OS/Linux/LinuxFileSystem.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxFileSystem.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxFileSystem.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Linux/LinuxFileSystem.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxFileSystem.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Linux/LinuxFileSystem.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxFileSystem.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Linux/LinuxFileSystem.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxLog.cpp$(ObjectSuffix): ../../../../Common_3/OS/Linux/LinuxLog.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxLog.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Linux/LinuxLog.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxLog.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxLog.cpp$(DependSuffix): ../../../../Common_3/OS/Linux/LinuxLog.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxLog.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxLog.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Linux/LinuxLog.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxLog.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Linux/LinuxLog.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxLog.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Linux/LinuxLog.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxThread.cpp$(ObjectSuffix): ../../../../Common_3/OS/Linux/LinuxThread.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxThread.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Linux/LinuxThread.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxThread.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxThread.cpp$(DependSuffix): ../../../../Common_3/OS/Linux/LinuxThread.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxThread.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxThread.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Linux/LinuxThread.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxThread.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Linux/LinuxThread.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxThread.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Linux/LinuxThread.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxTime.cpp$(ObjectSuffix): ../../../../Common_3/OS/Linux/LinuxTime.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxTime.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Linux/LinuxTime.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxTime.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxTime.cpp$(DependSuffix): ../../../../Common_3/OS/Linux/LinuxTime.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxTime.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxTime.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Linux/LinuxTime.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxTime.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Linux/LinuxTime.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Linux_LinuxTime.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Linux/LinuxTime.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_TinyEXR_tinyexr.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/TinyEXR/tinyexr.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_TinyEXR_tinyexr.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/TinyEXR/tinyexr.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_TinyEXR_tinyexr.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_TinyEXR_tinyexr.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/TinyEXR/tinyexr.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_TinyEXR_tinyexr.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_TinyEXR_tinyexr.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/TinyEXR/tinyexr.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_TinyEXR_tinyexr.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/TinyEXR/tinyexr.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_TinyEXR_tinyexr.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/TinyEXR/tinyexr.cpp

$(IntermediateDirectory)/up_up_up_up_Middleware_3_Text_Fontstash.cpp$(ObjectSuffix): ../../../../Middleware_3/Text/Fontstash.cpp $(IntermediateDirectory)/up_up_up_up_Middleware_3_Text_Fontstash.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Middleware_3/Text/Fontstash.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Middleware_3_Text_Fontstash.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Middleware_3_Text_Fontstash.cpp$(DependSuffix): ../../../../Middleware_3/Text/Fontstash.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Middleware_3_Text_Fontstash.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Middleware_3_Text_Fontstash.cpp$(DependSuffix) -MM ../../../../Middleware_3/Text/Fontstash.cpp

$(IntermediateDirectory)/up_up_up_up_Middleware_3_Text_Fontstash.cpp$(PreprocessSuffix): ../../../../Middleware_3/Text/Fontstash.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Middleware_3_Text_Fontstash.cpp$(PreprocessSuffix) ../../../../Middleware_3/Text/Fontstash.cpp

$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_AppUI.cpp$(ObjectSuffix): ../../../../Middleware_3/UI/AppUI.cpp $(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_AppUI.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Middleware_3/UI/AppUI.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_AppUI.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_AppUI.cpp$(DependSuffix): ../../../../Middleware_3/UI/AppUI.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_AppUI.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_AppUI.cpp$(DependSuffix) -MM ../../../../Middleware_3/UI/AppUI.cpp

$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_AppUI.cpp$(PreprocessSuffix): ../../../../Middleware_3/UI/AppUI.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_AppUI.cpp$(PreprocessSuffix) ../../../../Middleware_3/UI/AppUI.cpp

$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_ImguiGUIDriver.cpp$(ObjectSuffix): ../../../../Middleware_3/UI/ImguiGUIDriver.cpp $(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_ImguiGUIDriver.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Middleware_3/UI/ImguiGUIDriver.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_ImguiGUIDriver.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_ImguiGUIDriver.cpp$(DependSuffix): ../../../../Middleware_3/UI/ImguiGUIDriver.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_ImguiGUIDriver.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_ImguiGUIDriver.cpp$(DependSuffix) -MM ../../../../Middleware_3/UI/ImguiGUIDriver.cpp

$(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_ImguiGUIDriver.cpp$(PreprocessSuffix): ../../../../Middleware_3/UI/ImguiGUIDriver.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Middleware_3_UI_ImguiGUIDriver.cpp$(PreprocessSuffix) ../../../../Middleware_3/UI/ImguiGUIDriver.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/imgui/imgui.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_demo.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_demo.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_demo.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/imgui/imgui_demo.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_demo.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_demo.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_demo.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_demo.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_demo.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_demo.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_demo.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_demo.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_demo.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_demo.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_draw.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_draw.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_draw.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/imgui/imgui_draw.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_draw.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_draw.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_draw.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_draw.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_draw.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_draw.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_draw.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_draw.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_draw.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_draw.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_widgets.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_widgets.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_widgets.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/imgui/imgui_widgets.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_widgets.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_widgets.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_widgets.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_widgets.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_widgets.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_widgets.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_widgets.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_widgets.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_imgui_imgui_widgets.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/imgui/imgui_widgets.cpp


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Release/


