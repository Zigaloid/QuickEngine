#include <gtest/gtest.h>
#include <iostream>

// Main function to run all unit tests
int main(int argc, char** argv) {
    // Initialize Google Test framework
    ::testing::InitGoogleTest(&argc, argv);
    
    // Print header information
    std::cout << "Running Reflection System Unit Tests" << std::endl;
    std::cout << "====================================" << std::endl;
    
    // Run all tests
    int result = RUN_ALL_TESTS();
    
    // Print results summary
    if (result == 0) {
        std::cout << std::endl << "All tests passed successfully!" << std::endl;
    } else {
        std::cout << std::endl << "Some tests failed. Check output above for details." << std::endl;
    }
    
    return result;
}