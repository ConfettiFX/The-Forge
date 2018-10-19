##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=ozz_base
ConfigurationName      :=Release
WorkspacePath          :=/home/confetti/Desktop/Conffx/The-Forge/Examples_3/Unit_Tests_Animation/UbuntuCodelite
ProjectPath            :=/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/Ubuntu
IntermediateDirectory  :=$(WorkspacePath)/$(ProjectName)/Release
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=Confetti
Date                   :=17/10/18
CodeLitePath           :=/home/confetti/.codelite
LinkerName             :=g++
SharedObjectLinkerName :=g++ -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.o.i
DebugSwitch            :=-gstab
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
ObjectsFileList        :="ozz_base.txt"
PCHCompileFlags        :=
MakeDirCommand         :=mkdir -p
LinkOptions            :=  -ldl -pthread 
IncludePath            :=  $(IncludeSwitch)$(WorkspacePath)/../../../Common_3/ThirdParty/OpenSource/ozz-animation/include $(IncludeSwitch). $(IncludeSwitch)$(ProjectPath)/../.. $(IncludeSwitch)$(VULKAN_SDK)/include/ 
IncludePCH             := 
RcIncludePath          := 
Libs                   := 
ArLibs                 :=  
LibPath                := $(LibraryPathSwitch). $(LibraryPathSwitch)$(WorkspacePath)/OSBase/Release/ 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := ar rcus
CXX      := g++
CC       := gcc
CXXFLAGS :=  -O2 -std=c++11 -Wall  $(Preprocessors)
CFLAGS   :=  -O2 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/up_src_base_containers_string_archive.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_base_io_stream.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_base_memory_allocator.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_base_maths_simd_math_archive.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_base_maths_simd_math.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_base_platform.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_base_maths_box.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_base_io_archive.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_base_log.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_base_maths_math_archive.cc$(ObjectSuffix) \
	$(IntermediateDirectory)/up_src_base_maths_soa_math_archive.cc$(ObjectSuffix) 



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
	@$(MakeDirCommand) "/home/confetti/Desktop/Conffx/The-Forge/Examples_3/Unit_Tests_Animation/UbuntuCodelite/.build-release"
	@echo rebuilt > "/home/confetti/Desktop/Conffx/The-Forge/Examples_3/Unit_Tests_Animation/UbuntuCodelite/.build-release/ozz_base"

MakeIntermediateDirs:
	@test -d $(WorkspacePath)/$(ProjectName)/Release || $(MakeDirCommand) $(WorkspacePath)/$(ProjectName)/Release


$(WorkspacePath)/$(ProjectName)/Release:
	@test -d $(WorkspacePath)/$(ProjectName)/Release || $(MakeDirCommand) $(WorkspacePath)/$(ProjectName)/Release

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/up_src_base_containers_string_archive.cc$(ObjectSuffix): ../src/base/containers/string_archive.cc $(IntermediateDirectory)/up_src_base_containers_string_archive.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/containers/string_archive.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_containers_string_archive.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_containers_string_archive.cc$(DependSuffix): ../src/base/containers/string_archive.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_containers_string_archive.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_containers_string_archive.cc$(DependSuffix) -MM ../src/base/containers/string_archive.cc

$(IntermediateDirectory)/up_src_base_containers_string_archive.cc$(PreprocessSuffix): ../src/base/containers/string_archive.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_containers_string_archive.cc$(PreprocessSuffix) ../src/base/containers/string_archive.cc

$(IntermediateDirectory)/up_src_base_io_stream.cc$(ObjectSuffix): ../src/base/io/stream.cc $(IntermediateDirectory)/up_src_base_io_stream.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/io/stream.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_io_stream.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_io_stream.cc$(DependSuffix): ../src/base/io/stream.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_io_stream.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_io_stream.cc$(DependSuffix) -MM ../src/base/io/stream.cc

$(IntermediateDirectory)/up_src_base_io_stream.cc$(PreprocessSuffix): ../src/base/io/stream.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_io_stream.cc$(PreprocessSuffix) ../src/base/io/stream.cc

$(IntermediateDirectory)/up_src_base_memory_allocator.cc$(ObjectSuffix): ../src/base/memory/allocator.cc $(IntermediateDirectory)/up_src_base_memory_allocator.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/memory/allocator.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_memory_allocator.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_memory_allocator.cc$(DependSuffix): ../src/base/memory/allocator.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_memory_allocator.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_memory_allocator.cc$(DependSuffix) -MM ../src/base/memory/allocator.cc

$(IntermediateDirectory)/up_src_base_memory_allocator.cc$(PreprocessSuffix): ../src/base/memory/allocator.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_memory_allocator.cc$(PreprocessSuffix) ../src/base/memory/allocator.cc

$(IntermediateDirectory)/up_src_base_maths_simd_math_archive.cc$(ObjectSuffix): ../src/base/maths/simd_math_archive.cc $(IntermediateDirectory)/up_src_base_maths_simd_math_archive.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/maths/simd_math_archive.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_maths_simd_math_archive.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_maths_simd_math_archive.cc$(DependSuffix): ../src/base/maths/simd_math_archive.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_maths_simd_math_archive.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_maths_simd_math_archive.cc$(DependSuffix) -MM ../src/base/maths/simd_math_archive.cc

