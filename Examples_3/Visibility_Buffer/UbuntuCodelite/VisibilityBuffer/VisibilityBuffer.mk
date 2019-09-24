##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=VisibilityBuffer
ConfigurationName      :=Release
WorkspacePath          :=/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite
ProjectPath            :=/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite/VisibilityBuffer
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
OutputFile             :=$(IntermediateDirectory)/$(ProjectName)
Preprocessors          :=$(PreprocessorSwitch)VULKAN $(PreprocessorSwitch)NDEBUG 
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="VisibilityBuffer.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  -ldl -pthread 
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). $(IncludeSwitch)$(ProjectPath)/../.. $(IncludeSwitch)$(ProjectPath)/../../../../Common_3/ThirdParty/OpenSource/assimp/4.1.0/include $(IncludeSwitch)$(ProjectPath)/../../../.. 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)OS $(LibrarySwitch)Renderer $(LibrarySwitch)X11 $(LibrarySwitch)SpirVTools $(LibrarySwitch)vulkan $(LibrarySwitch)gainput $(LibrarySwitch)zlibstatic $(LibrarySwitch)assimp $(LibrarySwitch)EASTL 
ArLibs                 :=  "libOS.a" "libRenderer.a" "libX11.a" "libSpirVTools.a" "libvulkan.so" "libgainput.a" "libzlibstatic.a" "libassimp.a" "libEASTL.a" 
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch)$(ProjectPath)/../gainput/Release/ $(LibraryPathSwitch)$(ProjectPath)/../OSBase/Release/ $(LibraryPathSwitch)$(ProjectPath)/../Renderer/Release/ $(LibraryPathSwitch)$(ProjectPath)/../SpirVTools/Release/ $(LibraryPathSwitch)$(ProjectPath)/../../../../Common_3/ThirdParty/OpenSource/assimp/4.1.0/linux/Bin/ $(LibraryPathSwitch)$(ProjectPath)/../../../../Common_3/ThirdParty/OpenSource/EASTL/Linux/Release/ 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /usr/bin/ar rcu
CXX      := /usr/bin/g++
CC       := /usr/bin/gcc
CXXFLAGS :=  -O2 -std=c++14 -Wall -Wno-unknown-pragmas -march=native -msse4.1  $(Preprocessors)
CFLAGS   :=  -O2 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_AssimpImporter_AssimpImporter.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_src_Geometry.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_src_Visibility_Buffer.cpp$(ObjectSuffix) 



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(LinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)

MakeIntermediateDirs:
	@test -d ./Release || $(MakeDirCommand) ./Release


$(IntermediateDirectory)/.d:
	@test -d ./Release || $(MakeDirCommand) ./Release

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_AssimpImporter_AssimpImporter.cpp$(ObjectSuffix): ../../../../Common_3/Tools/AssimpImporter/AssimpImporter.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_Tools_AssimpImporter_AssimpImporter.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/Tools/AssimpImporter/AssimpImporter.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_AssimpImporter_AssimpImporter.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_AssimpImporter_AssimpImporter.cpp$(DependSuffix): ../../../../Common_3/Tools/AssimpImporter/AssimpImporter.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_AssimpImporter_AssimpImporter.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_AssimpImporter_AssimpImporter.cpp$(DependSuffix) -MM ../../../../Common_3/Tools/AssimpImporter/AssimpImporter.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_AssimpImporter_AssimpImporter.cpp$(PreprocessSuffix): ../../../../Common_3/Tools/AssimpImporter/AssimpImporter.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_Tools_AssimpImporter_AssimpImporter.cpp$(PreprocessSuffix) ../../../../Common_3/Tools/AssimpImporter/AssimpImporter.cpp

$(IntermediateDirectory)/up_up_src_Geometry.cpp$(ObjectSuffix): ../../src/Geometry.cpp $(IntermediateDirectory)/up_up_src_Geometry.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/src/Geometry.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_src_Geometry.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_src_Geometry.cpp$(DependSuffix): ../../src/Geometry.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_src_Geometry.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_src_Geometry.cpp$(DependSuffix) -MM ../../src/Geometry.cpp

$(IntermediateDirectory)/up_up_src_Geometry.cpp$(PreprocessSuffix): ../../src/Geometry.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_src_Geometry.cpp$(PreprocessSuffix) ../../src/Geometry.cpp

$(IntermediateDirectory)/up_up_src_Visibility_Buffer.cpp$(ObjectSuffix): ../../src/Visibility_Buffer.cpp $(IntermediateDirectory)/up_up_src_Visibility_Buffer.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/src/Visibility_Buffer.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_src_Visibility_Buffer.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_src_Visibility_Buffer.cpp$(DependSuffix): ../../src/Visibility_Buffer.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_src_Visibility_Buffer.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_src_Visibility_Buffer.cpp$(DependSuffix) -MM ../../src/Visibility_Buffer.cpp

$(IntermediateDirectory)/up_up_src_Visibility_Buffer.cpp$(PreprocessSuffix): ../../src/Visibility_Buffer.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_src_Visibility_Buffer.cpp$(PreprocessSuffix) ../../src/Visibility_Buffer.cpp


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Release/


