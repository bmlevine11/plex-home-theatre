set(CMAKE_SHARED_MODULE_SUFFIX ".vis")
set(CMAKE_SHARED_MODULE_PREFIX "")


if(NOT WIN32)
    include_directories(
      ${root}/xbmc/visualizations/EGLHelpers
      )

  add_subdirectory(EGLHelpers)
  add_subdirectory(OpenGLSpectrum)
  
else()
  add_subdirectory(Milkdrop)
  add_subdirectory(DirectXSpectrum)
endif()
