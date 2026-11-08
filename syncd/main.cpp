#include "swss/logger.h"

int syncd_main(int argc, char **argv);

int main(int argc, char **argv)
{
    SWSS_LOG_ENTER();

    return syncd_main(argc, argv);
}
