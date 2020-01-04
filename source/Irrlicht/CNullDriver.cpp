// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CNullDriver.h"
#include "os.h"
#include "IAnimatedMeshSceneNode.h"
#include "irr/asset/CMeshManipulator.h"

#include <new>
#include "IrrlichtDevice.h"

namespace irr
{
namespace video
{

FW_AtomicCounter CNullDriver::ReallocationCounter(0);
int32_t CNullDriver::incrementAndFetchReallocCounter()
{
// omg this has to be rewritten
#if defined(FW_MUTEX_H_CXX11_IMPL)
	return ReallocationCounter += 1;
#elif _MSC_VER && !__INTEL_COMPILER
    return InterlockedIncrement(&ReallocationCounter);
#elif defined(__GNUC__)
    return __sync_add_and_fetch(&ReallocationCounter,int32_t(1));
#endif // _MSC_VER
}

//! constructor
CNullDriver::CNullDriver(IrrlichtDevice* dev, io::IFileSystem* io, const SIrrlichtCreationParameters& _params)
			: IVideoDriver(dev), FileSystem(io), ViewPort(0,0,0,0), Params(_params), PrimitivesDrawn(0), DerivativeMapCreator()
{
	#ifdef _IRR_DEBUG
	setDebugName("CNullDriver");
	#endif

    for (size_t i = 0; i < EQOT_COUNT; i++)
        currentQuery[i] = nullptr;

	ViewPort = core::rect<int32_t>(core::position2d<int32_t>(0,0), Params.WindowSize);


	if (FileSystem)
		FileSystem->grab();

    MaxTextureSizes[IGPUImageView::ET_1D][0] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_1D][1] = 0x1u;
    MaxTextureSizes[IGPUImageView::ET_1D][2] = 0x1u;

    MaxTextureSizes[IGPUImageView::ET_2D][0] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_2D][1] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_2D][2] = 0x1u;

    MaxTextureSizes[IGPUImageView::ET_3D][0] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_3D][1] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_3D][2] = 0x80u;

    MaxTextureSizes[IGPUImageView::ET_1D_ARRAY][0] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_1D_ARRAY][1] = 0x1u;
    MaxTextureSizes[IGPUImageView::ET_1D_ARRAY][2] = 0x800u;

    MaxTextureSizes[IGPUImageView::ET_2D_ARRAY][0] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_2D_ARRAY][1] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_2D_ARRAY][2] = 0x800u;

    MaxTextureSizes[IGPUImageView::ET_CUBE_MAP][0] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_CUBE_MAP][1] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_CUBE_MAP][2] = 0x6u;

    MaxTextureSizes[IGPUImageView::ET_CUBE_MAP_ARRAY][0] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_CUBE_MAP_ARRAY][1] = 0x80u;
    MaxTextureSizes[IGPUImageView::ET_CUBE_MAP_ARRAY][2] = 0x800u*6;


	// set ExposedData to 0
	memset(&ExposedData, 0, sizeof(ExposedData));
}


//! destructor
CNullDriver::~CNullDriver()
{
	if (FileSystem)
		FileSystem->drop();
}

