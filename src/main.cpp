
#include <boost/program_options.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>
#include <vector>

#include <osmium/index/id_set.hpp>
#include <osmium/index/nwr_array.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/writer_options.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/util/progress_bar.hpp>

#include "object_filter.hpp"

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
        ("complete-ways,w", "Add nodes referenced in ways")
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
    bool complete_ways = false;

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

    if (vm.count("complete-ways")) {
        complete_ways = true;
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

    try {
        OSMObjectFilter filter{filter_expression};

        if (filter.entities() == osmium::osm_entity_bits::nothing) {
            std::cerr << "Filter expression can never match. Stopping.\n";
            return 1;
        }

        if (verbose) {
            filter.print_tree(std::cerr);

            const auto e = filter.entities();
            std::cerr << "entities:";
            if (e & osmium::osm_entity_bits::node) {
                std::cerr << " node";
            }
            if (e & osmium::osm_entity_bits::way) {
                std::cerr << " way";
            }
            if (e & osmium::osm_entity_bits::relation) {
                std::cerr << " relation";
            }
            std::cerr << "\n";
        }

        // With --dry-run or -n we are done.
        if (!run) {
            return 0;
        }

        filter.prepare();

        if (complete_ways) {
            osmium::nwr_array<osmium::index::IdSetDense<osmium::unsigned_object_id_type>> ids;

            {
                osmium::io::Reader reader{input_filename, filter.entities()};
                while (osmium::memory::Buffer buffer = reader.read()) {
                    for (const auto& object : buffer.select<osmium::OSMObject>()) {
                        if (filter.match(object)) {
                            ids(object.type()).set(object.positive_id());
                            if (object.type() == osmium::item_type::way) {
                                for (const auto& nr : static_cast<const osmium::Way&>(object).nodes()) {
                                    ids(osmium::item_type::node).set(nr.positive_ref());
                                }
                            }
                        }
                    }
                }
                reader.close();
            }

            osmium::io::Reader reader{input_filename};

            osmium::io::File output_file{output_filename, output_format};
            osmium::io::Writer writer{output_file, osmium::io::overwrite::allow};

            osmium::ProgressBar progress_bar{reader.file_size(), true};
            while (osmium::memory::Buffer buffer = reader.read()) {
                progress_bar.update(reader.offset());
                for (const auto& object : buffer.select<osmium::OSMObject>()) {
                    if (ids(object.type()).get(object.positive_id())) {
                        writer(object);
                    }
                }
            }
            progress_bar.done();

            reader.close();
            writer.close();
        } else {
            osmium::io::Reader reader{input_filename, filter.entities()};

            osmium::io::File output_file{output_filename, output_format};
            osmium::io::Writer writer{output_file, osmium::io::overwrite::allow};

            osmium::ProgressBar progress_bar{reader.file_size(), true};
            while (osmium::memory::Buffer buffer = reader.read()) {
                progress_bar.update(reader.offset());
                for (const auto& object : buffer.select<osmium::OSMObject>()) {
                    if (filter.match(object)) {
                        writer(object);
                    }
                }
            }
            progress_bar.done();

            reader.close();
            writer.close();
        }
    } catch (const expression_parser_error& e) {
        std::cerr << "Error parsing filter expression:\n";
        std::cerr << e.input() << "\n";
        if (e.pos() >= 0) {
            for (auto i = e.pos(); i > 0; --i) {
                std::cerr << ' ';
            }
        }
        std::cerr << "^\n";
    }

    return 0;
}

