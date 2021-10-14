# ForgeShadingLanguage (FSL)
The purpose of FSL is to provide a single shader syntax from which hlsl/pssl/vk-glsl/metal shader code shader code can be generated. The syntax is largely identical to hlsl, with differences in the
shader entry and resource declarations.
Whenever possible we make use of simple macros. For more complex modifications, a python script is used (Common_3/Tools/ForgeShadingLanguage/fsl.py).  
Therefore python 3.6 is necessary to generate the shaders.
We include a no-install python3.6 in Tools/python-3.6.0-embed-amd64.  
In the vs fsl.target custom target we prepend that path to PATH, such that the build system uses that binary.

The syntax is generally similar to hlsl, with some modifications intended to make it simpler to expand
the code as necessary.
For development we recommend to setup and use as many target compilers as possible.

* insert a note about ABS and other platforms (srts) ?

FSL supports vertex, pixel, compute and tessellation shaders (control and evaluation stages).
Entry functions are declared using the
```
VS_MAIN, PS_MAIN, CS_MAIN, TC_MAIN, TE_MAIN
```
keywords and should span a single line.  
The first statement in the main function body should be:
```
INIT_MAIN;
```
this statement will get expanded differently for each target language,
to return from the main function, the keyword
```
RETURN(); // for void main function
float4 Out = (...);
RETURN(Out); // for main function with return type
```
is used;

Here is a sample fsl pixel shader using shader IO and global resources:
```
STRUCT(VSOutput)
{
	DATA(float4, Position, SV_Position);
    DATA(float, TexCoord, TEXCOORD);
};

float4 PS_MAIN( VSOutput In )
{
    INIT_MAIN;
    float4 color = SampleTex2D(Get(uTexture0), Get(uSampler0), In.TexCoord);
    RETURN(color);
}
```

## Specialization/Function constants(Vulkan and Metal only)
These constants get baked into the micro-code during pipeline creation time so the performance is identical to using a macro without
any of the downsides of macros (too many shader variations increasing the size of the build).

Good read on Specialization constants. Same things apply to function constants on Metal
https://arm-software.github.io/vulkan_best_practice_for_mobile_developers/samples/performance/specialization_constants/specialization_constants_tutorial.html

Declared at global scope using SHADER_CONSTANT macro. Used as any regular variable after declaration

Macro arguments:
```
#define SHADER_CONSTANT(INDEX, TYPE, NAME, VALUE)
```
Example usage:
```
SHADER_CONSTANT(0, uint, gRenderMode, 0);
// Vulkan - layout (constant_id = 0) const uint gRenderMode = 0;
// Metal  - constant uint gRenderMode [[function_constant(0)]];
// Others - const uint gRenderMode = 0;

void main()
{
    // Can be used like regular variables in shader code
    if (gRenderMode == 1)
    {
        // 
    }
}
```

NOTE: Unlike Vulkan, Metal does not provide a way to initialize function constants to default value. So all required function constants need to be passed through ShaderLoadDesc/BinaryShaderDesc when creating the shader

## Matrices
FSL matrices are column major.
Matrices declared inside cbuffer, pushconstants, or structure buffers are initialized from memory in column major order.  
Explicit constructors and accessors are provided:
```
// this initializes a 3 cols by 2 rows matrix from three 2-component rows.
f3x2 M = f3x2Rows(r0, r1, r2); 
setElem(M, 0, 1, 42.0f); // sets the element at col 0, row 1 to 42
float3 col0 = getCol0(M);
float2 row1 = getRow1(M);

// create a matrix from scalars, provided in row-major order
f2x2 M = f2x2RowElems(
    0,1
    2,3);
float2 col1 = getCol1(M); // (0,2)
```

We also provide overloaded Identity constructors and helpers to initialize vectors with identical components:
```
f4x4 id = Identity();
float4 = f4(1); // float4(1,1,1,1)
```

## Shader Resources
The-Forge resources are grouped into four update frequencies:
```
UPDATE_FREQ_NONE
UPDATE_FREQ_PER_FRAME
UPDATE_FREQ_PER_BATCH
UPDATE_FREQ_PER_DRAW
```
These generally map to resource tables in the rootsignature.

Since a range of platforms require identical resource declarations per stage, we recommend placing these into
resource headers which get included by each stage source file (the declarations are not necessary if the stage uses no resources).  

Resources are declared using the CBUFFER(...), PUSH_CONSTANT(...) and RES(...) syntax.  
Resources, CBuffer and push constant elements are made available in a global resource namespace
which can be accessed from any function.  
For explicit resource placement, hlsl registers and glsl bindings need to be declared.

