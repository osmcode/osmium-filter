
#include <boost/program_options.hpp>

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
#include "compiled_filter.hpp"

namespace po = boost::program_options;

void print_help(const po::options_description& desc) {
    std::cout << "osmium-filter [OPTIONS] INPUT-FILE\n\n"
              << desc << "\n";
}

int main(int argc, char* argv[]) {
    po::options_description desc{"OPTIONS"};
    desc.add_options()
        ("help,h", "Print usage information")
        ("verbose,v", "Enable verbose output")
        ("output,o", po::value<std::string>(), "Output file name")
        ("output-format,f", po::value<std::string>(), "Output format")
        ("expression,e", po::value<std::string>(), "Filter expression")
        ("expression-file,E", po::value<std::string>(), "Filter expression file")
        ("dry-run,n", "Only parse expression, do not run it")
    ;

    po::options_description hidden;
    hidden.add_options()
    ("input-filename", po::value<std::string>(), "OSM input file")
    ;

    po::options_description parsed_options;
    parsed_options.add(desc).add(hidden);

    po::positional_options_description positional;
    positional.add("input-filename", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(parsed_options).positional(positional).run(), vm);
    po::notify(vm);

    std::string input_filename{"-"};
    std::string output_format;
    std::string output_filename{"-"};
    std::string filter_expression;
    bool verbose = false;
    bool run = true;

    if (vm.count("help")) {
        print_help(desc);
        std::exit(0);
    }

    if (vm.count("verbose")) {
        verbose = true;
    }

    if (vm.count("dry-run")) {
        run = false;
    }

    if (vm.count("input-filename")) {
        input_filename = vm["input-filename"].as<std::string>();
    }

    if (vm.count("output-format")) {
        output_format = vm["output-format"].as<std::string>();
    }

    if (vm.count("output")) {
        output_filename = vm["output"].as<std::string>();
    }

    if (vm.count("expression") && vm.count("expression-file")) {
        std::cerr << "Do not use --expression/-e and --expression-file/-E together\n";
        std::exit(2);
    }

    if (vm.count("expression")) {
        filter_expression = vm["expression"].as<std::string>();
    }

    if (vm.count("expression-file")) {
        std::ifstream t{vm["expression-file"].as<std::string>()};
        filter_expression.append(std::istreambuf_iterator<char>{t},
                                 std::istreambuf_iterator<char>{});
    }

    OSMObjectFilter filter{filter_expression, verbose};

    // With --dry-run or -n we are done.
    if (!run) {
        return 0;
    }

    CompiledFilter cfilter{filter};

    osmium::io::Reader reader{input_filename};

    osmium::io::File output_file{output_filename, output_format};
    osmium::io::Writer writer{output_file, osmium::io::overwrite::allow};

    osmium::ProgressBar progress_bar{reader.file_size(), true};
    while (osmium::memory::Buffer buffer = reader.read()) {
        for (auto& object : buffer.select<osmium::OSMObject>()) {
            progress_bar.update(reader.offset());
            if (cfilter.match(object)) {
                writer(object);
            }
        }
    }
    progress_bar.done();

    reader.close();
    writer.close();

    return 0;
}

