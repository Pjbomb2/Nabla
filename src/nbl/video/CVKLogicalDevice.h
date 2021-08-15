#ifndef __NBL_C_VK_LOGICAL_DEVICE_H_INCLUDED__
#define __NBL_C_VK_LOGICAL_DEVICE_H_INCLUDED__

#include <algorithm>

#include "nbl/video/ILogicalDevice.h"
#include "nbl/video/CVulkanDeviceFunctionTable.h"
#include "nbl/video/CVKSwapchain.h"
#include "nbl/video/CVulkanQueue.h"
#include "nbl/video/CVulkanRenderpass.h"
#include "nbl/video/CVulkanImageView.h"
#include "nbl/video/CVulkanFramebuffer.h"
#include "nbl/video/CVulkanSemaphore.h"
#include "nbl/video/CVulkanFence.h"
#include "nbl/video/CVulkanShader.h"
#include "nbl/video/CVulkanSpecializedShader.h"
#include "nbl/video/CVulkanCommandPool.h"
#include "nbl/video/CVulkanCommandBuffer.h"
#include "nbl/video/CVulkanDescriptorSetLayout.h"
#include "nbl/video/CVulkanSampler.h"
#include "nbl/video/CVulkanPipelineLayout.h"
#include "nbl/video/CVulkanPipelineCache.h"
#include "nbl/video/CVulkanComputePipeline.h"
#include "nbl/video/CVulkanDescriptorPool.h"
#include "nbl/video/CVulkanDescriptorSet.h"
#include "nbl/video/CVulkanMemoryAllocation.h"
#include "nbl/video/CVulkanBuffer.h"
// #include "nbl/video/surface/CSurfaceVulkan.h"

namespace nbl::video
{

// Todo(achal): There are methods in this class which aren't pure virtual in ILogicalDevice,
// need to implement those as well
class CVKLogicalDevice final : public ILogicalDevice
{
public:
    CVKLogicalDevice(IPhysicalDevice* physicalDevice, VkDevice vkdev,
        const SCreationParams& params, core::smart_refctd_ptr<system::ISystem>&& sys)
        : ILogicalDevice(physicalDevice, params), m_vkdev(vkdev), m_devf(vkdev)
    {
        // create actual queue objects
        for (uint32_t i = 0u; i < params.queueParamsCount; ++i)
        {
            const auto& qci = params.queueCreateInfos[i];
            const uint32_t famIx = qci.familyIndex;
            const uint32_t offset = (*m_offsets)[famIx];
            const auto flags = qci.flags;
                    
            for (uint32_t j = 0u; j < qci.count; ++j)
            {
                const float priority = qci.priorities[j];
                        
                VkQueue q;
                // m_devf.vk.vkGetDeviceQueue(m_vkdev, famIx, j, &q);
                vkGetDeviceQueue(m_vkdev, famIx, j, &q);
                        
                // Todo(achal): Kinda weird situation here by passing the same ILogicalDevice
                // refctd_ptr to both CThreadSafeGPUQueueAdapter and CVulkanQueue separately
                const uint32_t ix = offset + j;
                (*m_queues)[ix] = core::make_smart_refctd_ptr<CThreadSafeGPUQueueAdapter>(
                    core::make_smart_refctd_ptr<CVulkanQueue>(
                        core::smart_refctd_ptr<ILogicalDevice>(this), q, famIx, flags, priority),
                    core::smart_refctd_ptr<ILogicalDevice>(this));
            }
        }
    }
            
    ~CVKLogicalDevice()
    {
        m_devf.vk.vkDestroyDevice(m_vkdev, nullptr);
    }
            
    // Todo(achal): Need to hoist out creation
    core::smart_refctd_ptr<ISwapchain> createSwapchain(ISwapchain::SCreationParams&& params) override
    {
        return nullptr; // return core::make_smart_refctd_ptr<CVKSwapchain>(std::move(params), this);
    }
    
    core::smart_refctd_ptr<IGPUSemaphore> createSemaphore() override
    {
        VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        createInfo.pNext = nullptr; // Each pNext member of any structure (including this one) in the pNext chain must be either NULL or a pointer to a valid instance of VkExportSemaphoreCreateInfo, VkExportSemaphoreWin32HandleInfoKHR, or VkSemaphoreTypeCreateInfo
        createInfo.flags = static_cast<VkSemaphoreCreateFlags>(0); // flags must be 0

        VkSemaphore semaphore;
        if (vkCreateSemaphore(m_vkdev, &createInfo, nullptr, &semaphore) == VK_SUCCESS)
        {
            return core::make_smart_refctd_ptr<CVulkanSemaphore>
                (core::smart_refctd_ptr<ILogicalDevice>(this), semaphore);
        }
        else
        {
            return nullptr;
        }
    }
            
    core::smart_refctd_ptr<IGPUEvent> createEvent(IGPUEvent::E_CREATE_FLAGS flags) override
    {
        return nullptr;
    };
            
    IGPUEvent::E_STATUS getEventStatus(const IGPUEvent* _event) override
    {
        return IGPUEvent::E_STATUS::ES_FAILURE;
    }
            
    IGPUEvent::E_STATUS resetEvent(IGPUEvent* _event) override
    {
        return IGPUEvent::E_STATUS::ES_FAILURE;
    }
            
    IGPUEvent::E_STATUS setEvent(IGPUEvent* _event) override
    {
        return IGPUEvent::E_STATUS::ES_FAILURE;
    }
            
    core::smart_refctd_ptr<IGPUFence> createFence(IGPUFence::E_CREATE_FLAGS flags) override
    {
        VkFenceCreateInfo vk_createInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        vk_createInfo.pNext = nullptr; // Each pNext member of any structure (including this one) in the pNext chain must be either NULL or a pointer to a valid instance of VkExportFenceCreateInfo or VkExportFenceWin32HandleInfoKHR
        vk_createInfo.flags = static_cast<VkFenceCreateFlags>(flags);

        VkFence vk_fence;
        if (vkCreateFence(m_vkdev, &vk_createInfo, nullptr, &vk_fence) == VK_SUCCESS)
        {
            return core::make_smart_refctd_ptr<CVulkanFence>(core::smart_refctd_ptr<ILogicalDevice>(this),
                flags, vk_fence);
        }
        else
        {
            return nullptr;
        }
    }
            
    IGPUFence::E_STATUS getFenceStatus(IGPUFence* _fence) override
    {
        return IGPUFence::E_STATUS::ES_ERROR;
    }
            
