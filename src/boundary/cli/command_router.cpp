#include "boundary/cli/command_router.hpp"
#include "domain/value_objects/value_objects.hpp"
#include <iostream>
namespace crumb::boundary {
int CommandRouter::run(int argc, char** argv) const {
    const std::string command = argc > 1 ? argv[1] : "scan";
    if (command != "scan") { std::cerr << "usage: crumb [scan [DIRECTORY]]\n"; return 2; }
    const auto directory = domain::DirectoryPath::create(argc > 2 ? argv[2] : ".");
    auto result = reconcile_.execute(directory);
    if (!result) { std::cerr << "crumb: " << result.error() << '\n'; return 1; }
    std::cout << "added=" << result->added << " updated=" << result->updated << " removed=" << result->removed << '\n';
    return 0;
}
}