//!
bool CNullDriver::genericDriverInit(asset::IAssetManager* assMgr)
{
	core::stringw namePrint = L"Using renderer: ";
	namePrint += getName();
	os::Printer::log(namePrint.c_str(), ELL_INFORMATION);

	// down
	{
		auto reqs = getDownStreamingMemoryReqs();
		reqs.vulkanReqs.size = Params.StreamingDownloadBufferSize;
		reqs.vulkanReqs.alignment = 64u * 1024u; // if you need larger alignments then you're not right in the head
		defaultDownloadBuffer = core::make_smart_refctd_ptr<video::StreamingTransientDataBufferMT<> >(this, reqs);
	}
	// up
	{
		auto reqs = getUpStreamingMemoryReqs();
		reqs.vulkanReqs.size = Params.StreamingUploadBufferSize;
		reqs.vulkanReqs.alignment = 64u * 1024u; // if you need larger alignments then you're not right in the head
		defaultUploadBuffer = core::make_smart_refctd_ptr < video::StreamingTransientDataBufferMT<> >(this, reqs);
	}


	auto addBuiltInToCaches = [&](auto asset, const char* path) -> void
	{
		asset::SAssetBundle bundle({asset});
		assMgr->changeAssetKey(bundle,path);
		assMgr->insertAssetIntoCache(bundle);
		auto objects = getGPUObjectsFromAssets(&asset.get(),&asset.get()+1);
		assMgr->convertAssetToEmptyCacheHandle(asset.get(),std::move(objects->front()));
	};
	// materials
	{
		//
		auto buildInShader = [&](const char* source, asset::ISpecializedShader::E_SHADER_STAGE type, const char* path) -> void
		{
			auto shader = core::make_smart_refctd_ptr<asset::ICPUSpecializedShader>(core::make_smart_refctd_ptr<asset::ICPUShader>(source),
																					asset::ISpecializedShader::SInfo({},nullptr,"main",type));
			addBuiltInToCaches(shader,path);
		};
		buildInShader(R"===(
#version 430 core
layout(location = 0) in vec3 vPos;
layout(location = 2) in vec2 vTexCoord;

layout(push_constant, row_major) uniform Block {
	mat4 modelViewProj;
} PushConstants;

layout(location = 0) out vec2 uv;

void main()
{
    gl_Position = PushConstants.modelViewProj*vec4(vPos,1.0);
	uv = vTexCoord;
}
		)===",asset::ISpecializedShader::ESS_VERTEX,"irr/builtin/materials/lambertian/singletexture/specializedshader");
		buildInShader(R"===(
#version 430 core

layout(set = 0, binding = 0) uniform sampler2D albedo;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 pixelColor;

void main()
{
    pixelColor = texture(albedo,uv);
}
		)===",asset::ISpecializedShader::ESS_FRAGMENT,"irr/builtin/materials/lambertian/singletexture/specializedshader");

		constexpr uint32_t bindingCount = 1u;
		asset::ICPUDescriptorSetLayout::SBinding pBindings[bindingCount] = {0u,asset::EDT_COMBINED_IMAGE_SAMPLER,1u,asset::ISpecializedShader::ESS_FRAGMENT,nullptr};
		auto dsLayout = core::make_smart_refctd_ptr<asset::ICPUDescriptorSetLayout>(pBindings,pBindings+bindingCount);
		addBuiltInToCaches(dsLayout,"irr/builtin/materials/lambertian/singletexture/descriptorsetlayout/2");
		//
		constexpr uint32_t pcCount = 1u;
		asset::SPushConstantRange pcRanges[pcCount] = {asset::ISpecializedShader::ESS_VERTEX,0u,sizeof(core::matrix4SIMD)};
		auto pLayout = core::make_smart_refctd_ptr<asset::ICPUPipelineLayout>(pcRanges,pcRanges+pcCount,nullptr,nullptr,core::smart_refctd_ptr(dsLayout),nullptr);
		addBuiltInToCaches(pLayout,"irr/builtin/materials/lambertian/singletexture/pipelinelayout");
	}

	// samplers
	{
		asset::ISampler::SParams params;
		params.TextureWrapU = asset::ISampler::ETC_REPEAT;
		params.TextureWrapV = asset::ISampler::ETC_REPEAT;
		params.TextureWrapW = asset::ISampler::ETC_REPEAT;
		params.BorderColor = asset::ISampler::ETBC_FLOAT_OPAQUE_BLACK;
		params.MinFilter = asset::ISampler::ETF_LINEAR;
		params.MaxFilter = asset::ISampler::ETF_LINEAR;
		params.MipmapMode = asset::ISampler::ESMM_LINEAR;
		params.CompareEnable = false;
		params.CompareFunc = asset::ISampler::ECO_ALWAYS;
		params.AnisotropicFilter = 4u;
        params.LodBias = 0.f;
        params.MinLod = -1000.f;
        params.MaxLod = 1000.f;
		auto sampler = core::make_smart_refctd_ptr<asset::ICPUSampler>(params);
		addBuiltInToCaches(sampler,"irr/builtin/samplers/default");

        params.TextureWrapU = params.TextureWrapV = params.TextureWrapW = asset::ISampler::ETC_CLAMP_TO_BORDER;
        sampler = core::make_smart_refctd_ptr<asset::ICPUSampler>(params);
        addBuiltInToCaches(sampler, "irr/builtin/samplers/default_clamp_to_border");
	}

	DerivativeMapCreator = core::make_smart_refctd_ptr<CDerivativeMapCreator>(this);

	return true;
}


//! applications must call this method before performing any rendering. returns false if failed.
bool CNullDriver::beginScene(bool backBuffer, bool zBuffer, SColor color,
		const SExposedVideoData& videoData, core::rect<int32_t>* sourceRect)
{
	PrimitivesDrawn = 0;
	return true;
}


//! applications must call this method after performing any rendering. returns false if failed.
bool CNullDriver::endScene()
{
	FPSCounter.registerFrame(std::chrono::high_resolution_clock::now(), PrimitivesDrawn);

	return true;
}

