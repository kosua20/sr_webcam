
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
	sysincludedirs({ "include/", "examples/glfw/include/" })
	links({"glfw3", "sr_webcam"})

	-- Libraries for each platform.
	filter("system:macosx")
		links({"OpenGL.framework", "Cocoa.framework", "IOKit.framework", "CoreVideo.framework", "AppKit.framework", "AVFoundation.framework", "CoreMedia.framework" })

	filter("system:windows")
		links({"opengl32", "comctl32"})

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
	
project("cpp_app")
	language("C++")
	cppdialect("C++11")
	CommonSetup()
	files({ "cpp_app/main.cpp" })

include("examples/glfw/premake5.lua")
