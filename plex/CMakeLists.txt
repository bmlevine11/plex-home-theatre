
include_directories(
	${root}/xbmc/cores
	${root}/xbmc/guilib
	${root}/xbmc/network
	${root}/xbmc/music
	${root}/xbmc/filesystem
	${root}/xbmc/threads
	${root}/xbmc/video
	${root}/xbmc/music/tags
	${root}/xbmc/dialogs
	${root}/xbmc/pictures
	${root}/xbmc/playlists
	${root}/plex/FileSystem
	${root}/plex/Network
	${root}/plex/Remote
	${root}/plex/Filters
  ${root}/plex/Third-Party/rapidxml
  ${root}/plex/Playlists
  ${CONFIG_INCLUDE_PATH_SPARKLE}
)

if(ENABLE_AUTOUPDATE)
  include(ExternalProject)
  ExternalProject_Add(Update-Installer
    CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}"
    SOURCE_DIR ${plexdir}/Update-Installer
    INSTALL_DIR "${CMAKE_BINARY_DIR}"
  )
  if(TARGET_OSX)
    install(FILES ${CMAKE_BINARY_DIR}/update_installer/updater
            PERMISSIONS WORLD_READ WORLD_EXECUTE OWNER_WRITE OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ
            DESTINATION ${EXECUTABLE_NAME}.app/Contents/Resources/XBMC/tools)
  else(TARGET_OSX)
    install(FILES ${CMAKE_BINARY_DIR}/update_installer/updater.exe
            PERMISSIONS WORLD_READ WORLD_EXECUTE OWNER_WRITE OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ
            DESTINATION ${BINPATH} COMPONENT RUNTIME)
  endif(TARGET_OSX)

  add_subdirectory(AutoUpdate)
endif(ENABLE_AUTOUPDATE)

if(TARGET_WIN32)
  # disable safe seh
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO /DELAYLOAD:dnssd.dll /DELAYLOAD:dwmapi.dll /DELAYLOAD:libmicrohttpd-5.dll /DELAYLOAD:sqlite3.dll /DELAYLOAD:libsamplerate-0.dll /MANIFEST:NO")
endif(TARGET_WIN32)

OPTION(USE_RAPIDXML "Use RAPIDXML instead of TinyXML - Faster" ON)
if(USE_RAPIDXML)
  add_definitions(-DUSE_RAPIDXML)
endif(USE_RAPIDXML)
 
if(HAVE_BREAKPAD)
  if(NOT TARGET_WIN32)
   list(APPEND CONFIG_PLEX_LINK_LIBRARIES ${LIBBREAKPAD_CLIENT})
  endif(NOT TARGET_WIN32)
endif(HAVE_BREAKPAD)

add_subdirectory(CrashReporter)
add_subdirectory(Client)
add_subdirectory(FileSystem)
add_subdirectory(GUI)
add_subdirectory(Home)
add_subdirectory(Network)
add_subdirectory(Owned)
add_subdirectory(Utility)
add_subdirectory(Remote)
add_subdirectory(Filters)
add_subdirectory(Playlists)
add_subdirectory(Windows)
add_subdirectory(AutoUpdate)
add_subdirectory(Tests)

include(BreakpadSymbols)

if(TARGET_WIN32)
  add_subdirectory(Resources/MCE)
endif(TARGET_WIN32)

if(TARGET_OSX)
  add_subdirectory(Helper)
endif(TARGET_OSX)

find_all_sources(. plex_SRCS)

list(REMOVE_ITEM plex_SRCS ./pch_plex.cpp)

set(Headers PlexTypes.h)
if(NOT TARGET_OSX)
  list(REMOVE_ITEM plex_SRCS ./PlexMacUtils.mm)
endif(NOT TARGET_OSX)

if(NOT TARGET_WIN32)
  list(REMOVE_ITEM plex_SRCS ./MediaCenterLaunchHost.cpp)
else(NOT TARGET_WIN32)
  list(APPEND plex_SRCS ${root}/xbmc/win32/Win32DelayedDllLoad.cpp)
endif(NOT TARGET_WIN32)

