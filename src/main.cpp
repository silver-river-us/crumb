#include "boundary/cli/command_router.hpp"
#include "infrastructure/composition/application_builder.hpp"

int main(int argc, char** argv) {
    crumb::infrastructure::ApplicationComponents components;
    return crumb::boundary::CommandRouter(components.reconcile, components.search).run(argc, argv);
}
