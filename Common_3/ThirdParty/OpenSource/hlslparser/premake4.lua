solution "HLSLParser"
    location "build"
    configurations { "Debug", "Release" }
 
    project "HLSLParser"
        kind "ConsoleApp"
        language "C++"
        files { "src/**.h", "src/**.cpp" }
 
    configuration "Debug"
        targetdir "bin/debug"
        defines { "DEBUG" }
        flags { "Symbols" }
 
    configuration "Release"
        targetdir "bin/release"
        defines { "NDEBUG" }
        flags { "Optimize" }    