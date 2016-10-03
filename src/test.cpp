
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>

#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/writer_options.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/util/progress_bar.hpp>

#include "object_filter.hpp"

void test(std::string expression) {
    std::cerr << "-- Testing: [" << expression << "]\n";
    OSMObjectFilter filter{expression};
    filter.print_tree();
    std::cerr << "\n";
}

int main(int argc, char* argv[]) {
    assert(argc == 2);
    std::ifstream test_file{argv[1]};
    std::string line;
    while (std::getline(test_file, line)) {
        if (!line.empty() && line[0] != '#') {
            test(line);
        }
    }
}

