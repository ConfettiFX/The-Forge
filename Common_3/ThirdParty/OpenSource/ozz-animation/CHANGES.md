Release version 0.10.0
----------------------

* Library
  - [animation] Adds user-channel feature #4. ozz now offers tracks of float, float2, float3, float4 and quaternion for both raw/offline and runtime. A track can be used to store animated user-data, aka data that aren't joint transformations. Runtime jobs allow to query a track value for any time t (ozz::animation::TrackSamplingJob), or to find all rising and falling edges that happened during a period of time (ozz::animation::TrackTriggeringJob). Utilities allow to optimize a raw track (ozz::animation::offline::TrackOptimizer) and build a runtime track (ozz::animation::offline::TrackOptimizer). fbx2ozz comes with the ability to import tracks from fbx node properties.
  - [animation] Changed ozz::animation::SamplingJob::time (in interval [0,duration]) to a ratio (in unit interval [0,1]). This is a breaking change, aiming to unify sampling of animations and tracks. To conform with this change, sampling time should simply be divided by animation duration. ozz::sample:AnimationController has been updated accordingly. Offline animations and tools aren't impacted.
  - [base] Changes non-intrusive serialization mechanism to use a specialize template struct "Extern" instead of function overloading.

* Tools
  - Merged \*2skel and \*2anim in a single tool (\*2ozz, fbx2ozz for fbx importer) where all options are specified as a json config file. List of options with default values are available in [src/animation/offline/tools/reference.json](src/animation/offline/tools/reference.json) file. Consequently, ozz_animation_offline_skel_tools and ozz_animation_offline_anim_tools are also merged into a single ozz_animation_tools library.
  - Adds options to import user-channel tracks (from node properties for fbx) using json "animations[].tracks[].properties[]" definition.
  - Adds an option while importing skeletons to choose scene node types that must be considered as skeleton joints, ie not restricting to scene joints only. This is useful for the baked sample for example, which animates mesh nodes.

* Build pipeline
  - ozz optionnaly supports c++11 compiler.
  - Adds ozz_build_data option (OFF by default), to avoid building data on every code change. Building data takes time indeed, and isn't required on every change. It should be turned ON when output format changes to update all data again.
  - Removes fused source files from the depot. Fused files are generated during build to ${PROJECT_BINARY_DIR}/src_fused/ folder. To generate fused source files without building the whole project, then build BUILD_FUSE_ALL target with "cmake --build . --target BUILD_FUSE_ALL" command.
  - Adds support for Visual Studio 15 2017, drops Visual Studio 11 2012.

* Samples
  - [user_channel] Adds new user-channel sample, demonstrating usage of user-channel tracks API and import pipeline usage.
  - [sample_fbx2mesh] Remaps joint indices to the smaller range of skeleton joints that are actually used by the skinning. It's now required to index skeleton matrices using ozz::sample::framework:Mesh::joint_remaps when build skinning matrices.
  - [multithread] Switched from OpenMP to c++11 std::async API to implement a parallel-for loop over all computation tasks.
  
Release version 0.9.1
---------------------