    // API needs to change. vkResetFences can fail.
    void resetFences(uint32_t _count, IGPUFence*const* _fences) override
    {
        assert(_count < 100);

        VkFence vk_fences[100];
        for (uint32_t i = 0u; i < _count; ++i)
        {
            if (_fences[i]->getAPIType() != EAT_VULKAN)
            {
                // Probably log warning?
                assert(false);
            }

            vk_fences[i] = reinterpret_cast<CVulkanFence*>(_fences[i])->getInternalObject();
        }

        vkResetFences(m_vkdev, _count, vk_fences);
    }
            
    IGPUFence::E_STATUS waitForFences(uint32_t _count, IGPUFence*const* _fences, bool _waitAll, uint64_t _timeout) override
    {
        assert(_count < 100);

        VkFence vk_fences[100];
        for (uint32_t i = 0u; i < _count; ++i)
        {
            if (_fences[i]->getAPIType() != EAT_VULKAN)
            {
                // Probably log warning?
                return IGPUFence::E_STATUS::ES_ERROR;
            }

            vk_fences[i] = reinterpret_cast<CVulkanFence*>(_fences[i])->getInternalObject();
        }

        VkResult result = vkWaitForFences(m_vkdev, _count, vk_fences, _waitAll, _timeout);
        switch (result)
        {
        case VK_SUCCESS:
            return IGPUFence::ES_SUCCESS;
        case VK_TIMEOUT:
            return IGPUFence::ES_TIMEOUT;
        default:
            return IGPUFence::ES_ERROR;
        }
    }
            
    const core::smart_refctd_dynamic_array<std::string> getSupportedGLSLExtensions() const override
    {
        return nullptr;
    }
            
    core::smart_refctd_ptr<IGPUCommandPool> createCommandPool(uint32_t familyIndex, std::underlying_type_t<IGPUCommandPool::E_CREATE_FLAGS> flags) override
    {
        VkCommandPoolCreateInfo vk_createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        vk_createInfo.pNext = nullptr; // pNext must be NULL
        vk_createInfo.flags = static_cast<VkCommandPoolCreateFlags>(flags);
        vk_createInfo.queueFamilyIndex = familyIndex;

        VkCommandPool vk_commandPool = VK_NULL_HANDLE;
        if (vkCreateCommandPool(m_vkdev, &vk_createInfo, nullptr, &vk_commandPool) == VK_SUCCESS)
        {
            return core::make_smart_refctd_ptr<CVulkanCommandPool>(
                core::smart_refctd_ptr<ILogicalDevice>(this), flags, familyIndex, vk_commandPool);
        }
        else
        {
            return nullptr;
        }
    }
            
    core::smart_refctd_ptr<IDescriptorPool> createDescriptorPool(
        IDescriptorPool::E_CREATE_FLAGS flags, uint32_t maxSets, uint32_t poolSizeCount,
        const IDescriptorPool::SDescriptorPoolSize* poolSizes) override
    {
        constexpr uint32_t MAX_DESCRIPTOR_POOL_SIZE_COUNT = 100u;

        assert(poolSizeCount <= MAX_DESCRIPTOR_POOL_SIZE_COUNT);

        // I wonder if I can memcpy the entire array
        VkDescriptorPoolSize vk_descriptorPoolSizes[MAX_DESCRIPTOR_POOL_SIZE_COUNT];
        for (uint32_t i = 0u; i < poolSizeCount; ++i)
        {
            vk_descriptorPoolSizes[i].type = static_cast<VkDescriptorType>(poolSizes[i].type);
            vk_descriptorPoolSizes[i].descriptorCount = poolSizes[i].count;
        }

        VkDescriptorPoolCreateInfo vk_createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        vk_createInfo.pNext = nullptr; // Each pNext member of any structure (including this one) in the pNext chain must be either NULL or a pointer to a valid instance of VkDescriptorPoolInlineUniformBlockCreateInfoEXT or VkMutableDescriptorTypeCreateInfoVALVE
        vk_createInfo.flags = static_cast<VkDescriptorPoolCreateFlags>(flags);
        vk_createInfo.maxSets = maxSets;
        vk_createInfo.poolSizeCount = poolSizeCount;
        vk_createInfo.pPoolSizes = vk_descriptorPoolSizes;

        VkDescriptorPool vk_descriptorPool;
        if (vkCreateDescriptorPool(m_vkdev, &vk_createInfo, nullptr, &vk_descriptorPool) == VK_SUCCESS)
        {
            return core::make_smart_refctd_ptr<CVulkanDescriptorPool>(core::smart_refctd_ptr<ILogicalDevice>(this),
                vk_descriptorPool);
        }
        else
        {
            return nullptr;
        }
    }
            
    core::smart_refctd_ptr<IGPURenderpass> createGPURenderpass(const IGPURenderpass::SCreationParams& params) override
    {
        // Todo(achal): Hoist creation out of constructor
        return nullptr; // return core::make_smart_refctd_ptr<CVulkanRenderpass>(this, params);
    }
            
    void flushMappedMemoryRanges(core::SRange<const video::IDriverMemoryAllocation::MappedMemoryRange> ranges) override
    {
        return;
    }
            
    void invalidateMappedMemoryRanges(core::SRange<const video::IDriverMemoryAllocation::MappedMemoryRange> ranges) override
    {
        return;
    }

    //! Binds memory allocation to provide the backing for the resource.
    /** Available only on Vulkan, in OpenGL all resources create their own memory implicitly,
    so pooling or aliasing memory for different resources is not possible.
    There is no unbind, so once memory is bound it remains bound until you destroy the resource object.
    Actually all resource classes in OpenGL implement both IDriverMemoryBacked and IDriverMemoryAllocation,
    so effectively the memory is pre-bound at the time of creation.
    \return true on success, always false under OpenGL.*/
    bool bindBufferMemory(uint32_t bindInfoCount, const SBindBufferMemoryInfo* pBindInfos) override
    {
        for (uint32_t i = 0u; i < bindInfoCount; ++i)
        {
            if ((pBindInfos[i].buffer->getAPIType() != EAT_VULKAN) /*|| (pBindInfos[i].memory->getAPIType() != EAT_VULKAN)*/)
                continue;

            VkBuffer vk_buffer = static_cast<const CVulkanBuffer*>(pBindInfos[i].buffer)->getInternalObject();
            VkDeviceMemory vk_memory = static_cast<const CVulkanMemoryAllocation*>(pBindInfos[i].memory)->getInternalObject();
            if (vkBindBufferMemory(m_vkdev, vk_buffer, vk_memory, static_cast<VkDeviceSize>(pBindInfos[i].offset)) != VK_SUCCESS)
                return false;
        }

        return true;
    }

