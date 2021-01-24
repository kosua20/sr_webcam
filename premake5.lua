
workspace("sr_webcam")

-- Configurations
	configurations({ "Release", "Debug"})
	location("build")
	targetdir ("build/%{prj.name}/%{cfg.longname}")
	debugdir ("build/%{prj.name}/%{cfg.longname}")
	architecture("x64")

	-- Configuration specific settings.
	filter("configurations:Release")
		defines({ "NDEBUG" })
		optimize("On")

	filter("configurations:Debug")
		defines({ "DEBUG" })
		symbols("On")

	filter({})
	startproject("sr_webcam")

project("sr_webcam")
	kind("StaticLib")
	systemversion("latest")
	

	filter("toolset:not msc*")
		buildoptions({ "-Wall", "-Wextra" })
	filter("toolset:msc*")
		buildoptions({ "-W3"})
	filter({})

	includedirs({"include/"})
	-- common files
	files({"include/sr_webcam.h", "src/sr_webcam_internal.h", "src/sr_webcam.c"})
	
	-- system build filters
	filter("system:windows")
		language("C++")
		files({"src/sr_webcam_win.cpp"})

	filter("system:macosx")
		language("C")
		files({"src/sr_webcam_mac.m"})

	filter({"system:linux"})
		language("C")
		files({"src/sr_webcam_lin.c"})

	-- visual studio filters
	--filter("action:vs*")
	--	defines({ "_CRT_SECURE_NO_WARNINGS" })    

	filter({})

include("examples/premake5.lua")

-- Actions

newaction {
   trigger     = "clean",
   description = "Clean the build directory",
   execute     = function ()
      print("Cleaning...")
      os.rmdir("./build")
      print("Done.")
   end
}