void CNullDriver::bindDescriptorSets_generic(const IGPUPipelineLayout* _newLayout, uint32_t _first, uint32_t _count, const IGPUDescriptorSet* const* _descSets, const IGPUPipelineLayout** _destPplnLayouts)
{
    uint32_t compatibilityLimits[IGPUPipelineLayout::DESCRIPTOR_SET_COUNT]{}; //actually more like "compatibility limit + 1" (i.e. 0 mean not comaptible at all)
    for (uint32_t i=0u; i<IGPUPipelineLayout::DESCRIPTOR_SET_COUNT; i++)
    {
        const uint32_t lim = _destPplnLayouts[i] ? //if no descriptor set bound at this index
            _destPplnLayouts[i]->isCompatibleUpToSet(IGPUPipelineLayout::DESCRIPTOR_SET_COUNT-1u, _newLayout) : 0u;

        compatibilityLimits[i] = lim;
    }

    /*
    https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#descriptorsets-compatibility
    When binding a descriptor set (see Descriptor Set Binding) to set number N, if the previously bound descriptor sets for sets zero through N-1 were all bound using compatible pipeline layouts, then performing this binding does not disturb any of the lower numbered sets.
    */
    for (uint32_t i = 0u; i < _first; i++)
        if (compatibilityLimits[i] <= i)
            _destPplnLayouts[i] = nullptr;

    /*
    If, additionally, the previous bound descriptor set for set N was bound using a pipeline layout compatible for set N, then the bindings in sets numbered greater than N are also not disturbed.
    */
    if (compatibilityLimits[_first] <= _first)
        for (uint32_t i = _first+_count; i<IGPUPipelineLayout::DESCRIPTOR_SET_COUNT; i++)
            _destPplnLayouts = nullptr;
}

//! gets the area of the current viewport
const core::rect<int32_t>& CNullDriver::getViewPort() const
{
	return ViewPort;
}

//! returns color format
asset::E_FORMAT CNullDriver::getColorFormat() const
{
	return asset::EF_B5G6R5_UNORM_PACK16;
}


//! returns screen size
const core::dimension2d<uint32_t>& CNullDriver::getScreenSize() const
{
	return Params.WindowSize;
}


//! returns the current render target size,
//! or the screen size if render targets are not implemented
const core::dimension2d<uint32_t>& CNullDriver::getCurrentRenderTargetSize() const
{
	return Params.WindowSize;
}


// returns current frames per second value
int32_t CNullDriver::getFPS() const
{
	return FPSCounter.getFPS();
}



//! returns amount of primitives (mostly triangles) were drawn in the last frame.
//! very useful method for statistics.
uint32_t CNullDriver::getPrimitiveCountDrawn( uint32_t param ) const
{
	return (0 == param) ? FPSCounter.getPrimitive() : (1 == param) ? FPSCounter.getPrimitiveAverage() : FPSCounter.getPrimitiveTotal();
}



//! \return Returns the name of the video driver. Example: In case of the DIRECT3D8
//! driver, it would return "Direct3D8".

const wchar_t* CNullDriver::getName() const
{
	return L"Irrlicht NullDevice";
}

//!
void CNullDriver::drawMeshBuffer(const IGPUMeshBuffer* mb)
{
	if (!mb)
		return;

    uint32_t increment = mb->getInstanceCount();
    switch (mb->getPipeline()->getPrimitiveAssemblyParams().primitiveType)
    {
        case asset::EPT_POINT_LIST:
            increment *= mb->getIndexCount();
            break;
        case asset::EPT_LINE_STRIP:
            increment *= mb->getIndexCount()-1;
            break;
        case asset::EPT_LINE_LIST:
            increment *= mb->getIndexCount()/2;
            break;
        case asset::EPT_TRIANGLE_STRIP:
            increment *= mb->getIndexCount()-2;
            break;
        case asset::EPT_TRIANGLE_FAN:
            increment *= mb->getIndexCount()-2;
            break;
        case asset::EPT_TRIANGLE_LIST:
            increment *= mb->getIndexCount()/3;
            break;
    }
    PrimitivesDrawn += increment;
}

void CNullDriver::beginQuery(IQueryObject* query)
{
    if (!query)
        return; //error

    if (currentQuery[query->getQueryObjectType()])
        return; //error

    query->grab();
    currentQuery[query->getQueryObjectType()] = query;
}
void CNullDriver::endQuery(IQueryObject* query)
{
    if (!query)
        return; //error
    if (currentQuery[query->getQueryObjectType()]!=query)
        return; //error

    if (currentQuery[query->getQueryObjectType()])
        currentQuery[query->getQueryObjectType()]->drop();
    currentQuery[query->getQueryObjectType()] = NULL;
}


//! Only used by the internal engine. Used to notify the driver that
//! the window was resized.
void CNullDriver::OnResize(const core::dimension2d<uint32_t>& size)
{
	Params.WindowSize = size;
}


//! Returns driver and operating system specific data about the IVideoDriver.
const SExposedVideoData& CNullDriver::getExposedVideoData()
{
	return ExposedData;
}


//! Returns type of video driver
E_DRIVER_TYPE CNullDriver::getDriverType() const
{
	return EDT_NULL;
}


//! creates a video driver
IVideoDriver* createNullDriver(IrrlichtDevice* dev, io::IFileSystem* io, const SIrrlichtCreationParameters& params)
{
	CNullDriver* nullDriver = new CNullDriver(dev, io, params);

	return nullDriver;
}

const uint32_t* CNullDriver::getMaxTextureSize(IGPUImageView::E_TYPE type) const
{
    return MaxTextureSizes[type];
}

} // end namespace
} // end namespace
