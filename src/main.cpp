#include "boundary/cli/command_router.hpp"
#include "boundary/cli/user_config.hpp"
#include "infrastructure/composition/application_builder.hpp"

#include <iostream>

int main(int argc, char** argv) {
    auto config = crumb::boundary::UserConfig::load_default();
    if (!config) {
        std::cerr << "crumb: " << config.error() << '\n';
        return 1;
    }

    crumb::infrastructure::ApplicationComponents components;
    return crumb::boundary::CommandRouter(components.reconcile, components.rebuild_index,
                                          components.search, components.index_size, components.drive,
                                          std::move(*config)).run(argc, argv);
}
