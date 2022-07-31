add_library(steamaudio STATIC
    include/phonon.cpp    
    include/phonon.h
    include/phonon_interfaces.h
    include/phonon_version.h
)

set_target_properties(steamaudio PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(steamaudio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/phonon.lib)
set_target_properties(steamaudio PROPERTIES
    FOLDER Externals
)
link_directories(${CMAKE_SOURCE_DIR}/lib)
target_include_directories(steamaudio PUBLIC ./include)

add_custom_command(TARGET steamaudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/phonon.lib"
    $<TARGET_FILE_DIR:steamaudio>)

add_custom_command(TARGET steamaudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/phonon.dll"
    $<TARGET_FILE_DIR:steamaudio>)

add_custom_command(TARGET steamaudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/GPUUtilities.dll"
    $<TARGET_FILE_DIR:steamaudio>)

add_custom_command(TARGET steamaudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/bin/TrueAudioNext.dll"
    $<TARGET_FILE_DIR:steamaudio>)