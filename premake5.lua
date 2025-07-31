workspace "twig"
  configurations { "debug", "release" }
  architecture "x86_64"
  location "build"
  startproject "twig"

  filter "configurations:debug"
    defines { "DEBUG" }
    symbols "on"

  filter "configurations:release"
    defines { "NDEBUG" }
    optimize "on"

project "miniz"
  kind "staticlib"
  language "c"

  targetdir "%{wks.location}/miniz/bin/%{cfg.buildcfg}"
  objdir "%{wks.location}/miniz/obj/%{cfg.buildcfg}"

  files { "src/lib/miniz.h", "src/lib/miniz.c" }

project "wren"
  kind "staticlib"
  language "c"

  targetdir "%{wks.location}/wren/bin/%{cfg.buildcfg}"
  objdir "%{wks.location}/wren/obj/%{cfg.buildcfg}"

  files { "src/lib/wren.h", "src/lib/wren.c" }

project "stb_image"
  kind "staticlib"
  language "c"

  targetdir "%{wks.location}/stb_image/bin/%{cfg.buildcfg}"
  objdir "%{wks.location}/stb_image/obj/%{cfg.buildcfg}"

  files { "src/lib/stb_image.h", "src/lib/stb_image.c" }

project "twig"
  kind "windowedapp"
  language "c"
  cdialect "c23"
  warnings "extra"

  targetdir "%{wks.location}/bin/%{cfg.buildcfg}"
  objdir "%{wks.location}/obj/%{cfg.buildcfg}"

  files { "src/twig.c" }
  links { "miniz", "wren", "stb_image", "winmm" }

  prebuildcommands {
    "\"C:\\Program Files\\7-Zip\\7z.exe\" a -tzip -mx=9 data.zip ../data/* -bso0"
  }
