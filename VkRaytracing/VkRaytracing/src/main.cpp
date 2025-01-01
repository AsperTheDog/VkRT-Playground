#include "engine/engine.hpp"
#include "scene/scene_loader.hpp"
#include "utils/defer.hpp"
#include "utils/logger.hpp"

int main() {
    try
    {
#ifndef _DEBUG
        Logger::setLevels(Logger::WARN | Logger::ERR);
#else
        Logger::setLevels(Logger::ALL);
#endif
        Logger::setRootContext("VkRT");

        Scene::InitializeManager();
        CallOnDestroy sceneManagerDestroy{Scene::destroyManager};

        Engine engine{"VkRT", {1920, 1080}};
        engine.run();

        Logger::print("Cleaning up resources...", Logger::INFO);
        Logger::pushContext("Cleanup");
    }
    catch (const std::exception & e)
    {
        Logger::print((std::stringstream{} << "Exception caught: " << e.what()).str(), Logger::ERR);
#ifdef _DEBUG
        throw;
#endif
    }
}
