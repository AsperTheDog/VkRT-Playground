#include "engine.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include "vertex.hpp"
#include "vulkan_context.hpp"
#include "ext/vulkan_deferred_host_operation.hpp"
#include "utils/logger.hpp"

#include "ext/vulkan_extension_management.hpp"
#include "ext/vulkan_raytracing.hpp"
#include "ext/vulkan_shader_clock.hpp"
#include "ext/vulkan_swapchain.hpp"
#include "ext/vulkan_acceleration_structure.hpp"

static VulkanGPU chooseCorrectGPU()
{
    const std::vector<VulkanGPU> gpus = VulkanContext::getGPUs();
    Logger::print("Searching valid GPU", Logger::INFO);
    Logger::pushContext("GPU Selection");
    for (auto& gpu : gpus)
    {
        Logger::print(std::string("Checking GPU: ") + gpu.getProperties().deviceName, Logger::INFO);
        if(!gpu.getFeatures().geometryShader)
            continue;

        if (!std::ranges::any_of(gpu.getSupportedExtensions(), 
            [](const VkExtensionProperties& ext) { return std::string(ext.extensionName) == VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME; }))
            continue;

        GPUQueueStructure queueFamilies = gpu.getQueueFamilies();
        if (!queueFamilies.isQueueFlagSupported(VK_QUEUE_GRAPHICS_BIT))
            continue;

        Logger::print(std::string("Selected GPU: ") + gpu.getProperties().deviceName, Logger::INFO);
        Logger::popContext();
        return gpu;
    }

    throw std::runtime_error("No valid GPU found");
}