    core::smart_refctd_ptr<IGPUBuffer> createGPUBuffer(
        const IDriverMemoryBacked::SDriverMemoryRequirements& initialMreqs,
        const bool canModifySubData = false) override
    {
        // Todo(achal): I would probably need to create an IGPUBuffer::SCreationParams
        // Not sure about the course of action to resolve this yet.
        VkBufferCreateInfo vk_createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        vk_createInfo.pNext = nullptr; // Each pNext member of any structure (including this one) in the pNext chain must be either NULL or a pointer to a valid instance of VkBufferDeviceAddressCreateInfoEXT, VkBufferOpaqueCaptureAddressCreateInfo, VkDedicatedAllocationBufferCreateInfoNV, VkExternalMemoryBufferCreateInfo, VkVideoProfileKHR, or VkVideoProfilesKHR
        vk_createInfo.flags = 0; // currently no way to specify these, could nbl::asset::IImage::E_CREATE_FLAGS be used here, but then
        vk_createInfo.size = initialMreqs.vulkanReqs.size;
        vk_createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; // currently no way to specify this, its high time though
        vk_createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; 
        vk_createInfo.queueFamilyIndexCount = 0u; 
        vk_createInfo.pQueueFamilyIndices = nullptr;

        VkBuffer vk_buffer;
        if (vkCreateBuffer(m_vkdev, &vk_createInfo, nullptr, &vk_buffer) == VK_SUCCESS)
        {
            IDriverMemoryBacked::SDriverMemoryRequirements bufferMemoryReqs = {};

            // Not sure if I'd actually need these
            bufferMemoryReqs.memoryHeapLocation = initialMreqs.memoryHeapLocation;
            bufferMemoryReqs.mappingCapability = initialMreqs.mappingCapability;
            bufferMemoryReqs.prefersDedicatedAllocation = initialMreqs.prefersDedicatedAllocation;
            bufferMemoryReqs.requiresDedicatedAllocation = initialMreqs.requiresDedicatedAllocation;

            vkGetBufferMemoryRequirements(m_vkdev, vk_buffer, &bufferMemoryReqs.vulkanReqs);

            return core::make_smart_refctd_ptr<CVulkanBuffer>(core::smart_refctd_ptr<ILogicalDevice>(this),
                bufferMemoryReqs, vk_buffer);
        }
        else
        {
            return nullptr;
        }
    }

    //! Low level function used to implement the above, use with caution
    core::smart_refctd_ptr<IGPUBuffer> createGPUBufferOnDedMem(const IDriverMemoryBacked::SDriverMemoryRequirements& initialMreqs, const bool canModifySubData = false) override
    {
        return nullptr;
    }
        
    core::smart_refctd_ptr<IGPUShader> createGPUShader(core::smart_refctd_ptr<asset::ICPUShader>&& cpushader) override
    {
        const asset::ICPUBuffer* source = cpushader->getSPVorGLSL();
        core::smart_refctd_ptr<asset::ICPUBuffer> clone =
            core::smart_refctd_ptr_static_cast<asset::ICPUBuffer>(source->clone(1u));
        if (cpushader->containsGLSL())
        {
            return core::make_smart_refctd_ptr<CVulkanShader>(
                core::smart_refctd_ptr<ILogicalDevice>(this), std::move(clone),
                IGPUShader::buffer_contains_glsl);
        }
        else
        {
            return core::make_smart_refctd_ptr<CVulkanShader>(core::smart_refctd_ptr<ILogicalDevice>(this),
                std::move(clone));
        }
    }

    core::smart_refctd_ptr<IGPUImage> createGPUImage(asset::IImage::SCreationParams&& params) override
    {
#if 0
        VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        createInfo.flags = static_cast<VkImageCreateFlags>(params.flags);
        createInfo.imageType = createInfo.imageType = static_cast<VkImageType>(params.type);
        createInfo.format = ISurfaceVK::getVkFormat(params.format);
        createInfo.extent = { params.extent.width, params.extent.height, params.extent.depth };
        createInfo.mipLevels = params.mipLevels;
        createInfo.arrayLayers = params.arrayLayers;
        createInfo.samples = static_cast<VkSampleCountFlagBits>(params.samples);
        createInfo.tiling = static_cast<VkImageTiling>(params.tiling);
        createInfo.usage = static_cast<VkImageUsageFlags>(params.usage);
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Todo(achal): enumize this
        createInfo.queueFamilyIndexCount = params.queueFamilyIndices->size();
        createInfo.pQueueFamilyIndices = params.queueFamilyIndices->data();
        createInfo.initialLayout = static_cast<VkImageLayout>(params.initialLayout);

        VkImage vk_image;
        assert(vkCreateImage(m_vkdev, &createInfo, nullptr, &vk_image) == VK_SUCCESS); // Todo(achal): error handling

        return core::make_smart_refctd_ptr<CVulkanImage>(this, std::move(params));
#endif
        return nullptr;
    }

    //! The counterpart of @see bindBufferMemory for images
    bool bindImageMemory(uint32_t bindInfoCount, const SBindImageMemoryInfo* pBindInfos) override
    {
        return false;
    }
            
    core::smart_refctd_ptr<IGPUImage> createGPUImageOnDedMem(IGPUImage::SCreationParams&& params, const IDriverMemoryBacked::SDriverMemoryRequirements& initialMreqs) override
    {
#if 0
        VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        createInfo.flags = static_cast<VkImageCreateFlags>(params.flags);
        createInfo.imageType = createInfo.imageType = static_cast<VkImageType>(params.type);
        createInfo.format = ISurfaceVK::getVkFormat(params.format);
        createInfo.extent = { params.extent.width, params.extent.height, params.extent.depth };
        createInfo.mipLevels = params.mipLevels;
        createInfo.arrayLayers = params.arrayLayers;
        createInfo.samples = static_cast<VkSampleCountFlagBits>(params.samples);
        createInfo.tiling = static_cast<VkImageTiling>(params.tiling);
        createInfo.usage = static_cast<VkImageUsageFlags>(params.usage);
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Todo(achal): enumize this
        createInfo.queueFamilyIndexCount = params.queueFamilyIndices->size();
        createInfo.pQueueFamilyIndices = params.queueFamilyIndices->data();
        createInfo.initialLayout = static_cast<VkImageLayout>(params.initialLayout);

        VkImage vk_image;
        assert(vkCreateImage(m_vkdev, &createInfo, nullptr, &vk_image) == VK_SUCCESS); // Todo(achal): error handling

        return core::make_smart_refctd_ptr<CVulkanImage>(this, std::move(params));
#endif
        return nullptr;
    }

