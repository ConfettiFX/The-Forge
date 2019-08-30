##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=AssetPipelineCmd
ConfigurationName      :=Release
WorkspacePath          :=/home/confetti/Desktop/Ethan/The-Forge/Examples_3/Unit_Tests/UbuntuCodelite
ProjectPath            :=/home/confetti/Desktop/Ethan/The-Forge/Common_3/Tools/AssetPipeline/Linux
IntermediateDirectory  :=./Release
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=Confetti
Date                   :=29/08/19
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
Preprocessors          :=$(PreprocessorSwitch)NDEBUG 
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="AssetPipelineCmd.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  -pthread
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)AssetPipeline $(LibrarySwitch)OS $(LibrarySwitch)assimp $(LibrarySwitch)zlibstatic $(LibrarySwitch)ozz_animation_offline $(LibrarySwitch)ozz_animation $(LibrarySwitch)ozz_base $(LibrarySwitch)EASTL $(LibrarySwitch)MeshOptimizer 
ArLibs                 :=  "libAssetPipeline.a" "libOS.a" "libassimp.a" "libzlibstatic.a" "libozz_animation_offline.a" "libozz_animation.a" "libozz_base.a" "libEASTL.a" "libMeshOptimizer.a" 
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch)$(IntermediateDirectory) $(LibraryPathSwitch)../../../../Examples_3/Unit_Tests/UbuntuCodelite/OSBase/$(IntermediateDirectory) $(LibraryPathSwitch)../../../ThirdParty/OpenSource/assimp/4.1.0/linux/Bin $(LibraryPathSwitch)$(WorkspacePath)/ozz_base/$(IntermediateDirectory) $(LibraryPathSwitch)$(WorkspacePath)/ozz_animation_offline/$(IntermediateDirectory) $(LibraryPathSwitch)$(WorkspacePath)/ozz_animation/$(IntermediateDirectory) $(LibraryPathSwitch)$(ProjectPath)/../../../../Common_3/ThirdParty/OpenSource/EASTL/Linux/Release/ $(LibraryPathSwitch)$(WorkspacePath)/MeshOptimizer/$(IntermediateDirectory)/ 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := /usr/bin/ar rcu
CXX      := /usr/bin/g++
CC       := /usr/bin/gcc
CXXFLAGS :=  -O2 -Wall $(Preprocessors)
CFLAGS   :=  -O2 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/up_src_AssetPipelineCmd.cpp$(ObjectSuffix) 



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
$(IntermediateDirectory)/up_src_AssetPipelineCmd.cpp$(ObjectSuffix): ../src/AssetPipelineCmd.cpp $(IntermediateDirectory)/up_src_AssetPipelineCmd.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Ethan/The-Forge/Common_3/Tools/AssetPipeline/src/AssetPipelineCmd.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_AssetPipelineCmd.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_AssetPipelineCmd.cpp$(DependSuffix): ../src/AssetPipelineCmd.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_AssetPipelineCmd.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_AssetPipelineCmd.cpp$(DependSuffix) -MM ../src/AssetPipelineCmd.cpp

$(IntermediateDirectory)/up_src_AssetPipelineCmd.cpp$(PreprocessSuffix): ../src/AssetPipelineCmd.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_AssetPipelineCmd.cpp$(PreprocessSuffix) ../src/AssetPipelineCmd.cpp


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Release/