Engine::Engine(const std::string& p_WindowName, const SDLWindow::WindowSize p_WindowSize)
    : m_Window(p_WindowName, p_WindowSize), m_Cam({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f })
{
    // Vulkan Instance
    Logger::print("Initializing Engine", Logger::INFO);
    Logger::pushContext("Engine Init");
#ifndef _DEBUG
    VulkanContext::init(VK_API_VERSION_1_3, false, false, m_Window.getRequiredVulkanExtensions());
    bool validationEnabled = false;
#else
    VulkanContext::init(VK_API_VERSION_1_3, true, false, m_Window.getRequiredVulkanExtensions());
    bool validationEnabled = true;
#endif
    Logger::print("Vulkan Context Initialized", Logger::INFO);
    Logger::pushContext("Vulkan Context");
    Logger::print("API Version: 1.3", Logger::INFO);
    Logger::print("Validation layers: " + std::string(validationEnabled ? "enabled" : "not eisabled"), Logger::INFO);
    Logger::popContext();

    // Vulkan Surface
    m_Window.createSurface(VulkanContext::getHandle());

    // Choose Physical Device
    const VulkanGPU gpu = chooseCorrectGPU();

    // Select Queue Families
    const GPUQueueStructure queueStructure = gpu.getQueueFamilies();
    QueueFamilySelector queueFamilySelector(queueStructure);

    const QueueFamily graphicsQueueFamily = queueStructure.findQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    const QueueFamily computeQueueFamily = queueStructure.findQueueFamily(VK_QUEUE_COMPUTE_BIT);
    const QueueFamily presentQueueFamily = queueStructure.findPresentQueueFamily(m_Window.getSurface());
    const QueueFamily transferQueueFamily = queueStructure.findQueueFamily(VK_QUEUE_TRANSFER_BIT);

    // Select Queue Families and assign queues
    QueueFamilySelector selector{ queueStructure };
    selector.selectQueueFamily(graphicsQueueFamily, QueueFamilyTypeBits::GRAPHICS);
    selector.selectQueueFamily(computeQueueFamily, QueueFamilyTypeBits::COMPUTE);
    m_GraphicsQueuePos = selector.getOrAddQueue(graphicsQueueFamily, 1.0);
    m_ComputeQueuePos = selector.addQueue(computeQueueFamily, 1.0);
    m_TransferQueuePos = selector.addQueue(transferQueueFamily, 1.0);
    m_presentQueuePos = selector.addQueue(presentQueueFamily, 1.0);
    Logger::print("Queue Families Selected", Logger::INFO);
    
    // Logical Device
    {
        VulkanDeviceExtensionManager manager{};
        manager.addExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME, new VulkanSwapchainExtension(m_DeviceID));
        manager.addExtension(VK_KHR_SHADER_CLOCK_EXTENSION_NAME, new VulkanShaderClockExtension(m_DeviceID, false, false));
        manager.addExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, new VulkanDeferredHostOperationsExtension(m_DeviceID));
        manager.addExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, new VulkanAccelerationStructureExtension(m_DeviceID, true, false, false, false, false));
        manager.addExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, new VulkanRayTracingPipelineExtension(m_DeviceID, true, false, false, false ,false));

        VkPhysicalDeviceFeatures features = {};
        features.fillModeNonSolid = true;
        features.samplerAnisotropy = true;
        features.shaderInt64 = true;

        m_DeviceID = VulkanContext::createDevice(gpu, selector, &manager, {});
        Logger::print("Logical Device Created", Logger::INFO);
        Logger::pushContext("Logical Device");
        std::vector<const char*> extensions{};
        manager.populateExtensionNames(extensions);
        Logger::print("Extensions: " + std::to_string(extensions.size()), Logger::INFO);
        for (const auto& ext : extensions)
            Logger::print(std::string("- ") + ext, Logger::INFO);
        Logger::popContext();
    }
    VulkanDevice& device = VulkanContext::getDevice(m_DeviceID);

    // Swapchain
    VulkanSwapchainExtension* swapchainExtension = VulkanSwapchainExtension::get(device);
    m_SwapchainID = swapchainExtension->createSwapchain(m_Window.getSurface(), m_Window.getSize().toExtent2D(), { VK_FORMAT_R8G8B8A8_SRGB, VK_COLORSPACE_SRGB_NONLINEAR_KHR });
    const VulkanSwapchain& swapchain = swapchainExtension->getSwapchain(m_SwapchainID);
    Logger::print("Swapchain Created", Logger::INFO);
    Logger::pushContext("Swapchain");
    Logger::print("Image Count: " + std::to_string(swapchain.getImageCount()), Logger::INFO);
    Logger::print(std::string("Format: ") + string_VkFormat(swapchain.getFormat().format), Logger::INFO);
    Logger::print(std::string("Color Space:") + string_VkColorSpaceKHR(swapchain.getFormat().colorSpace), Logger::INFO);
    Logger::print(std::string("Extent: ") + std::to_string(swapchain.getExtent().width) + "x" + std::to_string(swapchain.getExtent().height), Logger::INFO);
    Logger::popContext();

    // Command Buffers
    device.configureOneTimeQueue(m_TransferQueuePos);
    m_graphicsCmdBufferID = device.createCommandBuffer(graphicsQueueFamily, 0, false);
    Logger::print("Command Buffers Created", Logger::INFO);

    // Depth buffer
	const VkFormat depthFormat = device.getGPU().findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, 
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    const VkExtent2D extent = swapchain.getExtent();
	m_DepthImageID = device.createImage(VK_IMAGE_TYPE_2D, depthFormat, {extent.width, extent.height, 1}, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0);
	device.getImage(m_DepthImageID).allocateFromFlags({VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, false});
	m_DepthImageView = device.getImage(m_DepthImageID).createImageView(depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    // Render Pass
    {
        VulkanRenderPassBuilder builder{};

        const VkAttachmentDescription colorAttachment = VulkanRenderPassBuilder::createAttachment(swapchain.getFormat().format, 
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        builder.addAttachment(colorAttachment);

        const VkAttachmentDescription depthAttachment = VulkanRenderPassBuilder::createAttachment(depthFormat,
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        builder.addAttachment(depthAttachment);

        std::vector<VulkanRenderPassBuilder::AttachmentReference> subpassRefs;
        subpassRefs.push_back({COLOR, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
        subpassRefs.push_back({DEPTH_STENCIL, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
        builder.addSubpass(VK_PIPELINE_BIND_POINT_GRAPHICS, subpassRefs, 0);

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        builder.addDependency(dependency);

        m_RenderPassID = device.createRenderPass(builder, 0);
    }

    //Graphics Pipeline
    VulkanPipelineBuilder builder{&device};

	VulkanBinding binding{0, VK_VERTEX_INPUT_RATE_VERTEX, sizeof(Vertex)};
	binding.addAttribDescription(VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos));
	binding.addAttribDescription(VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal));
	binding.addAttribDescription(VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord));
    binding.addAttribDescription(VK_FORMAT_R32_UINT, offsetof(Vertex, materialIndex));

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapchain.getExtent().width);
	viewport.height = static_cast<float>(swapchain.getExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchain.getExtent();

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

    builder.addVertexBinding(binding);
    builder.setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
    builder.setViewportState({viewport}, {scissor});
    builder.setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.setMultisampleState(VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 1.0f);
    builder.setDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
    builder.addColorBlendAttachment(colorBlendAttachment);

    //TODO: Left it at GraphicsPipeline, line 115

    //m_Window.initImgui();
}

Engine::~Engine()
{
    VulkanContext::freeDevice(m_DeviceID);
    m_Window.free();
    VulkanContext::free();
}

void Engine::run()
{
    while (!m_Window.shouldClose())
    {
        //m_Window.pollEvents();


    }
}

