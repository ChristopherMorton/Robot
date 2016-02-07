set(APP_FILES app.cpp game.cpp level.cpp background.cpp savestate.cpp animation.cpp shutdown.cpp util.cpp config.cpp log.cpp pugixml.cpp)

add_executable(Robot ${APP_FILES})

target_link_libraries(${EXECUTABLE_NAME} ${LIBRARY_NAME} ${SFML_LIBRARIES})
