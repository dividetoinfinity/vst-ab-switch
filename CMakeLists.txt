cmake_minimum_required (VERSION 3.9)

# build VST 2 by default
option(SMTG_CREATE_VST2_VERSION "Use VST2" ON)

#-------------------------------------------------------------------------------
# Platform Detection
#-------------------------------------------------------------------------------

if(APPLE)
    set(MAC TRUE)
elseif(WIN32)
    set(WIN TRUE)
elseif(UNIX)
    set(LINUX TRUE)
endif()

if(MAC)
  set(VST3_SDK_ROOT "/Users/Shared/Steinberg/VST_SDK.369/VST3_SDK" CACHE PATH "Location of VST3 SDK")
elseif(WIN)
  set(VST3_SDK_ROOT "C:/Users/Public/Documents/Steinberg/VST_SDK.369/VST3_SDK" CACHE PATH "Location of VST3 SDK")
else()
  set(VST3_SDK_ROOT "" CACHE PATH "Location of VST3 SDK")
endif()

set(PLUGIN_MAJOR_VERSION 1)
set(PLUGIN_MINOR_VERSION 1)
set(PLUGIN_PATCH_VERSION 2)
set(PLUGIN_VERSION "${PLUGIN_MAJOR_VERSION}.${PLUGIN_MINOR_VERSION}.${PLUGIN_PATCH_VERSION}")

project(VST_AB_Switch)

include(VST3_SDK.cmake)

set(CPP_SOURCES src/cpp)

configure_file(${CPP_SOURCES}/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h)
include_directories(${CMAKE_BINARY_DIR}/generated/)

