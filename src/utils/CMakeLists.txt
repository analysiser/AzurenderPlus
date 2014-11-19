add_library(utils Parallel.h tiny_obj_loader.cc tiny_obj_loader.h)
SET_TARGET_PROPERTIES(utils PROPERTIES LINKER_LANGUAGE C++0x)

# target_link_libraries(debug ${TBB_DEBUG} optimized ${TBB_RELEASE})