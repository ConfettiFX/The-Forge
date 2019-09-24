##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Release
ProjectName            :=Renderer
ConfigurationName      :=Release
WorkspacePath          :=/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite
ProjectPath            :=/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite/Renderer
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
ObjectsFileList        :="Renderer.txt"
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
CXXFLAGS :=  -std=c++14   $(Preprocessors)
CFLAGS   :=   $(Preprocessors)
ASFLAGS  := 
AS       := /usr/bin/as


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects0=$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_CommonShaderReflection.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_ResourceLoader.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_Vulkan.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanRaytracing.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanShaderReflection.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Image_Image.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_basis_universal_transcoder_basisu_transcoder.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerBase.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_Profiler.cpp$(ObjectSuffix) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_GpuProfiler.cpp$(ObjectSuffix) \
	$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerWidgetsUI.cpp$(ObjectSuffix) 



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
	@echo rebuilt > "/home/confetti/Desktop/Devansh/The-Forge/Examples_3/Visibility_Buffer/UbuntuCodelite/.build-release/Renderer"

MakeIntermediateDirs:
	@test -d ./Release || $(MakeDirCommand) ./Release


./Release:
	@test -d ./Release || $(MakeDirCommand) ./Release

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_CommonShaderReflection.cpp$(ObjectSuffix): ../../../../Common_3/Renderer/CommonShaderReflection.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_CommonShaderReflection.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/Renderer/CommonShaderReflection.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_CommonShaderReflection.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_CommonShaderReflection.cpp$(DependSuffix): ../../../../Common_3/Renderer/CommonShaderReflection.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_CommonShaderReflection.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_CommonShaderReflection.cpp$(DependSuffix) -MM ../../../../Common_3/Renderer/CommonShaderReflection.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_CommonShaderReflection.cpp$(PreprocessSuffix): ../../../../Common_3/Renderer/CommonShaderReflection.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_CommonShaderReflection.cpp$(PreprocessSuffix) ../../../../Common_3/Renderer/CommonShaderReflection.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_ResourceLoader.cpp$(ObjectSuffix): ../../../../Common_3/Renderer/ResourceLoader.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_ResourceLoader.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/Renderer/ResourceLoader.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_ResourceLoader.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_ResourceLoader.cpp$(DependSuffix): ../../../../Common_3/Renderer/ResourceLoader.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_ResourceLoader.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_ResourceLoader.cpp$(DependSuffix) -MM ../../../../Common_3/Renderer/ResourceLoader.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_ResourceLoader.cpp$(PreprocessSuffix): ../../../../Common_3/Renderer/ResourceLoader.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_ResourceLoader.cpp$(PreprocessSuffix) ../../../../Common_3/Renderer/ResourceLoader.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_Vulkan.cpp$(ObjectSuffix): ../../../../Common_3/Renderer/Vulkan/Vulkan.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_Vulkan.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/Renderer/Vulkan/Vulkan.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_Vulkan.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_Vulkan.cpp$(DependSuffix): ../../../../Common_3/Renderer/Vulkan/Vulkan.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_Vulkan.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_Vulkan.cpp$(DependSuffix) -MM ../../../../Common_3/Renderer/Vulkan/Vulkan.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_Vulkan.cpp$(PreprocessSuffix): ../../../../Common_3/Renderer/Vulkan/Vulkan.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_Vulkan.cpp$(PreprocessSuffix) ../../../../Common_3/Renderer/Vulkan/Vulkan.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanRaytracing.cpp$(ObjectSuffix): ../../../../Common_3/Renderer/Vulkan/VulkanRaytracing.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanRaytracing.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/Renderer/Vulkan/VulkanRaytracing.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanRaytracing.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanRaytracing.cpp$(DependSuffix): ../../../../Common_3/Renderer/Vulkan/VulkanRaytracing.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanRaytracing.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanRaytracing.cpp$(DependSuffix) -MM ../../../../Common_3/Renderer/Vulkan/VulkanRaytracing.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanRaytracing.cpp$(PreprocessSuffix): ../../../../Common_3/Renderer/Vulkan/VulkanRaytracing.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanRaytracing.cpp$(PreprocessSuffix) ../../../../Common_3/Renderer/Vulkan/VulkanRaytracing.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanShaderReflection.cpp$(ObjectSuffix): ../../../../Common_3/Renderer/Vulkan/VulkanShaderReflection.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanShaderReflection.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/Renderer/Vulkan/VulkanShaderReflection.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanShaderReflection.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanShaderReflection.cpp$(DependSuffix): ../../../../Common_3/Renderer/Vulkan/VulkanShaderReflection.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanShaderReflection.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanShaderReflection.cpp$(DependSuffix) -MM ../../../../Common_3/Renderer/Vulkan/VulkanShaderReflection.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanShaderReflection.cpp$(PreprocessSuffix): ../../../../Common_3/Renderer/Vulkan/VulkanShaderReflection.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_Vulkan_VulkanShaderReflection.cpp$(PreprocessSuffix) ../../../../Common_3/Renderer/Vulkan/VulkanShaderReflection.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Image_Image.cpp$(ObjectSuffix): ../../../../Common_3/OS/Image/Image.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Image_Image.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/OS/Image/Image.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Image_Image.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Image_Image.cpp$(DependSuffix): ../../../../Common_3/OS/Image/Image.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Image_Image.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Image_Image.cpp$(DependSuffix) -MM ../../../../Common_3/OS/Image/Image.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_OS_Image_Image.cpp$(PreprocessSuffix): ../../../../Common_3/OS/Image/Image.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_OS_Image_Image.cpp$(PreprocessSuffix) ../../../../Common_3/OS/Image/Image.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_basis_universal_transcoder_basisu_transcoder.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_basis_universal_transcoder_basisu_transcoder.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_basis_universal_transcoder_basisu_transcoder.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_basis_universal_transcoder_basisu_transcoder.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_basis_universal_transcoder_basisu_transcoder.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_basis_universal_transcoder_basisu_transcoder.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_basis_universal_transcoder_basisu_transcoder.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_basis_universal_transcoder_basisu_transcoder.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerBase.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerBase.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerBase.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerBase.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerBase.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerBase.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerBase.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerBase.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerBase.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerBase.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerBase.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerBase.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerBase.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerBase.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_Profiler.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/Profiler.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_Profiler.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/MicroProfile/Profiler.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_Profiler.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_Profiler.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/Profiler.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_Profiler.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_Profiler.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/Profiler.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_Profiler.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/Profiler.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_Profiler.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/Profiler.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_GpuProfiler.cpp$(ObjectSuffix): ../../../../Common_3/Renderer/GpuProfiler.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_GpuProfiler.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/Renderer/GpuProfiler.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_GpuProfiler.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_GpuProfiler.cpp$(DependSuffix): ../../../../Common_3/Renderer/GpuProfiler.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_GpuProfiler.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_GpuProfiler.cpp$(DependSuffix) -MM ../../../../Common_3/Renderer/GpuProfiler.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_GpuProfiler.cpp$(PreprocessSuffix): ../../../../Common_3/Renderer/GpuProfiler.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_Renderer_GpuProfiler.cpp$(PreprocessSuffix) ../../../../Common_3/Renderer/GpuProfiler.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerWidgetsUI.cpp$(ObjectSuffix): ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerWidgetsUI.cpp $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerWidgetsUI.cpp$(DependSuffix)
	$(CXX) $(IncludePCH) $(SourceSwitch) "/home/confetti/Desktop/Devansh/The-Forge/Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerWidgetsUI.cpp" $(CXXFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerWidgetsUI.cpp$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerWidgetsUI.cpp$(DependSuffix): ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerWidgetsUI.cpp
	@$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerWidgetsUI.cpp$(ObjectSuffix) -MF$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerWidgetsUI.cpp$(DependSuffix) -MM ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerWidgetsUI.cpp

$(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerWidgetsUI.cpp$(PreprocessSuffix): ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerWidgetsUI.cpp
	$(CXX) $(CXXFLAGS) $(IncludePCH) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/up_up_up_up_Common_3_ThirdParty_OpenSource_MicroProfile_ProfilerWidgetsUI.cpp$(PreprocessSuffix) ../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerWidgetsUI.cpp


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Release/