if (SMTG_CREATE_VST2_VERSION)
	set(vst2_sources
			${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp
			${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/eventlist.cpp
			${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/hostclasses.cpp
			${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/parameterchanges.cpp
			${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/processdata.cpp
			${VST3_SDK_ROOT}/public.sdk/source/vst/vst2wrapper/vst2wrapper.cpp
			${VST3_SDK_ROOT}/public.sdk/source/vst/vst2wrapper/vst2wrapper.h
			${VST3_SDK_ROOT}/public.sdk/source/vst2.x/audioeffect.cpp
			${VST3_SDK_ROOT}/public.sdk/source/vst2.x/audioeffectx.cpp
			${CPP_SOURCES}/ABSwitchVST2.cpp
			)
endif()

set(vst_sources
		${CPP_SOURCES}/ABSwitchCIDs.h
		${CPP_SOURCES}/ABSwitchController.h
		${CPP_SOURCES}/ABSwitchController.cpp
		${CPP_SOURCES}/ABSwitchProcessor.h
		${CPP_SOURCES}/ABSwitchProcessor.cpp
		${CPP_SOURCES}/ABSwitchUtils.h
		${CPP_SOURCES}/ABSwitchVST3.cpp
		${CPP_SOURCES}/Concurrent.h
		${CPP_SOURCES}/StringTextEdit.h
		${CPP_SOURCES}/StringTextEdit.cpp
		${CPP_SOURCES}/logging/loguru.hpp
		${CPP_SOURCES}/logging/logging.cpp
		)

# VST2 is only defined for macOS and Windows
if(MAC OR WIN)
	set(vst_sources ${vst_sources} ${vst2_sources})
endif()

set(target pongasoft_ABSwitch)
smtg_add_vst3plugin(${target} ${VST3_SDK_ROOT} ${vst_sources})
target_include_directories(${target} PUBLIC ${VSTGUI_ROOT}/vstgui4)
target_link_libraries(${target} PRIVATE base sdk vstgui_support)

smtg_add_vst3_resource(${target} "resource/ABSwitch.uidesc")
smtg_add_vst3_resource(${target} "resource/Background.png")
smtg_add_vst3_resource(${target} "resource/Background_2x.png")
smtg_add_vst3_resource(${target} "resource/Control_AudioSwitch.png")
smtg_add_vst3_resource(${target} "resource/Control_AudioSwitch_2x.png")
smtg_add_vst3_resource(${target} "resource/Control_SoftenSwitch.png")
smtg_add_vst3_resource(${target} "resource/Control_SoftenSwitch_2x.png")
smtg_add_vst3_resource(${target} "resource/Control_LEDLight_On_2x.png")
smtg_add_vst3_resource(${target} "resource/Control_LEDLight_On.png")
smtg_add_vst3_resource(${target} "resource/Control_LEDLight_Off_2x.png")
smtg_add_vst3_resource(${target} "resource/Control_LEDLight_Off.png")

if(MAC)
	smtg_set_bundle(${target} INFOPLIST "${CMAKE_CURRENT_LIST_DIR}/mac/Info.plist" PREPROCESS)
elseif(WIN)
	target_sources(${target} PRIVATE resource/ABSwitch.rc)
endif()

###################################################
# Create archive (.tgz)
###################################################
if(MAC)
	set(ARCHITECTURE "macOS_64bits")
elseif(WIN)
	set(ARCHITECTURE "win_64bits")
endif()

set(ARCHIVE_NAME ${target}-${ARCHITECTURE}-${PLUGIN_VERSION})
set(ARCHIVE_PATH ${CMAKE_BINARY_DIR}/archive/${ARCHIVE_NAME})

add_custom_command(OUTPUT ${ARCHIVE_PATH}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${ARCHIVE_PATH}
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/LICENSE.txt ${ARCHIVE_PATH}
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_LIST_DIR}/archive/README-${ARCHITECTURE}.txt ${ARCHIVE_PATH}/README.txt
		)

if(MAC)
	add_custom_command(OUTPUT ${ARCHIVE_PATH}.zip
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${VST3_OUTPUT_DIR}/${target}.${VST3_EXTENSION} ${ARCHIVE_PATH}/ABSwitch.vst3
			DEPENDS ${target}
			DEPENDS ${ARCHIVE_PATH}
			WORKING_DIRECTORY archive
			)
elseif(WIN)
	add_custom_command(OUTPUT ${ARCHIVE_PATH}.zip
			COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}> ${ARCHIVE_PATH}/ABSwitch.vst3
			DEPENDS ${target}
			DEPENDS ${ARCHIVE_PATH}
			WORKING_DIRECTORY archive
			)
endif()

if (SMTG_CREATE_VST2_VERSION)
	message(STATUS "SMTG_CREATE_VST2_VERSION is set for ${target}. A VST 2 version of the plug-in will be created (just rename the generated file from .vst3 to .vst).")
	if(MAC)
		# fix missing VSTPluginMain symbol when also building VST 2 version
		smtg_set_exported_symbols(${target} "${CMAKE_CURRENT_LIST_DIR}/mac/macexport_vst2.exp")
	endif()
	if (WIN)
		add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	endif()
endif()

if(MAC OR WIN)
	add_custom_command(OUTPUT ${ARCHIVE_PATH}.zip
			COMMAND ${CMAKE_COMMAND} -E tar cvf ${ARCHIVE_NAME}.zip --format=zip ${ARCHIVE_PATH}
			COMMAND ${CMAKE_COMMAND} -E remove_directory ${ARCHIVE_PATH}
			APPEND
			)

add_custom_target(archive
		DEPENDS ${ARCHIVE_PATH}.zip
		)
endif()

###################################################
# Testing
###################################################
# Download and unpack googletest at configure time
include(gtest.cmake)
enable_testing()
set(PROJECT_TEST_NAME ${PROJECT_NAME}_test)

file(GLOB_RECURSE TEST_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} test/cpp/*cpp)
message(STATUS "Test case source files: " ${TEST_SRC_FILES})

add_executable(${PROJECT_TEST_NAME} ${TEST_SRC_FILES} ${PROJECT_SOURCE_DIR}/src/cpp/logging/logging.cpp)
target_link_libraries(${PROJECT_TEST_NAME} gtest_main)
target_include_directories(${PROJECT_TEST_NAME} PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(${PROJECT_TEST_NAME} PUBLIC ${GTEST_INCLUDE_DIRS})

include(GoogleTest)
gtest_add_tests(
		TARGET      ${PROJECT_TEST_NAME}
		TEST_LIST   ${PROJECT_TEST_NAME}_targets
)