$(IntermediateDirectory)/up_src_base_maths_simd_math_archive.cc$(PreprocessSuffix): ../src/base/maths/simd_math_archive.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_maths_simd_math_archive.cc$(PreprocessSuffix) ../src/base/maths/simd_math_archive.cc

$(IntermediateDirectory)/up_src_base_maths_simd_math.cc$(ObjectSuffix): ../src/base/maths/simd_math.cc $(IntermediateDirectory)/up_src_base_maths_simd_math.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/maths/simd_math.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_maths_simd_math.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_maths_simd_math.cc$(DependSuffix): ../src/base/maths/simd_math.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_maths_simd_math.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_maths_simd_math.cc$(DependSuffix) -MM ../src/base/maths/simd_math.cc

$(IntermediateDirectory)/up_src_base_maths_simd_math.cc$(PreprocessSuffix): ../src/base/maths/simd_math.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_maths_simd_math.cc$(PreprocessSuffix) ../src/base/maths/simd_math.cc

$(IntermediateDirectory)/up_src_base_platform.cc$(ObjectSuffix): ../src/base/platform.cc $(IntermediateDirectory)/up_src_base_platform.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/platform.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_platform.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_platform.cc$(DependSuffix): ../src/base/platform.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_platform.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_platform.cc$(DependSuffix) -MM ../src/base/platform.cc

$(IntermediateDirectory)/up_src_base_platform.cc$(PreprocessSuffix): ../src/base/platform.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_platform.cc$(PreprocessSuffix) ../src/base/platform.cc

$(IntermediateDirectory)/up_src_base_maths_box.cc$(ObjectSuffix): ../src/base/maths/box.cc $(IntermediateDirectory)/up_src_base_maths_box.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/maths/box.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_maths_box.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_maths_box.cc$(DependSuffix): ../src/base/maths/box.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_maths_box.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_maths_box.cc$(DependSuffix) -MM ../src/base/maths/box.cc

$(IntermediateDirectory)/up_src_base_maths_box.cc$(PreprocessSuffix): ../src/base/maths/box.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_maths_box.cc$(PreprocessSuffix) ../src/base/maths/box.cc

$(IntermediateDirectory)/up_src_base_io_archive.cc$(ObjectSuffix): ../src/base/io/archive.cc $(IntermediateDirectory)/up_src_base_io_archive.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/io/archive.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_io_archive.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_io_archive.cc$(DependSuffix): ../src/base/io/archive.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_io_archive.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_io_archive.cc$(DependSuffix) -MM ../src/base/io/archive.cc

$(IntermediateDirectory)/up_src_base_io_archive.cc$(PreprocessSuffix): ../src/base/io/archive.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_io_archive.cc$(PreprocessSuffix) ../src/base/io/archive.cc

$(IntermediateDirectory)/up_src_base_log.cc$(ObjectSuffix): ../src/base/log.cc $(IntermediateDirectory)/up_src_base_log.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/log.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_log.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_log.cc$(DependSuffix): ../src/base/log.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_log.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_log.cc$(DependSuffix) -MM ../src/base/log.cc

$(IntermediateDirectory)/up_src_base_log.cc$(PreprocessSuffix): ../src/base/log.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_log.cc$(PreprocessSuffix) ../src/base/log.cc

$(IntermediateDirectory)/up_src_base_maths_math_archive.cc$(ObjectSuffix): ../src/base/maths/math_archive.cc $(IntermediateDirectory)/up_src_base_maths_math_archive.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/maths/math_archive.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_maths_math_archive.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_maths_math_archive.cc$(DependSuffix): ../src/base/maths/math_archive.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_maths_math_archive.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_maths_math_archive.cc$(DependSuffix) -MM ../src/base/maths/math_archive.cc

$(IntermediateDirectory)/up_src_base_maths_math_archive.cc$(PreprocessSuffix): ../src/base/maths/math_archive.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_maths_math_archive.cc$(PreprocessSuffix) ../src/base/maths/math_archive.cc

$(IntermediateDirectory)/up_src_base_maths_soa_math_archive.cc$(ObjectSuffix): ../src/base/maths/soa_math_archive.cc $(IntermediateDirectory)/up_src_base_maths_soa_math_archive.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/base/maths/soa_math_archive.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_base_maths_soa_math_archive.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_base_maths_soa_math_archive.cc$(DependSuffix): ../src/base/maths/soa_math_archive.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_base_maths_soa_math_archive.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_base_maths_soa_math_archive.cc$(DependSuffix) -MM ../src/base/maths/soa_math_archive.cc

$(IntermediateDirectory)/up_src_base_maths_soa_math_archive.cc$(PreprocessSuffix): ../src/base/maths/soa_math_archive.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_base_maths_soa_math_archive.cc$(PreprocessSuffix) ../src/base/maths/soa_math_archive.cc


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r $(WorkspacePath)/$(ProjectName)/Release/


