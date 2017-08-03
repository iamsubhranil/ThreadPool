set(SOURCE_FILES mytest.c)
set(LIBRARY_FILES mythreads.c)

add_library(mythreads mythreads.c)
target_link_libraries(mythreads pthread)

add_executable(mytest mytest.c)
target_link_libraries(mytest mythreads)