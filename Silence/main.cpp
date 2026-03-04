#include "rbx/classes/classes.hpp"
#include "utils/configs/configs.hpp"
#include "utils/overlay/overlay.hpp"
#include <filesystem>
#include <iostream>
#include <thread>
#include <string>

int main()
{
    std::string appdata = silence::utils::appdata_path();
    if (!std::filesystem::exists(appdata + "\\silence"))
    {
        std::filesystem::create_directory(appdata + "\\silence");
    }

    if (!std::filesystem::exists(appdata + "\\silence\\configs"))
    {
        std::filesystem::create_directory(appdata + "\\silence\\configs");

    }

    std::thread(silence::roblox::init).detach();

    printf("silence > Start RBX::init\n");
    silence::utils::overlay::render();
}