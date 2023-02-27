#pragma once

#include "settings.pb.h"

enum class LoadSettingsResult
{
    Loaded,
    DefaultInit,
};
LoadSettingsResult loadSettings(Settings& settings);

enum class SaveSettingsResult
{
    Saved,
    EncodingFailed,
};
SaveSettingsResult saveSettings(Settings& settings);

void dumpSettings(const Settings& settings);

void performRandomModifications(Settings& settings);