    void updateDescriptorSets(uint32_t descriptorWriteCount, const IGPUDescriptorSet::SWriteDescriptorSet* pDescriptorWrites,
        uint32_t descriptorCopyCount, const IGPUDescriptorSet::SCopyDescriptorSet* pDescriptorCopies) override
    {
        constexpr uint32_t MAX_DESCRIPTOR_WRITE_COUNT = 100u;
        constexpr uint32_t MAX_DESCRIPTOR_COPY_COUNT = 100u;
        constexpr uint32_t MAX_DESCRIPTOR_ARRAY_COUNT = MAX_DESCRIPTOR_WRITE_COUNT;

        // Todo(achal): This exceeds 16384 bytes on stack, move to heap

        assert(descriptorWriteCount <= MAX_DESCRIPTOR_WRITE_COUNT);
        VkWriteDescriptorSet vk_writeDescriptorSets[MAX_DESCRIPTOR_WRITE_COUNT];

        uint32_t bufferInfoOffset = 0u;
        VkDescriptorBufferInfo vk_bufferInfos[MAX_DESCRIPTOR_WRITE_COUNT * MAX_DESCRIPTOR_ARRAY_COUNT];

        uint32_t imageInfoOffset = 0u;
        VkDescriptorImageInfo vk_imageInfos[MAX_DESCRIPTOR_WRITE_COUNT * MAX_DESCRIPTOR_ARRAY_COUNT];

        uint32_t bufferViewOffset = 0u;
        VkBufferView vk_bufferViews[MAX_DESCRIPTOR_WRITE_COUNT * MAX_DESCRIPTOR_ARRAY_COUNT];

        for (uint32_t i = 0u; i < descriptorWriteCount; ++i)
        {
            vk_writeDescriptorSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vk_writeDescriptorSets[i].pNext = nullptr; // Each pNext member of any structure (including this one) in the pNext chain must be either NULL or a pointer to a valid instance of VkWriteDescriptorSetAccelerationStructureKHR, VkWriteDescriptorSetAccelerationStructureNV, or VkWriteDescriptorSetInlineUniformBlockEXT

            const IGPUDescriptorSetLayout* layout = pDescriptorWrites[i].dstSet->getLayout();
            if (layout->getAPIType() != EAT_VULKAN)
                continue;

            const CVulkanDescriptorSet* vulkanDescriptorSet = static_cast<const CVulkanDescriptorSet*>(pDescriptorWrites[i].dstSet);
            vk_writeDescriptorSets[i].dstSet = vulkanDescriptorSet->getInternalObject();

            vk_writeDescriptorSets[i].dstBinding = pDescriptorWrites[i].binding;
            vk_writeDescriptorSets[i].dstArrayElement = pDescriptorWrites[i].arrayElement;
            vk_writeDescriptorSets[i].descriptorCount = pDescriptorWrites[i].count;
            vk_writeDescriptorSets[i].descriptorType = static_cast<VkDescriptorType>(pDescriptorWrites[i].descriptorType);

            assert(pDescriptorWrites[i].count <= MAX_DESCRIPTOR_ARRAY_COUNT);

            switch (pDescriptorWrites[i].info->desc->getTypeCategory())
            {
                case asset::IDescriptor::EC_BUFFER:
                {
                    for (uint32_t j = 0u; j < pDescriptorWrites[i].count; ++j)
                    {
                        // if (pDescriptorWrites[i].info[j].desc->getAPIType() != EAT_VULKAN)
                        //     continue;

                        VkBuffer vk_buffer = static_cast<const CVulkanBuffer*>(pDescriptorWrites[i].info[j].desc.get())->getInternalObject();

                        vk_bufferInfos[j].buffer = vk_buffer;
                        vk_bufferInfos[j].offset = pDescriptorWrites[i].info[j].buffer.offset;
                        vk_bufferInfos[j].range = pDescriptorWrites[i].info[j].buffer.size;
                    }

                    vk_writeDescriptorSets[i].pBufferInfo = vk_bufferInfos + bufferInfoOffset;
                    bufferInfoOffset += pDescriptorWrites[i].count;
                } break;

                case asset::IDescriptor::EC_IMAGE:
                {
                    for (uint32_t j = 0u; j < pDescriptorWrites[i].count; ++j)
                    {
                        auto descriptorWriteImageInfo = pDescriptorWrites[i].info[j].image;

                        VkSampler vk_sampler = VK_NULL_HANDLE;
                        if (descriptorWriteImageInfo.sampler && (descriptorWriteImageInfo.sampler->getAPIType() == EAT_VULKAN))
                            vk_sampler = static_cast<const CVulkanSampler*>(descriptorWriteImageInfo.sampler.get())->getInternalObject();

                        // if (pDescriptorWrites[i].info[j].desc->getAPIType() != EAT_VULKAN)
                        //     continue;
                        VkImageView vk_imageView = static_cast<const CVulkanImageView*>(pDescriptorWrites[i].info[j].desc.get())->getInternalObject();

                        vk_imageInfos[j].sampler = vk_sampler;
                        vk_imageInfos[j].imageView = vk_imageView;
                        vk_imageInfos[j].imageLayout = static_cast<VkImageLayout>(descriptorWriteImageInfo.imageLayout);
                    }

                    vk_writeDescriptorSets[i].pImageInfo = vk_imageInfos + imageInfoOffset;
                    imageInfoOffset += pDescriptorWrites[i].count;
                } break;

                case asset::IDescriptor::EC_BUFFER_VIEW:
                {
                    // Todo(achal): Implement when you create the buffer view stuff
                    assert(false);
                } break;
            }
        }

        assert(descriptorCopyCount <= MAX_DESCRIPTOR_COPY_COUNT);
        VkCopyDescriptorSet vk_copyDescriptorSets[MAX_DESCRIPTOR_COPY_COUNT];

        for (uint32_t i = 0u; i < descriptorCopyCount; ++i)
        {
            vk_copyDescriptorSets[i].sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
            vk_copyDescriptorSets[i].pNext = nullptr; // pNext must be NULL

            // if (pDescriptorCopies[i].srcSet->getAPIType() != EAT_VULKAN)
            //     continue;
            vk_copyDescriptorSets[i].srcSet = static_cast<const CVulkanDescriptorSet*>(pDescriptorCopies[i].srcSet)->getInternalObject();

            vk_copyDescriptorSets[i].srcBinding = pDescriptorCopies[i].srcBinding;
            vk_copyDescriptorSets[i].srcArrayElement = pDescriptorCopies[i].srcArrayElement;

            // if (pDescriptorCopies[i].dstSet->getAPIType() != EAT_VULKAN)
            //     continue;
            vk_copyDescriptorSets[i].dstSet = static_cast<const CVulkanDescriptorSet*>(pDescriptorCopies[i].dstSet)->getInternalObject();

            vk_copyDescriptorSets[i].dstBinding = pDescriptorCopies[i].dstBinding;
            vk_copyDescriptorSets[i].dstArrayElement = pDescriptorCopies[i].dstArrayElement;
            vk_copyDescriptorSets[i].descriptorCount = pDescriptorCopies[i].count;
        }

        vkUpdateDescriptorSets(m_vkdev, descriptorWriteCount, vk_writeDescriptorSets, descriptorCopyCount, vk_copyDescriptorSets);
    }

