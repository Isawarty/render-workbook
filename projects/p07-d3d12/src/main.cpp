#include "D3D12App.h"

#include <cstdlib>
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        uint32_t stage = 6;
        if (argc > 1) stage = static_cast<uint32_t>(std::stoul(argv[1]));
        if (stage < 1 || stage > 6) {
            std::cerr << "stage must be between 1 and 6\n";
            return EXIT_FAILURE;
        }
        p07::D3D12App app;
        app.initialize(static_cast<p07::Stage>(stage));
        app.runFrames(120);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "P07 failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
