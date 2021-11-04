#include "core/Core.hpp"
#include "settings.hpp"

#include <stdio.h>
#include <unistd.h>

#include <err.h>

static void eunveil(const char *path, const char *permissions) {
    if (unveil(path, permissions) < 0)
        err(1, "unveil");
}

void sandbox_start() {
    //return;
    printf("[INFO] Starting PLEDGE+UNVEIL sandbox...\n");

    if (pledge("stdio rpath wpath cpath inet flock unveil", NULL) < 0)
        err(1, "pledge");

    // database stuff
    eunveil(settings::DBPATH.c_str(), "rwc");
    eunveil((settings::DBPATH + "-journal").c_str(), "rwc");
    eunveil((settings::DBPATH + "-wal").c_str(), "rwc");

    // tabledata stuff
    //eunveil(settings::TDATADIR.c_str(), "r");
    eunveil((settings::TDATADIR + settings::GRUNTWORKJSON).c_str(), "wc");

    // for bcrypt_gensalt()
    eunveil("/dev/urandom", "r");

    eunveil(NULL, NULL);
}