    core::smart_refctd_ptr<IDriverMemoryAllocation> allocateDeviceLocalMemory(
        const IDriverMemoryBacked::SDriverMemoryRequirements& additionalReqs) override
    {
        // Todo(achal): I need to take into account getDeviceLocalGPUMemoryReqs, probably
        
        VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocateInfo.pNext = nullptr; // Todo(achal): What extensions?
        allocateInfo.allocationSize = additionalReqs.vulkanReqs.size;
        allocateInfo.memoryTypeIndex = 2u; // Todo(achal): LOL?!

        VkDeviceMemory vk_deviceMemory;
        if (vkAllocateMemory(m_vkdev, &allocateInfo, nullptr, &vk_deviceMemory) == VK_SUCCESS)
        {
            return core::make_smart_refctd_ptr<CVulkanMemoryAllocation>(this, vk_deviceMemory);
        }
        else
        {
            return nullptr;
        }
    }

    //! If cannot or don't want to use device local memory, then this memory can be used
    /** If the above fails (only possible on vulkan) or we have perfomance hitches due to video memory oversubscription.*/
    core::smart_refctd_ptr<IDriverMemoryAllocation> allocateSpilloverMemory(
        const IDriverMemoryBacked::SDriverMemoryRequirements& additionalReqs) override
    {
        return nullptr;
    }

    //! Best for staging uploads to the GPU, such as resource streaming, and data to update the above memory with
    core::smart_refctd_ptr<IDriverMemoryAllocation> allocateUpStreamingMemory(
        const IDriverMemoryBacked::SDriverMemoryRequirements& additionalReqs) override
    {
        return nullptr;
    }

    //! Best for staging downloads from the GPU, such as query results, Z-Buffer, video frames for recording, etc.
    core::smart_refctd_ptr<IDriverMemoryAllocation> allocateDownStreamingMemory(
        const IDriverMemoryBacked::SDriverMemoryRequirements& additionalReqs) override
    {
        return nullptr;
    }

    //! Should be just as fast to play around with on the CPU as regular malloc'ed memory, but slowest to access with GPU
    core::smart_refctd_ptr<IDriverMemoryAllocation> allocateCPUSideGPUVisibleMemory(
        const IDriverMemoryBacked::SDriverMemoryRequirements& additionalReqs) override
    {
        return nullptr;
    }


    core::smart_refctd_ptr<IGPUSampler> createGPUSampler(const IGPUSampler::SParams& _params) override
    {
        return nullptr;
    }

    // API changes needed, this could also fail.
    void waitIdle() override
    {
        // Todo(achal): Handle errors
        assert(vkDeviceWaitIdle(m_vkdev) == VK_SUCCESS);
    }

    void* mapMemory(const IDriverMemoryAllocation::MappedMemoryRange& memory, IDriverMemoryAllocation::E_MAPPING_CPU_ACCESS_FLAG accessHint = IDriverMemoryAllocation::EMCAF_READ_AND_WRITE) override
    {
        // if (memory.memory->getAPIType() != EAT_VULKAN)
        //     return nullptr;

        VkMemoryMapFlags memoryMapFlags = 0; // reserved for future use, by Vulkan
        VkDeviceMemory vk_memory = static_cast<const CVulkanMemoryAllocation*>(memory.memory)->getInternalObject();
        void* mappedPtr;
        if (vkMapMemory(m_vkdev, vk_memory, static_cast<VkDeviceSize>(memory.offset),
            static_cast<VkDeviceSize>(memory.length), memoryMapFlags, &mappedPtr) == VK_SUCCESS)
        {
            return mappedPtr;
        }
        else
        {
            return nullptr;
        }
    }

    void unmapMemory(IDriverMemoryAllocation* memory) override
    {
        // if (memory.memory->getAPIType() != EAT_VULKAN)
        //     return;

        VkDeviceMemory vk_deviceMemory = static_cast<const CVulkanMemoryAllocation*>(memory)->getInternalObject();
        vkUnmapMemory(m_vkdev, vk_deviceMemory);
    }

    CVulkanDeviceFunctionTable* getFunctionTable() { return &m_devf; }

    VkDevice getInternalObject() const { return m_vkdev; }

protected:
    bool createCommandBuffers_impl(IGPUCommandPool* cmdPool, IGPUCommandBuffer::E_LEVEL level,
        uint32_t count, core::smart_refctd_ptr<IGPUCommandBuffer>* outCmdBufs) override
    {
        if (cmdPool->getAPIType() != EAT_VULKAN)
            return false;

        auto vk_commandPool = reinterpret_cast<CVulkanCommandPool*>(cmdPool)->getInternalObject();

        assert(count <= 100);
        VkCommandBuffer vk_commandBuffers[100];

        VkCommandBufferAllocateInfo vk_allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        vk_allocateInfo.pNext = nullptr; // this must be NULL
        vk_allocateInfo.commandPool = vk_commandPool;
        vk_allocateInfo.level = static_cast<VkCommandBufferLevel>(level);
        vk_allocateInfo.commandBufferCount = count;

        if (vkAllocateCommandBuffers(m_vkdev, &vk_allocateInfo, vk_commandBuffers) == VK_SUCCESS)
        {
            for (uint32_t i = 0u; i < count; ++i)
            {
                outCmdBufs[i] = core::make_smart_refctd_ptr<CVulkanCommandBuffer>(core::smart_refctd_ptr<ILogicalDevice>(this),
                    level, vk_commandBuffers[i], cmdPool);
            }

            return true;
        }
        else
        {
            return false;
        }
    }

