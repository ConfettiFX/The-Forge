solution "embed"
   configurations { "Debug"}
   location "build/" 
   project "embed"
      kind "ConsoleApp"
      language "C"
      files { "embed.c", "microprofile.h", "microprofile.html"}
     
      defines {"GLEW_STATIC;_CRT_SECURE_NO_WARNINGS"} 
      debugdir "."

      buildoptions {"/TP"}
      configuration "Debug"
         defines { "DEBUG" }
         flags { "Symbols", "StaticRuntime" }
