#include "settings.h"
#include "utils.h"

#include "FlashPROM.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include <stdio.h>

void core1()
{
    multicore_lockout_victim_init();
    for (;;) {}
}

int main()
{
    stdio_init_all();
    EEPROM.start();
    // Needed for FlashPROM
    multicore_launch_core1(core1);

    for (;;)
    {
        printf("Waiting for BOOTSEL press ...\n\n");
        while (getBootselButton());
        while (!getBootselButton());

        Settings settings;

        printf("Loading settings ...\n");
        if (loadSettings(settings) == LoadSettingsResult::DefaultInit)
        {
            printf("Failed to load settings. Initialize to defaults.\n");

            // Perform additional manual default initialization here ...
        }
        printf("\n");

        if (!settings.has_buzzerAddon)
        {
            // Perform additional manual default initialization for buzzerAddon here ...
        }

        printf("\nInitial State:\n==============\n\n");
        dumpSettings(settings);
        printf("\n");

        printf("Performing random modifications ...\n");
        performRandomModifications(settings);

        printf("\nModified State:\n===============\n\n");
        dumpSettings(settings);
        printf("\n");

        printf("Saving settings ...\n\n");
        saveSettings(settings);
    }

    return 0;
}
