#include "zv.h"

#include <fcntl.h>

int main(void) {
    zv_warn("warning test");
    zv_info("info test");
    zv_debug("debug test");

    /* open("noexist", O_RDONLY); */
    zv_err("err test");

    return 0;
}
