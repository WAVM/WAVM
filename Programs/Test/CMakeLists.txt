add_executable(Test Test.cpp)
target_link_libraries(Test Logging IR WAST Runtime ThreadTest)
set_target_properties(Test PROPERTIES FOLDER Testing)
