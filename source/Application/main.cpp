#include <UGEApplication.h>
#include "ConfigLoader.h"
#include "ModuleLoader.hpp"
#include <cstdio>
#include <string>

int main(int Argc, char* Argv[])
{
    auto settings = LoadLaunchSettingsFromAppDirectory();
    if (!settings)
    {
        fprintf(stderr, "Fatal: %s\n", settings.error().c_str());
        return 1;
    }

    std::string moduleError;

    // Load all configured modules in order
    if (!ModuleLoader::LoadModulesRelativeToExecutable(settings->Modules, moduleError))
    {
        fprintf(stderr, "Fatal: failed to load modules: %s\n", moduleError.c_str());
        return 1;
    }

    auto ugeApp = UGEApplication::Create(Argc, Argv, *settings);
    if (!ugeApp) { fprintf(stderr, "Fatal: %s\n", ugeApp.error().c_str()); return 1; }

    (*ugeApp)->Run();

    return 0;
}