get_property(PLEX_MODULE_SRCS GLOBAL PROPERTY SRCS_LIST)
set(PLEX_ALL_SRCS ${PLEX_MODULE_SRCS} ${plex_SRCS} ${Headers})

ADD_MSVC_PRECOMPILED_HEADER("pch_plex.h" "pch_plex.cpp" PLEX_ALL_SRCS)

# build all plex related source files
add_library(PlexMain OBJECT ${PLEX_ALL_SRCS})

# build test cases
if(ENABLE_TESTING)
  get_property(TEST_CASES GLOBAL PROPERTY PLEX_TEST_CASES)
  foreach(cases ${TEST_CASES})
    message(STATUS "Adding test case ${cases}")
    list(APPEND TEST_CASES_OBJS $<TARGET_OBJECTS:${cases}>)
  endforeach(cases ${TEST_CASES})
  add_executable(PlexHomeTheaterTests ${TEST_CASES_OBJS} $<TARGET_OBJECTS:xbmctest> $<TARGET_OBJECTS:PlexMain> $<TARGET_OBJECTS:PlexTestingUtils>)
  target_link_libraries(PlexHomeTheaterTests xbmc pcre gtest ${CONFIG_INTERNAL_LIBS} ${CONFIG_PLEX_LINK_LIBRARIES})
  add_test(PlexHomeTheaterTests ${EXECUTABLE_OUTPUT_PATH}/PlexHomeTheaterTests --gtest_output=xml:${CMAKE_CURRENT_BINARY_DIR}/PlexHomeTheaterTests.xml --gtest_color=yes)

  add_custom_target(all_tests DEPENDS PlexHomeTheaterTests)
  add_custom_target(run_tests COMMAND ${CMAKE_CTEST_COMMAND} -V DEPENDS all_tests)
endif(ENABLE_TESTING)

find_program(LIZARD lizard HINTS /usr/local/bin)
if(NOT LIZARD MATCHES "-NOTFOUND")
  add_custom_command(
    OUTPUT lizard-output.xml
    COMMAND ${LIZARD} --xml -t 4 ${plexdir} -x"${plexdir}/Dependencies/*" -x"${plexdir}/Update-Installer/*" -x"${plexdir}/Third-Party/*" > ${CMAKE_CURRENT_BINARY_DIR}/lizard-output.xml
  )
  add_custom_command(
    OUTPUT lizard-stdout
    COMMAND ${LIZARD} -w -t 4 ${plexdir} -x"${plexdir}/Dependencies/*" -x"${plexdir}/Update-Installer/*" -x"${plexdir}/Third-Party/*"
  )

  add_custom_target(lizard DEPENDS lizard-output.xml)
  add_custom_target(lizard-warnings DEPENDS lizard-stdout)
endif(NOT LIZARD MATCHES "-NOTFOUND")

if(TARGET_COMMON_DARWIN)
  add_executable(${PLEX_TARGET_NAME} MACOSX_BUNDLE $<TARGET_OBJECTS:PlexMain> $<TARGET_OBJECTS:xbmcmain>)
  set_target_properties(${PLEX_TARGET_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${plexdir}/Resources/Info.plist.in")
  add_dependencies(${PLEX_TARGET_NAME} Update-Installer)
  set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PLEX_VERSION_STRING}")
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PLEX_VERSION_STRING_SHORT}")
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${PLEX_VERSION_STRING}")
else()
  add_executable(${PLEX_TARGET_NAME} WIN32 $<TARGET_OBJECTS:PlexMain> $<TARGET_OBJECTS:xbmcmain> Resources/Plex.rc Resources/Plex.rc2)
endif()

# Set the correct binary name
set_target_properties(${PLEX_TARGET_NAME} PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME})
set_target_properties(${PLEX_TARGET_NAME} PROPERTIES PDB_NAME "Plex Home Theater")

if(ENABLE_DUMP_SYMBOLS)
  GENERATE_BREAKPAD_SYMBOLS(${PLEX_TARGET_NAME})
  add_custom_target(symbols DEPENDS ${PLEX_TARGET_NAME}_symbols)
endif(ENABLE_DUMP_SYMBOLS)

