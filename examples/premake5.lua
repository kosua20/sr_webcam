
-- Helper functions for the projects.

function CommonSetup()
	kind("ConsoleApp")
	systemversion("latest")

	-- Build flags
	filter("toolset:not msc*")
		buildoptions({ "-Wall", "-Wextra" })
	filter("toolset:msc*")
		buildoptions({ "-W3"})
	filter({})

	-- Common include dirs
	sysincludedirs({ "../include/", "common/glfw/include/", "common/gl3w/", "common/" })
	links({"glfw3", "sr_webcam"})
	files({"common/gl3w/gl3w.cpp", "common/common.c", "common/common.h"})
	

	-- Libraries for each platform.
	filter("system:macosx")
		links({"OpenGL.framework", "Cocoa.framework", "IOKit.framework", "CoreVideo.framework", "AppKit.framework", "AVFoundation.framework", "CoreMedia.framework", "Accelerate.framework" })

	filter("system:windows")
		links({"opengl32", "comctl32", "mfplat", "mf", "mfuuid", "Mfreadwrite", "Shlwapi"})

	filter("system:linux")
		links({"GL", "X11", "Xi", "Xrandr", "Xxf86vm", "Xinerama", "Xcursor", "Xext", "Xrender", "Xfixes", "xcb", "Xau", "Xdmcp", "rt", "m", "pthread", "dl"})
	
	filter({})

end


-- Projects
group("Examples")

project("c_app")
	language("C")
	CommonSetup()
	files({ "c_app/main.c" })
	vpaths({
	   ["*"] = {"common/gl3w/**.*", "common/common.*", "c_app/**.*"}
	})
	
project("cpp_app")
	language("C++")
	cppdialect("C++11")
	CommonSetup()
	files({ "cpp_app/main.cpp" })
	vpaths({
	   ["*"] = {"common/gl3w/**.*", "common/common.*", "cpp_app/**.*"}
	})

include("examples/common/glfw/premake5.lua")
