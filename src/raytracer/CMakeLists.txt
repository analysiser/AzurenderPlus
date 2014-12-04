file(GLOB RAYTRACER_FILES "*.h" "*.hpp" "*cpp")
add_executable(raytracer ${RAYTRACER_FILES})

target_link_libraries(raytracer application math scene tinyxml ${SDL_LIBRARY}
                      ${PNG_LIBRARIES} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES}
                      ${GLEW_LIBRARIES} ${MPI_LIBRARIES})

if(APPLE)
    target_link_libraries(raytracer SDLmain)
endif()

install(TARGETS raytracer DESTINATION ${PROJECT_SOURCE_DIR}/..)
