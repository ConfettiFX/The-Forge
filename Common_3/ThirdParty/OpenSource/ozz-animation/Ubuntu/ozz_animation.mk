##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=ozz_animation
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
ObjectsFileList        :="ozz_animation.txt"
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
Objects0=$(IntermediateDirectory)/up_src_animation_runtime_skeleton_utils.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_animation_runtime_skeleton.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_animation_runtime_blending_job.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_animation_runtime_track_sampling_job.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_animation_runtime_local_to_model_job.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_animation_runtime_track_triggering_job.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_animation_runtime_animation.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_animation_runtime_sampling_job.cc$(ObjectSuffix) $(IntermediateDirectory)/up_src_animation_runtime_track.cc$(ObjectSuffix) 



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
	@echo rebuilt > "/home/confetti/Desktop/Conffx/The-Forge/Examples_3/Unit_Tests_Animation/UbuntuCodelite/.build-release/ozz_animation"

MakeIntermediateDirs:
	@test -d $(WorkspacePath)/$(ProjectName)/Release || $(MakeDirCommand) $(WorkspacePath)/$(ProjectName)/Release


$(WorkspacePath)/$(ProjectName)/Release:
	@test -d $(WorkspacePath)/$(ProjectName)/Release || $(MakeDirCommand) $(WorkspacePath)/$(ProjectName)/Release

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/up_src_animation_runtime_skeleton_utils.cc$(ObjectSuffix): ../src/animation/runtime/skeleton_utils.cc $(IntermediateDirectory)/up_src_animation_runtime_skeleton_utils.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/animation/runtime/skeleton_utils.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_animation_runtime_skeleton_utils.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_animation_runtime_skeleton_utils.cc$(DependSuffix): ../src/animation/runtime/skeleton_utils.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_animation_runtime_skeleton_utils.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_animation_runtime_skeleton_utils.cc$(DependSuffix) -MM ../src/animation/runtime/skeleton_utils.cc

$(IntermediateDirectory)/up_src_animation_runtime_skeleton_utils.cc$(PreprocessSuffix): ../src/animation/runtime/skeleton_utils.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_animation_runtime_skeleton_utils.cc$(PreprocessSuffix) ../src/animation/runtime/skeleton_utils.cc

$(IntermediateDirectory)/up_src_animation_runtime_skeleton.cc$(ObjectSuffix): ../src/animation/runtime/skeleton.cc $(IntermediateDirectory)/up_src_animation_runtime_skeleton.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/animation/runtime/skeleton.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_animation_runtime_skeleton.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_animation_runtime_skeleton.cc$(DependSuffix): ../src/animation/runtime/skeleton.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_animation_runtime_skeleton.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_animation_runtime_skeleton.cc$(DependSuffix) -MM ../src/animation/runtime/skeleton.cc

$(IntermediateDirectory)/up_src_animation_runtime_skeleton.cc$(PreprocessSuffix): ../src/animation/runtime/skeleton.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_animation_runtime_skeleton.cc$(PreprocessSuffix) ../src/animation/runtime/skeleton.cc

$(IntermediateDirectory)/up_src_animation_runtime_blending_job.cc$(ObjectSuffix): ../src/animation/runtime/blending_job.cc $(IntermediateDirectory)/up_src_animation_runtime_blending_job.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/animation/runtime/blending_job.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_animation_runtime_blending_job.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_animation_runtime_blending_job.cc$(DependSuffix): ../src/animation/runtime/blending_job.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_animation_runtime_blending_job.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_animation_runtime_blending_job.cc$(DependSuffix) -MM ../src/animation/runtime/blending_job.cc

$(IntermediateDirectory)/up_src_animation_runtime_blending_job.cc$(PreprocessSuffix): ../src/animation/runtime/blending_job.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_animation_runtime_blending_job.cc$(PreprocessSuffix) ../src/animation/runtime/blending_job.cc

$(IntermediateDirectory)/up_src_animation_runtime_track_sampling_job.cc$(ObjectSuffix): ../src/animation/runtime/track_sampling_job.cc $(IntermediateDirectory)/up_src_animation_runtime_track_sampling_job.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/animation/runtime/track_sampling_job.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_animation_runtime_track_sampling_job.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_animation_runtime_track_sampling_job.cc$(DependSuffix): ../src/animation/runtime/track_sampling_job.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_animation_runtime_track_sampling_job.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_animation_runtime_track_sampling_job.cc$(DependSuffix) -MM ../src/animation/runtime/track_sampling_job.cc

