#include "printf.hpp"
#include <iostream>
#include <string>
#include <vector>

int main() {
    sjtu::printf("Hello %s, %d, %u, %_\n", "world", -42, 42u, std::vector<int>{1, 2, 3});
    sjtu::printf("Escape %% test\n");
    sjtu::printf("Vector of strings: %_\n", std::vector<std::string>{"a", "b", "c"});
    sjtu::printf("Hello\n");
    sjtu::printf("Nested vector: %_\n", std::vector<std::vector<int>>{{1, 2}, {3, 4}});
    return 0;
}

