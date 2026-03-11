#include "app.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    App app;
    if (!init_app(app)) {
        return 1;
    }

    run_app(app);
    shutdown_app(app);

    return 0;
}
