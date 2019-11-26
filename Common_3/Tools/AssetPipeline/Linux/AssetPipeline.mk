##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=AssetPipeline
ConfigurationName      :=Debug
WorkspacePath          :=/home/confetti/Desktop/Gitlab/The-Forge/Examples_3/Unit_Tests/UbuntuCodelite
ProjectPath            :=/home/confetti/Desktop/Gitlab/The-Forge/Common_3/Tools/AssetPipeline/Linux
IntermediateDirectory  :=./Debug
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=Confetti
Date                   :=25/11/19
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
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="AssetPipeline.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch)../../../ThirdParty/OpenSource/assimp/4.1.0/include $(IncludeSwitch)../../../ThirdParty/OpenSource/ozz-animation/include 
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
CXXFLAGS :=  -g $(Preprocessors)
CFLAGS   :=  -g $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/up_src_AssetLoader.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_src_AssetPipeline.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_src_gltfpack.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_src_TFXImporter.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_src_TressFXAsset.cpp$(ObjectSuffix) 



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
	@$(MakeDirCommand) "/home/confetti/Desktop/Gitlab/The-Forge/Examples_3/Unit_Tests/UbuntuCodelite/.build-debug"
	@echo rebuilt > "/home/confetti/Desktop/Gitlab/The-Forge/Examples_3/Unit_Tests/UbuntuCodelite/.build-debug/AssetPipeline"

MakeIntermediateDirs:
	@test -d ./Debug || $(MakeDirCommand) ./Debug


./Debug:
	@test -d ./Debug || $(MakeDirCommand) ./Debug

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/up_src_AssetLoader.cpp$(ObjectSuffix): ../src/AssetLoader.cpp $(IntermediateDirectory)/up_src_AssetLoader.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Gitlab/The-Forge/Common_3/Tools/AssetPipeline/src/AssetLoader.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_AssetLoader.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_AssetLoader.cpp$(DependSuffix): ../src/AssetLoader.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_AssetLoader.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_AssetLoader.cpp$(DependSuffix) -MM ../src/AssetLoader.cpp

$(IntermediateDirectory)/up_src_AssetLoader.cpp$(PreprocessSuffix): ../src/AssetLoader.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_AssetLoader.cpp$(PreprocessSuffix) ../src/AssetLoader.cpp

$(IntermediateDirectory)/up_src_AssetPipeline.cpp$(ObjectSuffix): ../src/AssetPipeline.cpp $(IntermediateDirectory)/up_src_AssetPipeline.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Gitlab/The-Forge/Common_3/Tools/AssetPipeline/src/AssetPipeline.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_AssetPipeline.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_AssetPipeline.cpp$(DependSuffix): ../src/AssetPipeline.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_AssetPipeline.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_AssetPipeline.cpp$(DependSuffix) -MM ../src/AssetPipeline.cpp

$(IntermediateDirectory)/up_src_AssetPipeline.cpp$(PreprocessSuffix): ../src/AssetPipeline.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_AssetPipeline.cpp$(PreprocessSuffix) ../src/AssetPipeline.cpp

$(IntermediateDirectory)/up_src_gltfpack.cpp$(ObjectSuffix): ../src/gltfpack.cpp $(IntermediateDirectory)/up_src_gltfpack.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Gitlab/The-Forge/Common_3/Tools/AssetPipeline/src/gltfpack.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_gltfpack.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_gltfpack.cpp$(DependSuffix): ../src/gltfpack.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_gltfpack.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_gltfpack.cpp$(DependSuffix) -MM ../src/gltfpack.cpp

$(IntermediateDirectory)/up_src_gltfpack.cpp$(PreprocessSuffix): ../src/gltfpack.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_gltfpack.cpp$(PreprocessSuffix) ../src/gltfpack.cpp

$(IntermediateDirectory)/up_src_TFXImporter.cpp$(ObjectSuffix): ../src/TFXImporter.cpp $(IntermediateDirectory)/up_src_TFXImporter.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Gitlab/The-Forge/Common_3/Tools/AssetPipeline/src/TFXImporter.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_TFXImporter.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_TFXImporter.cpp$(DependSuffix): ../src/TFXImporter.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_TFXImporter.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_TFXImporter.cpp$(DependSuffix) -MM ../src/TFXImporter.cpp

$(IntermediateDirectory)/up_src_TFXImporter.cpp$(PreprocessSuffix): ../src/TFXImporter.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_TFXImporter.cpp$(PreprocessSuffix) ../src/TFXImporter.cpp

$(IntermediateDirectory)/up_src_TressFXAsset.cpp$(ObjectSuffix): ../src/TressFXAsset.cpp $(IntermediateDirectory)/up_src_TressFXAsset.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Gitlab/The-Forge/Common_3/Tools/AssetPipeline/src/TressFXAsset.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_TressFXAsset.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_TressFXAsset.cpp$(DependSuffix): ../src/TressFXAsset.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_TressFXAsset.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_TressFXAsset.cpp$(DependSuffix) -MM ../src/TressFXAsset.cpp

$(IntermediateDirectory)/up_src_TressFXAsset.cpp$(PreprocessSuffix): ../src/TressFXAsset.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_TressFXAsset.cpp$(PreprocessSuffix) ../src/TressFXAsset.cpp


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Debug/