To access a resource, the syntax Get(resource) is used.
Texture and Buffer resources can be declared as arrays by appending the dimension to the identifier. For Metal, argument buffers are generated for an update frequency whenever a single resource is declared as an array:
```
RES(Buffer(uint), myBuffers[2], UPDATE_FREQ_NONE, b0, binding=0);
```
If any such resource declaration is active in a shader, all resource declaring
with the same update frequency get placed inside the argument buffer.

### CBuffers
The following syntax declares a CBuffer:
```
CBUFFER(Uniforms, UPDATE_FRE_NONE, b0, binding=0)
{
    DATA(f4x4, mvp, None);
};
```

### Push Constants
The following syntax declares a PushConstant:
```
PUSH_CONSTANT(PushConstants, b0)
{
    DATA(uint, index, None);
};
```

### Buffers
The following types of buffers are supported:  
Buffer, WBuffer, RWBuffer, ByteBuffer, and RWByteBuffer
```
RES(RWBuffer(MyType), myArray, UPDATE_FREQ_NONE, b0, binding=0);
```

The following atomic functions are supported:
```
// atomic add of value 42 at location 0, previous value is written to last argument
AtomicAdd(Get(uRWBuffer)[0], 0, 42, pre_val);

 // atomic load & store of value 42 at location 0
val = AtomicLoad(Get(uRWBuffer)[0]);
AtomicStore(Get(uRWBuffer)[0], 42);

AtomicMin(Get(uRWBuffer)[0], 42);
AtomicMax(Get(uRWBuffer)[0], 42);
```

### Textures
FSL texture are fundamentally split between readonly types for sampling  
* Tex#D, Tex#DArray, Tex2DMS, TexCube, Depth2D, Depth2DMS  

And read-write types:
* RTex#D (readonly), WTex#D (writeonly), RWTex#D (read-write)  

Sampling types map to hlsl `Texture#D` types, glsl `texture#D` and metal `texture#d<T, access::sample>` types.  
Read-Write types map to hlsl `RWTexture#D` types, glsl `image#D` types and metal `texture#d<T, access::read_write>` types.

Sampling is performed using `SampleTex#` functions.  
Load access is performed using `LoadTex#` functions for sampling types,
and `LoadRWTex#` for read-write types.  
Writing is performed using `Write#D` functions.

An example, sampling from a cube texture and writing the result to an RW texture2D array:
```
RES(TexCube(float4), srcTexture, UPDATE_FREQ_NONE, t0, binding = 0);
RES(RWTex2DArray(float4), dstTexture, UPDATE_FREQ_NONE, u2, binding = 2);
RES(SamplerState, skyboxSampler, UPDATE_FREQ_NONE, s3, binding = 3);
(...)
float4 value = SampleLvlTexCube(Get(srcTexture), Get(skyboxSampler), float3(1,0,0), 0);
Write3D(Get(dstTexture), int3(0,0,0), value); // write to texel (0,0) of slice 0.
```

For loading functions, the sampler argument can also be `NO_SAMPLER`,
though for Vulkan `GL_EXT_samplerless_texture_functions` is necessary (its gets automatically enabled).

Texture dimensions can be retrieved using:
```
int2 size = GetDimensions(Get(uTexture), Get(uSampler));

// samplerless alternative
int2 size2 = GetDimensions(Get(uTexture), NO_SAMPLER);
```

## Shader IO
Shader input and output structs are declared using the following syntax:
```
STRUCT(VSInput)
{
    DATA(float4, position, SV_Position);
};
```
Such declared datatypes are then normally passen to the main function:
```
VSOutput(Out) VS_MAIN(VS_Input In)
```
The shader return variables get automatically created in the `INIT_MAIN` expansion,
and is automatically returned on a call to `RETURN`.
The following semantics are supported:
```
SV_Position
SV_VertexID
SV_InstanceID
SV_GroupID
SV_DispatchThreadID
SV_GroupThreadID
SV_GroupIndex
SV_SampleIndex
SV_PrimitiveID
SV_DomainLocation
```

For regular main inputs, the semantic is used as a case-insensitive decoration around the variable type:
```
void CS_MAIN(SV_GroupIndex(uint) groupIndex)
{...}
```

