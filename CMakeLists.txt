nw_project_prepare(Core)

nwstd_add_library_slim(task SHARED ${CMAKE_CURRENT_SOURCE_DIR})

#System Check and Add Platform-Specific Source
if (WIN32)
	set(TARGET_MACRO -DTASK_TARGET_WINDOWS)
elseif (UNIX)
	set(TARGET_MACRO -DTASK_TARGET_POSIX)
    if (APPLE)
		list(APPEND TARGET_MACRO -DTASK_TARGET_DARWIN)
    else()
		list(APPEND TARGET_MACRO -DTASK_TARGET_UNIX_GENERIC)
	endif()
    set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost REQUIRED COMPONENTS context)
	set(TARGET_LIBS Boost::context)
endif ()

target_link_libraries(task ${TARGET_LIBS})
