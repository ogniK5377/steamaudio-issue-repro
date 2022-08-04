add_executable(StaticMeshCrash
    main.cpp
)
link_directories(lib)
target_link_libraries(StaticMeshCrash PRIVATE steamaudio)
set_target_properties(StaticMeshCrash PROPERTIES
    FOLDER TestCases
)