    bool freeCommandBuffers_impl(IGPUCommandBuffer** _cmdbufs, uint32_t _count) override
    {
        return false;
    }

    core::smart_refctd_ptr<IGPUFramebuffer> createGPUFramebuffer_impl(IGPUFramebuffer::SCreationParams&& params) override
    {
        // Todo(achal): Hoist creation out of constructor
        return nullptr; // return core::make_smart_refctd_ptr<CVulkanFramebuffer>(this, std::move(params));
    }

    // Todo(achal): For some reason this is not printing shader compilation errors to console
    core::smart_refctd_ptr<IGPUSpecializedShader> createGPUSpecializedShader_impl(const IGPUShader* _unspecialized, const asset::ISpecializedShader::SInfo& specInfo, const asset::ISPIRVOptimizer* spvopt) override
    {
        if (_unspecialized->getAPIType() != EAT_VULKAN)
        {
            return nullptr;
        }
        const CVulkanShader* unspecializedShader = static_cast<const CVulkanShader*>(_unspecialized);

        const std::string& entryPoint = specInfo.entryPoint;
        const asset::ISpecializedShader::E_SHADER_STAGE shaderStage = specInfo.shaderStage;

        core::smart_refctd_ptr<asset::ICPUBuffer> spirv = nullptr;
        if (unspecializedShader->containsGLSL())
        {
            const char* begin = reinterpret_cast<const char*>(unspecializedShader->getSPVorGLSL()->getPointer());
            const char* end = begin + unspecializedShader->getSPVorGLSL()->getSize();
            std::string glsl(begin, end);
            core::smart_refctd_ptr<asset::ICPUShader> glslShader_woIncludes =
                m_physicalDevice->getGLSLCompiler()->resolveIncludeDirectives(glsl.c_str(),
                    shaderStage, specInfo.m_filePathHint.string().c_str());

            spirv = m_physicalDevice->getGLSLCompiler()->compileSPIRVFromGLSL(
                reinterpret_cast<const char*>(glslShader_woIncludes->getSPVorGLSL()->getPointer()),
                shaderStage, entryPoint.c_str(), specInfo.m_filePathHint.string().c_str());
        }
        else
        {
            spirv = unspecializedShader->getSPVorGLSL_refctd();
        }

        // Should just do this check in ISPIRVOptimizer::optimize
        if (!spirv)
            return nullptr;

        if (spvopt)
            spirv = spvopt->optimize(spirv.get(), m_physicalDevice->getDebugCallback()->getLogger());

        if (!spirv)
            return nullptr;

        VkShaderModuleCreateInfo vk_createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        vk_createInfo.pNext = nullptr;
        vk_createInfo.flags = static_cast<VkShaderModuleCreateFlags>(0); // reserved for future use by Vulkan
        vk_createInfo.codeSize = spirv->getSize();
        vk_createInfo.pCode = reinterpret_cast<const uint32_t*>(spirv->getPointer());

        VkShaderModule vk_shaderModule;
        if (vkCreateShaderModule(m_vkdev, &vk_createInfo, nullptr, &vk_shaderModule) == VK_SUCCESS)
        {
            return core::make_smart_refctd_ptr<video::CVulkanSpecializedShader>(
                core::smart_refctd_ptr<ILogicalDevice>(this), vk_shaderModule, shaderStage);
        }
        else
        {
            return nullptr;
        }
    }

    core::smart_refctd_ptr<IGPUBufferView> createGPUBufferView_impl(IGPUBuffer* _underlying, asset::E_FORMAT _fmt, size_t _offset = 0ull, size_t _size = IGPUBufferView::whole_buffer) override
    {
        return nullptr;
    }

    core::smart_refctd_ptr<IGPUImageView> createGPUImageView_impl(IGPUImageView::SCreationParams&& params) override
    {
        // Todo(achal): Hoist creation out of constructor
        return nullptr; // return core::make_smart_refctd_ptr<CVulkanImageView>(this, std::move(params));
    }

    core::smart_refctd_ptr<IGPUDescriptorSet> createGPUDescriptorSet_impl(IDescriptorPool* pool, core::smart_refctd_ptr<const IGPUDescriptorSetLayout>&& layout) override
    {
        if (pool->getAPIType() != EAT_VULKAN)
            return nullptr;

        const CVulkanDescriptorPool* vulkanPool = static_cast<const CVulkanDescriptorPool*>(pool);

        VkDescriptorSetAllocateInfo vk_allocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        vk_allocateInfo.pNext = nullptr; // pNext must be NULL or a pointer to a valid instance of VkDescriptorSetVariableDescriptorCountAllocateInfo

        vk_allocateInfo.descriptorPool = vulkanPool->getInternalObject();
        vk_allocateInfo.descriptorSetCount = 1u; // Isn't creating only descriptor set every time wasteful?

        if (layout->getAPIType() != EAT_VULKAN)
            return nullptr;
        VkDescriptorSetLayout vk_dsLayout = static_cast<const CVulkanDescriptorSetLayout*>(layout.get())->getInternalObject();
        vk_allocateInfo.pSetLayouts = &vk_dsLayout;

        VkDescriptorSet vk_descriptorSet;
        if (vkAllocateDescriptorSets(m_vkdev, &vk_allocateInfo, &vk_descriptorSet) == VK_SUCCESS)
        {
            return core::make_smart_refctd_ptr<CVulkanDescriptorSet>(
                core::smart_refctd_ptr<ILogicalDevice>(this), std::move(layout),
                core::smart_refctd_ptr<const CVulkanDescriptorPool>(vulkanPool),
                vk_descriptorSet);
        }
        else
        {
            return nullptr;
        }
    }

