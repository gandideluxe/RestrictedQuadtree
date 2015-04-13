file(GLOB_RECURSE FRAMEWORK_SHADER_VERTEX RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.vert)
file(GLOB_RECURSE FRAMEWORK_SHADER_FRAGME RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.frag)

add_executable(RestrictedQuadtree restricted_quadtree.cpp
              ${FRAMEWORK_SHADER_VERTEX}
              ${FRAMEWORK_SHADER_FRAGME})
#add_test(buffer-uniform COMMAND $<TARGET_FILE:RestrictedQuadtree>)

target_link_libraries(RestrictedQuadtree ${FRAMEWORK_NAME} ${BINARY_FILES})
add_dependencies(RestrictedQuadtree glfw ${FRAMEWORK_NAME} ${COPY_BINARY})

install(TARGETS RestrictedQuadtree DESTINATION .)
