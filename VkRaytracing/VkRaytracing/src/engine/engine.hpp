#pragma once
#include <string>

#include "camera.hpp"
#include "sdl_window.hpp"

#include "vulkan_queues.hpp"
#include "utils/identifiable.hpp"

class Engine
{
public:
    Engine(const std::string& p_WindowName, SDLWindow::WindowSize p_WindowSize);
    ~Engine();

    void run();

private:
    SDLWindow m_Window;

	Camera m_Cam;

    ResourceID m_DeviceID;
    ResourceID m_SwapchainID;
    ResourceID m_graphicsCmdBufferID;

    ResourceID m_DepthImageID;
    VkImageView m_DepthImageView;

    ResourceID m_RenderPassID;
    ResourceID m_PipelineID;

    QueueSelection m_GraphicsQueuePos;
    QueueSelection m_ComputeQueuePos;
    QueueSelection m_TransferQueuePos;
    QueueSelection m_presentQueuePos;
};