    //
    core::smart_refctd_ptr<IGPUDescriptorSetLayout> createGPUDescriptorSetLayout_impl(const IGPUDescriptorSetLayout::SBinding* _begin, const IGPUDescriptorSetLayout::SBinding* _end) override
    {
#if 0
        VkDescriptorSetLayoutBinding vk_dsLayoutBindings[100];

        uint32_t bindingCount = std::distance(_begin, _end);
        for (uint32_t b = 0u; b < bindingCount; ++b)
        {
            auto binding = _begin + b;

#if 0
            if (binding->samplers)
            {
                assert(binding->count <= 100);
                VkSampler immutableSamplers[100];
                for (uint32_t i = 0u; i < binding->count; ++i)
                {
                    if (binding->samplers[i]->getAPIType() != EAT_VULKAN)
                        continue;

                    immutableSamplers[i] = static_cast<const CVulkanSampler*>(binding->samplers[i].get())->getInternalObject();
                }
            }
#endif

            vk_dsLayoutBindings[b].binding = binding->binding;
            vk_dsLayoutBindings[b].descriptorType = static_cast<VkDescriptorType>(binding->type);
            vk_dsLayoutBindings[b].descriptorCount = binding->count;
            vk_dsLayoutBindings[b].stageFlags = static_cast<VkShaderStageFlags>(binding->stageFlags);
            vk_dsLayoutBindings[b].pImmutableSamplers = nullptr; // Todo(achal)
        }

        VkDescriptorSetLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        createInfo.pNext = nullptr; // Each pNext member of any structure (including this one) in the pNext chain must be either NULL or a pointer to a valid instance of VkDescriptorSetLayoutBindingFlagsCreateInfo or VkMutableDescriptorTypeCreateInfoVALVE
        createInfo.flags = 0; // Todo(achal): I would need to create a IDescriptorSetLayout::SCreationParams for this
        createInfo.bindingCount = bindingCount;
        createInfo.pBindings = vk_dsLayoutBindings;

        VkDescriptorSetLayout vk_dsLayout;
        if (vkCreateDescriptorSetLayout(m_vkdev, &createInfo, nullptr, &vk_dsLayout) == VK_SUCCESS)
        {
            return core::make_smart_refctd_ptr<CVulkanDescriptorSetLayout>(this, _begin, _end, vk_dsLayout);
        }
        else
        {
            return nullptr;
        }
#endif
        return nullptr;
    }

    core::smart_refctd_ptr<IGPUPipelineLayout> createGPUPipelineLayout_impl(const asset::SPushConstantRange* const _pcRangesBegin = nullptr,
        const asset::SPushConstantRange* const _pcRangesEnd = nullptr, core::smart_refctd_ptr<IGPUDescriptorSetLayout>&& layout0 = nullptr,
        core::smart_refctd_ptr<IGPUDescriptorSetLayout>&& layout1 = nullptr, core::smart_refctd_ptr<IGPUDescriptorSetLayout>&& layout2 = nullptr,
        core::smart_refctd_ptr<IGPUDescriptorSetLayout>&& layout3 = nullptr) override
    {
        constexpr uint32_t MAX_DESCRIPTOR_SET_LAYOUT_COUNT = 4u; // temporary max, I believe

        const core::smart_refctd_ptr<IGPUDescriptorSetLayout> tmp[] = { layout0, layout1, layout2,
            layout3 };

        VkDescriptorSetLayout vk_dsLayouts[MAX_DESCRIPTOR_SET_LAYOUT_COUNT];
        uint32_t dsLayoutCount = 0u;
        for (uint32_t i = 0u; i < MAX_DESCRIPTOR_SET_LAYOUT_COUNT; ++i)
        {
            if (tmp[i] && (tmp[i]->getAPIType() == EAT_VULKAN))
                vk_dsLayouts[dsLayoutCount++] = static_cast<const CVulkanDescriptorSetLayout*>(tmp[i].get())->getInternalObject();
        }

        const auto pcRangeCount = std::distance(_pcRangesBegin, _pcRangesEnd);
        assert(pcRangeCount <= 100);
        VkPushConstantRange vk_pushConstantRanges[100];
        for (uint32_t i = 0u; i < pcRangeCount; ++i)
        {
            const auto pcRange = _pcRangesBegin + i;

            vk_pushConstantRanges[i].stageFlags = static_cast<VkShaderStageFlags>(pcRange->stageFlags);
            vk_pushConstantRanges[i].offset = pcRange->offset;
            vk_pushConstantRanges[i].size = pcRange->size;
        }

        VkPipelineLayoutCreateInfo vk_createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        vk_createInfo.pNext = nullptr; // pNext must be NULL
        vk_createInfo.flags = static_cast<VkPipelineLayoutCreateFlags>(0); // flags must be 0
        vk_createInfo.setLayoutCount = dsLayoutCount;
        vk_createInfo.pSetLayouts = vk_dsLayouts;
        vk_createInfo.pushConstantRangeCount = pcRangeCount;
        vk_createInfo.pPushConstantRanges = vk_pushConstantRanges;
                
        VkPipelineLayout vk_pipelineLayout;
        if (vkCreatePipelineLayout(m_vkdev, &vk_createInfo, nullptr, &vk_pipelineLayout) == VK_SUCCESS)
        {
            return core::make_smart_refctd_ptr<CVulkanPipelineLayout>(
                core::smart_refctd_ptr<ILogicalDevice>(this), _pcRangesBegin, _pcRangesEnd,
                std::move(layout0), std::move(layout1), std::move(layout2), std::move(layout3),
                vk_pipelineLayout);
        }
        else
        {
            return nullptr;
        }
    }

    // For consistency's sake why not pass IGPUComputePipeline::SCreationParams as
    // only second argument, like in createGPUComputePipelines_impl below? Especially
    // now, since I've added more members to IGPUComputePipeline::SCreationParams
    core::smart_refctd_ptr<IGPUComputePipeline> createGPUComputePipeline_impl(
        IGPUPipelineCache* _pipelineCache, core::smart_refctd_ptr<IGPUPipelineLayout>&& _layout,
        core::smart_refctd_ptr<IGPUSpecializedShader>&& _shader) override
    {
        core::smart_refctd_ptr<IGPUComputePipeline> result = nullptr;

        IGPUComputePipeline::SCreationParams creationParams = {};
        // Todo(achal): Put E_PIPELINE_CREATE_FLAGS in a different place
        // creationParams.flags = static_cast<asset::E_PIPELINE_CREATE_FLAGS>(0); // No way to get this now!
        creationParams.layout = std::move(_layout);
        creationParams.shader = std::move(_shader);
        // creationParams.basePipeline = nullptr; // No way to get this now!
        // creationParams.basePipelineIndex = ~0u; // No way to get this now!

        core::SRange<const IGPUComputePipeline::SCreationParams> creationParamsRange(&creationParams,
            &creationParams + 1);

        if (createGPUComputePipelines_impl(_pipelineCache, creationParamsRange, &result))
        {
            return result;
        }
        else
        {
            return nullptr;
        }
    }

