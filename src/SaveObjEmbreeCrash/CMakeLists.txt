add_executable(SaveObjEmbreeCrash
    main.cpp
)
link_directories(lib)
target_link_libraries(SaveObjEmbreeCrash PRIVATE steamaudio)
set_target_properties(SaveObjEmbreeCrash PROPERTIES
    FOLDER TestCases
)
