add_executable(dconwrapper main.c)

target_link_libraries(dconwrapper bf_intern_dualcon)

target_include_directories(dconwrapper PRIVATE ..)
