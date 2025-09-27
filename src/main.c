#include "app/app.h"
#include "platform/system.h"

int main(void) {
    platform_bootstrap();
    app_run();
    platform_shutdown();
    return 0;
}