## Non Uniform Resource Index
For accessing elements of resource arrays, special syntax is necessary when the index is divergent:
```
uint index = (...);
float4 texColor = f4(0);
BeginNonUniformResourceIndex(index, 256); // 256 is the max possible index
    texColor = SampleLvlTex2D(Get(textureMaps)[index], Get(smp), uv, 0);
EndNonUniformResourceIndex();
```

For Vulkan, the enclosed block gets replaced based on the availability of the following extensions:  
* VK_EXT_DESCRIPTOR_INDEXING_EXTENSION: wraps the index inside the block with nonuniformEXT(...)
* VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING: code inside the block is left untouched
* if no extension is available, a switch construct is used

For other platforms, a loop with lane masking is being used as necessary.

## Tessellation
For Tessellation, the following syntax is provided:
```
TESS_VS_SHADER("shader.vert.fsl") // the vs which will be part of the pipeline
PATCH_CONSTANT_FUNC("ConstantHS") // name of the pcf

// declare domain, partitioning and output topology
// required in TC and TE stages
TESS_LAYOUT("quad", "integer", "triangle_ccw")

OUTPUT_CONTROL_POINTS(1)
MAX_TESS_FACTOR(10.0f)
```

For metal, each TC shader get transformed into a compute shader which:  
* calls the VS main function
* runs the TC main code
* calls the pcf function
* write the results to a buffer

## Wave Intrinsics
To enable Wave Intrisics, the keyword
```
ENABLE_WAVEOPS
```
needs to be inserted into the shader code, its location isnt relevant.  
The following intrinsics are supported:
```
ballot_t vote = WaveActiveBallot(expr);
uint numActiveLanes = CountBallot(activeLaneMask);
if (WaveIsFirstLane())
    {...}
if (WaveGetLaneIndex() == WaveGetMaxActiveIndex())
    {...}

val = WaveReadLaneFirst(val);
val = WaveActiveSum(val);
val = QuadReadAcrossX(i):
val = QuadReadAcrossX(j);
```

## Integration and python tool
FSL is integrate into our Visual Studio, XCode and CodeLite projects.
The generator tool can also be called directly:
```
usage: fsl.py [-h] -d DESTINATION -b BINARYDESTINATION
              [-l {DIRECT3D11,DIRECT3D12,METAL,ORBIS,PROSPERO,SCARLETT,VULKAN,XBOX,GLES} [{DIRECT3D11,DIRECT3D12,METAL,ORBIS,PROSPERO,SCARLETT,VULKAN,XBOX,GLES} ...]]
              [--verbose] [--compile] [--rootSignature ROOTSIGNATURE]
              fsl_input
```
If compilation is requested, the tool will attempt to locate appropirate compilers using env variables:
```
DIRECT3D11: $(FSL_COMPILER_FXC)
(if not set, will default to "C:/Program Files (x86)/Windows Kits/8.1/bin/x64/fxc.exe")
DIRECT3D12: $(FSL_COMPILER_DXC)
(if not set, will default to "The-Forge/ThirdParty/OpenSource/DirectXShaderCompiler/bin/x64/dxc.exe")
METAL:      $(FSL_COMPILER_METAL)
(if not set, will default to "'C:/Program Files/METAL Developer Tools/macos/bin/metal.exe'")
ORBIS:      $(SCE_ORBIS_SDK_DIR)/host_tools/bin/orbis-wave-psslc.exe
PROSPERO:   $(SCE_PROSPERO_SDK_DIR)/host_tools/bin/prospero-wave-psslc.exe
VULKAN:     $(VULKAN_SDK)/Bin/glslangValidator.exe
XBOX:       $(GXDKLATEST)/bin/XboxOne/dxc.exe
SCARLETT:   $(GXDKLATEST)/bin/Scarlett/dxc.exe
GLES:       (Can only be compiled during runtime)
```

### Visual Studio
A custom buid dependency is defined in `Common_3/Tools/ForgeShadingLanguage/VS/fsl.target`.
Once added to a project, any added *.fsl is assigned the `<FSLShader>` item type.
Using the project or file property pages, the target language and output directories can be configured.
### XCode
For XCode, we use a custom build rule for *.fsl resources and directly generate the metal shaders
into the the target package.
### CodeLite
For codelite we use custom makefile additions.

For further examples, please consult our Unit Test shader code.

## Additional Notes
We aimed to handle includes on our own as much as possible to reduce the need for compiler include handlers.
A notable case was dxc, where our generated shaders would compile and run just fine,
but hlsl::Exceptions were being thrown from IDxcCompiler::Compile() which originated from the clang ast parse.