    bool createGPUComputePipelines_impl(IGPUPipelineCache* pipelineCache,
        core::SRange<const IGPUComputePipeline::SCreationParams> createInfos,
        core::smart_refctd_ptr<IGPUComputePipeline>* output) override
    {
        constexpr uint32_t MAX_PIPELINE_COUNT = 100u;

        assert(createInfos.size() <= MAX_PIPELINE_COUNT);

        const IGPUComputePipeline::SCreationParams* creationParams = createInfos.begin();
        for (size_t i = 0ull; i < createInfos.size(); ++i)
        {
            if ((creationParams[i].layout->getAPIType() != EAT_VULKAN) ||
                (creationParams[i].shader->getAPIType() != EAT_VULKAN))
            {
                // Probably log warning 
                return false;
            }
        }

        VkPipelineCache vk_pipelineCache = VK_NULL_HANDLE;
        if (pipelineCache && pipelineCache->getAPIType() == EAT_VULKAN)
            vk_pipelineCache = static_cast<const CVulkanPipelineCache*>(pipelineCache)->getInternalObject();

        VkPipelineShaderStageCreateInfo vk_shaderStageCreateInfos[MAX_PIPELINE_COUNT];

        VkComputePipelineCreateInfo vk_createInfos[MAX_PIPELINE_COUNT];
        for (size_t i = 0ull; i < createInfos.size(); ++i)
        {
            const auto createInfo = createInfos.begin() + i;

            vk_createInfos[i].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            vk_createInfos[i].pNext = nullptr; // pNext must be either NULL or a pointer to a valid instance of VkPipelineCompilerControlCreateInfoAMD, VkPipelineCreationFeedbackCreateInfoEXT, or VkSubpassShadingPipelineCreateInfoHUAWEI
            // vk_createInfos[i].flags = static_cast<VkPipelineCreateFlags>(createInfo->flags); // Todo(achal)

            if (createInfo->shader->getAPIType() == EAT_VULKAN)
            {
                const CVulkanSpecializedShader* specShader
                    = static_cast<const CVulkanSpecializedShader*>(createInfo->shader.get());

                vk_shaderStageCreateInfos[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                vk_shaderStageCreateInfos[i].pNext = nullptr; // pNext must be NULL or a pointer to a valid instance of VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT
                vk_shaderStageCreateInfos[i].flags = 0; // currently there is no way to get this in the API https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkPipelineShaderStageCreateFlagBits.html
                vk_shaderStageCreateInfos[i].stage = static_cast<VkShaderStageFlagBits>(specShader->getStage());
                vk_shaderStageCreateInfos[i].module = specShader->getInternalObject();
                vk_shaderStageCreateInfos[i].pName = "main"; // Probably want to change the API of IGPUSpecializedShader to have something like getEntryPointName like theres getStage
                vk_shaderStageCreateInfos[i].pSpecializationInfo = nullptr; // Todo(achal): Should we have a asset::ISpecializedShader::SInfo member in CVulkanSpecializedShader, otherwise I don't know how I'm gonna get the values required for VkSpecializationInfo https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkSpecializationInfo.html
            }

            vk_createInfos[i].stage = vk_shaderStageCreateInfos[i];

            vk_createInfos[i].layout = VK_NULL_HANDLE;
            if (createInfo->layout && (createInfo->layout->getAPIType() == EAT_VULKAN))
                vk_createInfos[i].layout = static_cast<const CVulkanPipelineLayout*>(createInfo->layout.get())->getInternalObject();

            vk_createInfos[i].basePipelineHandle = VK_NULL_HANDLE;
            // Todo(achal):
            // if (createInfo->basePipeline && (createInfo->basePipeline->getAPIType() == EAT_VULKAN))
            //     vk_createInfos[i].basePipelineHandle = static_cast<const CVulkanComputePipeline*>(createInfo->basePipeline.get())->getInternalObject();

            // Todo(achal):
            // vk_createInfos[i].basePipelineIndex = createInfo->basePipelineIndex;
        }
        
        VkPipeline vk_pipelines[MAX_PIPELINE_COUNT];
        if (vkCreateComputePipelines(m_vkdev, vk_pipelineCache, static_cast<uint32_t>(createInfos.size()),
            vk_createInfos, nullptr, vk_pipelines) == VK_SUCCESS)
        {
            for (size_t i = 0ull; i < createInfos.size(); ++i)
            {
                const auto createInfo = createInfos.begin() + i;

                output[i] = core::make_smart_refctd_ptr<CVulkanComputePipeline>(
                    core::smart_refctd_ptr<ILogicalDevice>(this),
                    core::smart_refctd_ptr(createInfo->layout),
                    core::smart_refctd_ptr(createInfo->shader), vk_pipelines[i]);
            }
            return true;
        }
        else
        {
            return false;
        }
    }

    core::smart_refctd_ptr<IGPURenderpassIndependentPipeline> createGPURenderpassIndependentPipeline_impl(IGPUPipelineCache* _pipelineCache,
        core::smart_refctd_ptr<IGPUPipelineLayout>&& _layout, IGPUSpecializedShader** _shaders, IGPUSpecializedShader** _shadersEnd,
        const asset::SVertexInputParams& _vertexInputParams, const asset::SBlendParams& _blendParams, const asset::SPrimitiveAssemblyParams& _primAsmParams,
        const asset::SRasterizationParams& _rasterParams) override
    {
        return nullptr;
    }

    bool createGPURenderpassIndependentPipelines_impl(IGPUPipelineCache* pipelineCache, core::SRange<const IGPURenderpassIndependentPipeline::SCreationParams> createInfos,
        core::smart_refctd_ptr<IGPURenderpassIndependentPipeline>* output) override
    {
        return false;
    }

    core::smart_refctd_ptr<IGPUGraphicsPipeline> createGPUGraphicsPipeline_impl(IGPUPipelineCache* pipelineCache, IGPUGraphicsPipeline::SCreationParams&& params) override
    {
        return nullptr;
    }

    bool createGPUGraphicsPipelines_impl(IGPUPipelineCache* pipelineCache, core::SRange<const IGPUGraphicsPipeline::SCreationParams> params, core::smart_refctd_ptr<IGPUGraphicsPipeline>* output) override
    {
        return false;
    }
            
private:
    VkDevice m_vkdev;
    CVulkanDeviceFunctionTable m_devf; // Todo(achal): I don't have a function table yet
};

}

#endif