##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=SpirVTools
ConfigurationName      :=Release
WorkspacePath          :=/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite
ProjectPath            :=/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite/SpirVTools
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
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="SpirVTools.txt"
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
Objects0=$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_SpirvTools_SpirvTools.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cpp.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cfg.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_parsed_ir.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_util.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_glsl.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_hlsl.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_msl.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_parser.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_reflect.cpp$(ObjectSuffix) 



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
	@echo rebuilt > "/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite/.build-release/SpirVTools"

MakeIntermediateDirs:
	@test -d ./Release || $(MakeDirCommand) ./Release


./Release:
	@test -d ./Release || $(MakeDirCommand) ./Release

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_SpirvTools_SpirvTools.cpp$(ObjectSuffix): ../../../../Common_3/Tools/SpirvTools/SpirvTools.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_Tools_SpirvTools_SpirvTools.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/Tools/SpirvTools/SpirvTools.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_SpirvTools_SpirvTools.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_SpirvTools_SpirvTools.cpp$(DependSuffix): ../../../../Common_3/Tools/SpirvTools/SpirvTools.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_SpirvTools_SpirvTools.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_SpirvTools_SpirvTools.cpp$(DependSuffix) -MM ../../../../Common_3/Tools/SpirvTools/SpirvTools.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Tools_SpirvTools_SpirvTools.cpp$(PreprocessSuffix): ../../../../Common_3/Tools/SpirvTools/SpirvTools.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_Tools_SpirvTools_SpirvTools.cpp$(PreprocessSuffix) ../../../../Common_3/Tools/SpirvTools/SpirvTools.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cpp.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cpp.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cpp.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cpp.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cpp.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cpp.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cpp.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cpp.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cpp.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cpp.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cpp.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cpp.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cpp.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cpp.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cfg.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cfg.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cfg.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cfg.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cfg.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cfg.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cfg.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cfg.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cfg.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cfg.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cfg.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cfg.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cfg.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cfg.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_parsed_ir.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_parsed_ir.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_parsed_ir.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_parsed_ir.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_parsed_ir.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_parsed_ir.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_parsed_ir.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_parsed_ir.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_parsed_ir.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_parsed_ir.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_parsed_ir.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_parsed_ir.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_parsed_ir.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_parsed_ir.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_util.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_util.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_util.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_util.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_util.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_util.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_util.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_util.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_util.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_util.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_util.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_util.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_cross_util.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_cross_util.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_glsl.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_glsl.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_glsl.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_glsl.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_glsl.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_glsl.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_glsl.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_glsl.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_glsl.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_glsl.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_glsl.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_glsl.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_glsl.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_glsl.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_hlsl.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_hlsl.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_hlsl.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_hlsl.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_hlsl.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_hlsl.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_hlsl.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_hlsl.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_hlsl.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_hlsl.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_hlsl.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_hlsl.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_hlsl.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_hlsl.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_msl.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_msl.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_msl.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_msl.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_msl.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_msl.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_msl.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_msl.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_msl.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_msl.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_msl.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_msl.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_msl.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_msl.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_parser.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_parser.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_parser.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_parser.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_parser.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_parser.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_parser.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_parser.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_parser.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_parser.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_parser.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_parser.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_parser.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_parser.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_reflect.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_reflect.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_reflect.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_reflect.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_reflect.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_reflect.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_reflect.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_reflect.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_reflect.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_reflect.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_reflect.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_reflect.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_SPIRV_Cross_spirv_reflect.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/SPIRV_Cross/spirv_reflect.cpp


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Release/


