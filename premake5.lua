workspace "IV.EFLC.ColAccel"
   configurations { "Release", "Debug" }
   location "build"
   
   defines { "rsc_CompanyName=\"ThirteenAG\"" }
   defines { "rsc_LegalCopyright=\"MIT License\""} 
   defines { "rsc_FileVersion=\"1.0.0.0\"", "rsc_ProductVersion=\"1.0.0.0\"" }
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{prj.name}.asi\"" }
   defines { "rsc_FileDescription=\"https://github.com/ThirteenAG\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/IV.EFLC.ColAccel\"" }
   
   files { "source/*.h" }
   files { "source/*.cpp", "source/*.c" }
   files { "source/*.rc" }

   files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
   
   includedirs { "source/" }
   includedirs { "external/hooking" }
   includedirs { "external/injector/include" }
	  
project "IV.EFLC.ColAccel"
   kind "SharedLib"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"
   targetextension ".asi"

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
	  characterset ("MBCS")

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
	  flags { "StaticRuntime" }
	  characterset ("MBCS")
	  targetdir "data/"