$(IntermediateDirectory)/up_src_animation_runtime_track_sampling_job.cc$(PreprocessSuffix): ../src/animation/runtime/track_sampling_job.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_animation_runtime_track_sampling_job.cc$(PreprocessSuffix) ../src/animation/runtime/track_sampling_job.cc

$(IntermediateDirectory)/up_src_animation_runtime_local_to_model_job.cc$(ObjectSuffix): ../src/animation/runtime/local_to_model_job.cc $(IntermediateDirectory)/up_src_animation_runtime_local_to_model_job.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/animation/runtime/local_to_model_job.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_animation_runtime_local_to_model_job.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_animation_runtime_local_to_model_job.cc$(DependSuffix): ../src/animation/runtime/local_to_model_job.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_animation_runtime_local_to_model_job.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_animation_runtime_local_to_model_job.cc$(DependSuffix) -MM ../src/animation/runtime/local_to_model_job.cc

$(IntermediateDirectory)/up_src_animation_runtime_local_to_model_job.cc$(PreprocessSuffix): ../src/animation/runtime/local_to_model_job.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_animation_runtime_local_to_model_job.cc$(PreprocessSuffix) ../src/animation/runtime/local_to_model_job.cc

$(IntermediateDirectory)/up_src_animation_runtime_track_triggering_job.cc$(ObjectSuffix): ../src/animation/runtime/track_triggering_job.cc $(IntermediateDirectory)/up_src_animation_runtime_track_triggering_job.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/animation/runtime/track_triggering_job.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_animation_runtime_track_triggering_job.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_animation_runtime_track_triggering_job.cc$(DependSuffix): ../src/animation/runtime/track_triggering_job.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_animation_runtime_track_triggering_job.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_animation_runtime_track_triggering_job.cc$(DependSuffix) -MM ../src/animation/runtime/track_triggering_job.cc

$(IntermediateDirectory)/up_src_animation_runtime_track_triggering_job.cc$(PreprocessSuffix): ../src/animation/runtime/track_triggering_job.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_animation_runtime_track_triggering_job.cc$(PreprocessSuffix) ../src/animation/runtime/track_triggering_job.cc

$(IntermediateDirectory)/up_src_animation_runtime_animation.cc$(ObjectSuffix): ../src/animation/runtime/animation.cc $(IntermediateDirectory)/up_src_animation_runtime_animation.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/animation/runtime/animation.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_animation_runtime_animation.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_animation_runtime_animation.cc$(DependSuffix): ../src/animation/runtime/animation.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_animation_runtime_animation.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_animation_runtime_animation.cc$(DependSuffix) -MM ../src/animation/runtime/animation.cc

$(IntermediateDirectory)/up_src_animation_runtime_animation.cc$(PreprocessSuffix): ../src/animation/runtime/animation.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_animation_runtime_animation.cc$(PreprocessSuffix) ../src/animation/runtime/animation.cc

$(IntermediateDirectory)/up_src_animation_runtime_sampling_job.cc$(ObjectSuffix): ../src/animation/runtime/sampling_job.cc $(IntermediateDirectory)/up_src_animation_runtime_sampling_job.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/animation/runtime/sampling_job.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_animation_runtime_sampling_job.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_animation_runtime_sampling_job.cc$(DependSuffix): ../src/animation/runtime/sampling_job.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_animation_runtime_sampling_job.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_animation_runtime_sampling_job.cc$(DependSuffix) -MM ../src/animation/runtime/sampling_job.cc

$(IntermediateDirectory)/up_src_animation_runtime_sampling_job.cc$(PreprocessSuffix): ../src/animation/runtime/sampling_job.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_animation_runtime_sampling_job.cc$(PreprocessSuffix) ../src/animation/runtime/sampling_job.cc

$(IntermediateDirectory)/up_src_animation_runtime_track.cc$(ObjectSuffix): ../src/animation/runtime/track.cc $(IntermediateDirectory)/up_src_animation_runtime_track.cc$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Conffx/The-Forge/Common_3/ThirdParty/OpenSource/ozz-animation/src/animation/runtime/track.cc" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_src_animation_runtime_track.cc$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_src_animation_runtime_track.cc$(DependSuffix): ../src/animation/runtime/track.cc
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_src_animation_runtime_track.cc$(ObjectSuffix) -MF$(IntermediateDirectory)/up_src_animation_runtime_track.cc$(DependSuffix) -MM ../src/animation/runtime/track.cc

$(IntermediateDirectory)/up_src_animation_runtime_track.cc$(PreprocessSuffix): ../src/animation/runtime/track.cc
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_src_animation_runtime_track.cc$(PreprocessSuffix) ../src/animation/runtime/track.cc


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r $(WorkspacePath)/$(ProjectName)/Release/