add_dependencies(${PLEX_TARGET_NAME} git_revision.h)
target_link_libraries(${PLEX_TARGET_NAME} xbmc pcre ${CONFIG_INTERNAL_LIBS} ${CONFIG_PLEX_LINK_LIBRARIES})

if(TARGET_WIN32)
  add_custom_command(
    TARGET ${PLEX_TARGET_NAME}
	POST_BUILD
	COMMAND "mt.exe" -manifest "${plexdir}/Resources/pht.manifest" -outputresource:"$<TARGET_FILE:${PLEX_TARGET_NAME}>"
	COMMENT "Embedding manifest")
endif(TARGET_WIN32)

if(ENABLE_PYTHON)
  target_link_libraries(${PLEX_TARGET_NAME} addons_python)
endif(ENABLE_PYTHON)


# find all image files in the skin
file(GLOB MEDIA_IMAGES_PLEX_SKIN ${root}/addons/skin.plex/Media/*.png ${root}/addons/skin.plex/Media/*.gif)

if(COMPRESS_TEXTURES AND NOT TARGET_RPI)
  # Build the packed textures
  if(WIN32)
    set(WORKDIR ${root}/tools/TexturePacker)
  else(WIN32)
    set(WORKDIR ${CMAKE_CURRENT_BINARY_DIR})
  endif(WIN32)

  if(TEXTUREPACKERPATH)
    set(TEXTUREPACKER_EXE ${TEXTUREPACKERPATH})
  else(TEXTUREPACKERPATH)
    set(TEXTUREPACKER_EXE $<TARGET_FILE:TexturePacker>)
  endif(TEXTUREPACKERPATH)

  add_custom_command(
    OUTPUT Textures_Plex.xbt
    COMMAND ${TEXTUREPACKER_EXE} -input ${root}/addons/skin.plex/Media -output ${CMAKE_CURRENT_BINARY_DIR}/Textures_Plex.xbt
    MAIN_DEPENDENCY ${MEDIA_IMAGES_PLEX_SKIN}
    DEPENDS TexturePacker
    WORKING_DIRECTORY ${WORKDIR}
  )
  add_custom_target(CompressTextures ALL DEPENDS Textures_Plex.xbt)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Textures_Plex.xbt DESTINATION ${RESOURCEPATH}/addons/skin.plex/Media RENAME Textures.xbt COMPONENT RUNTIME)

  set(EXCLUDE_TEXTURES "skin.plex/Media/*")
else()
  set(EXCLUDE_TEXTURES ^foo)
endif()

# Set some variables we need to configure the CMakeCompleteBundle file
set(PLEXDIR ${plexdir})
set(ROOTDIR ${root})
set(DEPENDDIR ${dependdir})
configure_file(CMakeCompleteBundle.cmake.in CMakeCompleteBundle.cmake @ONLY)

if(TARGET_COMMON_DARWIN OR TARGET_WIN32)
  install(FILES ${CONFIG_PLEX_INSTALL_LIBRARIES} DESTINATION ${LIBPATH} COMPONENT RUNTIME)
endif()

if(TARGET_COMMON_DARWIN)
  install(TARGETS ${PLEX_TARGET_NAME} BUNDLE DESTINATION . COMPONENT RUNTIME)
  install(FILES ${plexdir}/Resources/Plex.icns DESTINATION ${EXECUTABLE_NAME}.app/Contents/Resources COMPONENT RUNTIME)
  install(FILES ${plexdir}/Resources/Credits.html DESTINATION ${EXECUTABLE_NAME}.app/Contents/Resources COMPONENT RUNTIME)
else(TARGET_COMMON_DARWIN)
  install(TARGETS ${PLEX_TARGET_NAME} RUNTIME DESTINATION ${BINPATH} COMPONENT RUNTIME)
  install(FILES ${plexdir}/Resources/Credits.html DESTINATION ${RESOURCEPATH} COMPONENT RUNTIME)
endif(TARGET_COMMON_DARWIN)

if(NOT DEFINED PLEX_SPLASH)
  if(TARGET_RPI)
    set(PLEX_SPLASH Splash-RPI.png)
  else(TARGET_RPI)
    set(PLEX_SPLASH Splash.png)
  endif(TARGET_RPI)
endif(NOT DEFINED PLEX_SPLASH)


install(DIRECTORY ${root}/media ${root}/sounds ${root}/language DESTINATION ${RESOURCEPATH} COMPONENT RUNTIME
        PATTERN ${PLEX_SPLASH} EXCLUDE
        PATTERN Credits.html EXCLUDE)

install(DIRECTORY ${root}/system DESTINATION ${RESOURCEPATH} COMPONENT RUNTIME
        PATTERN python EXCLUDE
        PATTERN playercorefactory.xml EXCLUDE
        PATTERN peripherals.xml EXCLUDE)


install(DIRECTORY ${root}/addons DESTINATION ${RESOURCEPATH} COMPONENT RUNTIME
        PATTERN skin.confluence EXCLUDE
        PATTERN skin.touched EXCLUDE
        REGEX screensaver.rsxs* EXCLUDE
        REGEX library.* EXCLUDE
        REGEX metadata.* EXCLUDE
        REGEX weather.* EXCLUDE
        PATTERN repository.xbmc.org EXCLUDE
        REGEX ${EXCLUDE_TEXTURES} EXCLUDE
        PATTERN .git EXCLUDE
        PATTERN xbmc.python EXCLUDE
)

install(FILES ${plexdir}/Resources/${PLEX_SPLASH} DESTINATION ${RESOURCEPATH}/media COMPONENT RUNTIME RENAME ${PLEX_SPLASH})

install(DIRECTORY ${plexdir}/addons DESTINATION DESTINATION ${RESOURCEPATH} COMPONENT RUNTIME)
install(FILES ${plexdir}/Resources/plex-icon-120.png DESTINATION ${RESOURCEPATH}/media COMPONENT RUNTIME)
install(FILES ${plexdir}/Resources/plex-icon-256.png DESTINATION ${RESOURCEPATH}/media COMPONENT RUNTIME)
install(FILES ${plexdir}/Resources/SlideshowOverlay.png DESTINATION ${RESOURCEPATH}/media COMPONENT RUNTIME)
install(FILES ${plexdir}/Resources/peripherals.xml ${plexdir}/Resources/playercorefactory.xml DESTINATION ${RESOURCEPATH}/system COMPONENT RUNTIME)

if(TARGET_OSX)
  install(FILES ${plexdir}/Resources/com.plexapp.ht.helper.plist DESTINATION ${RESOURCEPATH}/tools/darwin/runtime COMPONENT RUNTIME)
elseif(TARGET_WIN32)
  install(FILES ${root}/system/zlib1.dll DESTINATION ${BINPATH} COMPONENT RUNTIME)
  install(FILES ${root}/project/Win32BuildSetup/dependencies/libcdio-13.dll DESTINATION ${BINPATH} COMPONENT RUNTIME)
  install(FILES ${plexdir}/build/dependencies/vcredist/2012/vcredist_x86.exe DESTINATION ${BINPATH}/Dependencies COMPONENT VCREDIST RENAME vcredist_2012_x86.exe)
  install(FILES ${plexdir}/build/dependencies/vcredist/2010/vcredist_x86.exe DESTINATION ${BINPATH}/Dependencies COMPONENT VCREDIST RENAME vcredist_2010_x86.exe)
  install(DIRECTORY ${plexdir}/build/dependencies/dxsetup DESTINATION ${BINPATH}/Dependencies COMPONENT QDXSETUP)
  install(FILES ${root}/project/Win32BuildSetup/dependencies/glew32.dll DESTINATION ${BINPATH} COMPONENT RUNTIME)
  install(FILES ${root}/project/Win32BuildSetup/dependencies/libiconv-2.dll DESTINATION ${BINPATH} COMPONENT RUNTIME)
  install(FILES ${plexdir}/Resources/Plex.ico ${plexdir}/Resources/PlexBanner.bmp DESTINATION ${RESOURCEPATH}/media COMPONENT RUNTIME)
endif()

if(CREATE_BUNDLE)
  if(TARGET_COMMON_DARWIN OR TARGET_WIN32)
    install(SCRIPT ${CMAKE_BINARY_DIR}/plex/CMakeCompleteBundle.cmake COMPONENT RUNTIME)
  endif()
endif()