* Build pipeline
  - Allows to use ozz-animation from another project using cmake add_subdirectory() command, conforming with [online documentation](http://guillaumeblanc.github.io/ozz-animation/documentation/build/).
  - Adds Travis-CI (http://travis-ci.org/guillaumeblanc/ozz-animation) and AppVeyor (http://ci.appveyor.com/project/guillaumeblanc/ozz-animation) continuous integration support.
  - Exposes MSVC /MD and /MT option (ozz_build_msvc_rt_dll). Default is /MD, same as MSVC/cmake.
  - Adds support for Xcode 8.3.2 (fbx specific compilation option).

Release version 0.9.0
---------------------

* Library
  - [offline] Removes dae2* tools, offline libraries and dependencies. Collada is still supported through fbx2* tools suite, as they are based on the Autodesk Fbx sdk.
  - [offline][animation] Adds a name to the offline::RawAnimation and Animation data structure. ozz::animation::Animation serialization format has changed, animations generated with a previous version need to be re-built.
  - [animation] Optimizes animation and skeleton allocation strategy, merging all member buffers to a single allocation.
  - [animation] Fixes memory read overrun in ozz::animation::Skeleton while fixing up skeleton joint names.
  - [offline] #5 Allows importing of all animations from a DCC file with a single command. fbx2anim now support the use of the wildcard character '*' in the --animation option (output file name), which is replaced with the imported animation names when the output file is written to disk.
  - [offline] Uses scene frame rate as the default sampling rate option in fbx2anim. Allows to match DCC keys and avoid interpolation issues while importing from fbx sdk.
  - [base] Adds support for Range serialization (as an array) via ozz::io::MakeArray utility.
  - [base] Fixes SSE matrix decomposition implementation which wasn't able to decompose matrices with very small scales.

* Build pipeline
  - Rework build pipeline and media directory to allow to have working samples even if Fbx sdk isn't available to build fbx tools. Media directory now contains pre-built binary data for samples usage.
  - A Fused version of the sources for all libraries can be found in src_fused folder. It is automatically generated by cmake when any library source file changes.
  - #20 FindFbx.cmake module now supports a version as argument which can be tested in conformance with cmake find_package specifications.
  - Adds a clang-format target that re-formats all sdk sources. This target is not added to the ALL_BUILD target. It must be ran manually.
  - Removes forced MSVC compiler options (SSE, RTC, GS) that are already properly handled by default.

* Samples
  - [baked] Adds new baked physic sample. Demonstrates how to modify Fbx skeleton importer to build skeletons from custom nodes, and use an animation to drive rigid bodies.
  - [sample_fbx2mesh] Fixes welding of redundant vertices. Re-imported meshes now have significantly less vertices.
  - [sample_fbx2mesh] Adds UVs, tangents and vertex color to ozz::sample::framework::Mesh structure. Meshes generated with a previous version need to be re-built.
  - [framework] Fixes sample first frame time, setting aside time spent initializing.
  - [framework] Supports emscripten webgl canvas resizing.

Release version 0.8.0
---------------------
 
* Library
  - [animation] Adds additive blending support to ozz::animation::BlendingJob. Animations used for additive blending should be delta animations (relative to the first frame). Use ozz::animation::offline::AdditiveAnimationBuilder to prepare such animations.
  - [animation] Improves quaternion compression scheme by quantizing the 3 smallest components of the quaternion, instead of the firsts 3. This improves numerical accuracy when the restored component (4th) is small. It also allows to pre-multiply each of the 3 smallest components by sqrt(2), maximizing quantization range by over 41%.
  - [offline] Improves animation optimizer process (ozz::animation::offline::AnimationOptimizer) with a new hierarchical translation tolerance. The optimizer now computes the error (a distance) generated from the optimization of a joint on its whole child hierarchy (like the whole arm length and hand when optimizing the shoulder). This provides a better optimization in both quality and quantity.
  - [offline] Adds ozz::animation::offline::AdditiveAnimationBuilder utility to build delta animations that can be used for additive blending. This utility processes a raw animation to calculate the delta transformation from the first key to all subsequent ones, for all tracks.
  - [offline] Adds --additive option to dae2anim and fbx2anim, allowing to output a delta animation suitable for additive blending.
  - [offline] Adds fbx 20161.* sdk support.

* Build pipeline
  - Adds c++11 build option for gcc/clang compilers. Use cmake ozz_build_cpp11 option.
  - Automatically detects SIMD implementation based on compiler settings. SSE2 implementation is automatically enabled on x64/amd64 targets, or if /arch:SSE2 is selected on MSVC/x86 builds. One could use ozz_build_simd_ref cmake option (OZZ_BUILD_SIMD_REF preprocessor directive) to bypass detection and force reference (aka scalar) implementation. OZZ_HAS_SSE2 is now deprecated.
  - Fixes #3 gcc5 warnings with simd math reference implementation.
  - Fixes #6 by updating to gtest 1.70 to support new platforms (FreeBSD...).
  - Adds Microsoft Visual Studio 14 2015 support.
  - Adds emscripten 1.35 support.
  - Integrate Coverity static analysis (https://scan.coverity.com/projects/guillaumeblanc-ozz-animation).

* Samples
  - [additive] Adds an additive blending sample which demonstrates the new additive layers available through the ozz::animation::BlendingJob.
  - [optimize] Adds hierarchical translation tolerance parameter to the optimize sample.
  - [skin] Removes sample skin, as from now on skinning is part of the sample framework and used by other samples. See additive sample.

Release version 0.7.3
---------------------

* Changes license to the MIT License (MIT).

Release version 0.7.2
---------------------

* Library
  - [animation] Improves rotations accuracy during animation sampling and blending. Quaternion normalization now uses one more Newton-Raphson step when computing inverse square root.
  - [offline] Fixes fbx animation duration when take duration is different to timeline length.
  - [offline] Bakes fbx axis/unit systems to all transformations, vertices, animations (and so on...), instead of using fbx sdk ConvertScene strategy which only transform scene root. This allows to mix skeleton, animation and meshes imported from any axis/unit system.
  - [offline] Uses bind-pose transformation, instead of identity, for skeleton joints that are not animated in a fbx file.
  - [offline] Adds support to ozz fbx tools (fbx2skel and fbx2anim) for other formats: Autodesk AutoCAD DXF (.dxf), Collada DAE (.dae), 3D Studio 3DS (.3ds)  and Alias OBJ (.obj). This toolchain based on fbx sdk will replace dae toolchain (dae2skel and dae2anim) which is now deprecated.

* HowTos
  - Adds file loading how-to, which demonstrates how to open a file and deserialize an object with ozz archive library.
  - Adds custom skeleton importer how-to, which demonstrates RawSkeleton setup and conversion to runtime skeleton.
  - Adds custom animation importer how-to, which demonstrates RawAnimation setup and conversion to runtime animation.

* Samples
  - [skin] Skin sample now uses the inverse bind-pose matrices imported from the mesh file, instead of the one computed from the skeleton. This is a more robust solution, which furthermore allow to share the skeleton with meshes using different bind-poses.
  - [skin] Fixes joints weight normalization when importing fbx skin meshes.
  - [framework] Optimizes dynamic vertex buffer object re-allocation strategy.

Release version 0.7.1
---------------------

* Library
  - [offline] Updates to fbx sdk 2015.1 with vs2013 support.
  
* Samples
  - Adds sample_skin_fbx2skin to binary packages.

Release version 0.7.0
---------------------

* Library
  - [geometry] Adds support for matrix palette skinning transformation in a new ozz_geometry library.
  - [offline] Returns EXIT_FAILURE from dae2skel and fbx2skel when no skeleton found in the source file.
  - [offline] Fixes fbx axis and unit system conversions.
  - [offline] Removes raw_skeleton_archive.h and raw_animation_archive.h files, moving serialization definitions back to raw_skeleton.h and raw_animation.h to simplify understanding.
  - [offline] Removes skeleton_archive.h and animation_archive.h files, moving serialization definitions back to skeleton.h and animation.h.
  - [base] Changes Range<>::Size() definition, returning range's size in bytes instead of element count.

* Samples
  - Adds a skinning sample which demonstrates new ozz_geometry SkinningJob feature and usage.

Release version 0.6.0
---------------------

* Library
  - [animation] Compresses animation key frames memory footprint. Rotation key frames are compressed from 24B to 12B (50%). 3 of the 4 components of the quaternion are quantized to 2B each, while the 4th is restored during sampling. Translation and scale components are compressed to half float, reducing their size from 20B to 12B (40%).
  - [animation] Changes runtime::Animation class serialization format to support compression. Serialization retro-compatibility for this class has not been implemented, meaning that all runtime::Animation must be rebuilt and serialized using usual dae2anim, fbx2anim or using offline::AnimationBuilder utility.
  - [base] Adds float-to-half and half-to-float conversion functions to simd math library.

Release version 0.5.0
---------------------

* Library
  - [offline] Adds --raw option to *2skel and *2anim command line tools. Allows to export raw skeleton/animation object format instead of runtime objects.
  - [offline] Moves RawAnimation and RawSkeleton from the builder files to raw_animation.h and raw_skeleton.h files.
  - [offline] Renames skeleton_serialize.h and animation_serialize.h to skeleton_archive.h and animation_archive.h for consistency.
  - [offline] Adds RawAnimation and RawSkeleton serialization support with ozz archives.
  - [options] Changes parser command line arguments type to "const char* const*" in order to support implicit casting from arguments of type "char**".
  - [base] Change ozz::String std redirection from typedef to struct to be coherent with all other std containers redirection.
  - [base] Moves maths archiving file from ozz/base/io to ozz/base/maths for consistency.
  - [base] Adds containers serialization support with ozz archives.
  - [base] Removes ozz fixed size integers in favor of standard types available with <stdint.h> file.

* Samples
  - Adds Emscripten support to all supported samples.
  - Changes OpenGL rendering to comply with Gles2/WebGL.

* Build pipeline
  - Adds Emscripten and cross-compilation support to the builder helper python script.
  - Support for CMake 3.x.
  - Adds support for Microsoft Visual Studio 2013.
  - Drops support for Microsoft Visual Studio 2008 and olders, as a consequence of using <stdint.h>.

Release version 0.4.0
---------------------

* Library
  - [offline] Adds Fbx import pipeline, through fbx2skel and fbx2anim command line tools.
  - [offline] Adds Fbx import and conversion library, through ozz_animation_fbx. Building fbx related libraries requires fbx sdk to be installed.
  - [offline] Adds ozz_animation_offline_tools library to share the common work for Collada and Fbx import tools. This could be use to implement custom conversion command line tools.

* Samples
  - Adds Fbx resources to media path.
  - Makes use of Fbx resources with existing samples.

Release version 0.3.1
---------------------

* Samples
  - Adds keyboard camera controls.

* Build pipeline
  - Adds Mac OSX support, full offline and runtime pipeline, samples, dashboard...
  - Moves dashboard to http://ozz.qualipilote.fr/dashboard/cdash/
  - Improves dashboard configuration, using json configuration files available there: http://ozz.qualipilote.fr/dashboard/config/.

Release version 0.3.0
---------------------

* Library
  - [animation] Adds partial animation blending and masking, through per-joint-weight blending coefficients.
  - [animation] Switches all explicit [begin,end[ ranges (sequence of objects) to ozz::Range structure.
  - [animation] Moves runtime files (.h and .cc) to a separate runtime folder (ozz/animation/runtime).
  - [animation] Removes ozz/animation/utils.h and .cc
  - [options] Detects duplicated command line arguments and reports failure. 
  - [base] Adds helper functions to ozz::memory::Allocator to support allocation/reallocation/deallocation of ranges of objects through ozz::Range structure.

* Samples
  - Adds partial animation blending sample.
  - Adds multi-threading sample, using OpenMp to distribute workload.
  - Adds a sample that demonstrates how to attach an object to animated skeleton joints.
  - Improves skeleton rendering sample utility feature: includes joint rendering.
  - Adds screen-shot and video capture options from samples ui.
  - Adds a command line option (--render/--norender) to enable/disable rendering of sample, used for dashboard unit-tests.
  - Adds time management options, to dissociate (fix) update delta time from the real application time.
  - Improves camera manipulations: disables auto-framing when zooming/panning, adds mouse wheel support for zooming.
  - Fixes sample camera framing to match rendering field of view.

* Build pipeline
  - Adds CMake python helper tools (build-helper.py). Removes helper .bat files (setup, build, pack...).
  - Adds CDash support to generate nightly build reports. Default CDash server is http://my.cdash.org/index.php?project=Ozz.
  - Adds code coverage testing support using gcov.

Release version 0.2.0
---------------------

* Library
  - [animation] Adds animation blending support.
  - [animation] Sets maximum skeleton joints to 1023 (aka Skeleton::kMaxJointsNumBits) to improve packing and allow stack allocations.
  - [animation] Adds Skeleton::kRootIndex enum for parent index of a root joint.
  - [base] Adds signed/unsigned bit shift functions to simd library.
  - [base] Fixes SSE build flags for Visual Studio 64b builds.

* Samples
  - Adds blending sample.
  - Adds playback controller utility class to the sample framework.

Release version 0.1.0, initial open source release
--------------------------------------------------

* Library
  - [animation] Support for run-time animation sampling.
  - [offline] Support for building run-time animation and skeleton from a raw (aka offline/user friendly) format.
  - [offline] Full Collada import pipeline (lib and tools).
  - [offline]  Support for animation key-frame optimizations.
  - [base] Memory management redirection.
  - [base] Containers definition.
  - [base] Serialization and IO utilities implementation.
  - [base] scalar and vector maths, SIMD (SSE) and SoA implementation.
  - [options] Command line parsing utility.

* Samples
  - Playback sample, loads and samples an animation.
  - Millipede sample, offline pipeline usage.
  - Optimize sample, offline optimization pipeline.

* Build pipeline
  - CMake based build pipeline.
  - CTest/GTest based unit test framework.