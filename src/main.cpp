#include <iostream>
#include <expected>
#include <string>
#include <utility>

#include "presentation/cli/command_router.hpp"
#include "presentation/cli/user_config.hpp"
#include "composition/application_builder.hpp"

int main(int argc, char** argv) {
    auto config = crumb::boundary::UserConfig::load_default();
    if (!config) {
        std::cerr << "crumb: " << config.error() << '\n';
        return 1;
    }

    crumb::infrastructure::ApplicationComponents components;
    return crumb::boundary::CommandRouter(components.reconcile, components.rebuild_index,
                                          components.search, components.index_size,
                                          components.drive, std::move(*config))
        .run(argc, argv);
}
