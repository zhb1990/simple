#include <simple/application/application.h>
#include <simple/log/log.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        simple::error("application need config");
        return 0;
    }

    simple::application::instance().start(argv[1]);
    return 0;
}
