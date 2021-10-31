// Copyright (C) 2018-2020 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h

#include "nbl/video/COpenGLShader.h"

using namespace nbl::core;
using namespace nbl::asset;
using namespace nbl::video;

//
COpenGLShader::COpenGLShader(smart_refctd_ptr<const ILogicalDevice>&& dev, smart_refctd_ptr<ICPUBuffer>&& _spirv) : IGPUShader(std::move(dev)), m_code(std::move(_spirv)), m_containsGLSL(false) {}
COpenGLShader::COpenGLShader(smart_refctd_ptr<const ILogicalDevice>&& dev, smart_refctd_ptr<ICPUBuffer>&& _glsl, buffer_contains_glsl_t buffer_contains_glsl) : IGPUShader(std::move(dev)), m_code(std::move(_glsl)), m_containsGLSL(true) {}

// TODO: deduce the following in COpenGLPhysicalDevice from the OpenGL extension presence, and then add the defines as standardy from IPhysicalDevice : addCommonGLSLDefines() instead
// The subgroup stuff is a bit hard cause we made up "extra" extensions such as `NBL_GL_KHR_shader_subgroup_ballot_inverse_ballot_bit_count` (probably best to expose as extra features)
const char* COpenGLShader::k_openGL2VulkanExtensionMap = R"(
#ifdef NBL_IMPL_GL_AMD_gpu_shader_half_float
#define NBL_GL_EXT_shader_explicit_arithmetic_types_float16
#endif

#ifdef NBL_IMPL_GL_NV_gpu_shader5
#define NBL_GL_EXT_shader_explicit_arithmetic_types_float16
#define NBL_GL_EXT_nonuniform_qualifier
#define NBL_GL_KHR_shader_subgroup_vote_subgroup_any_all_equal_bool
#endif

#ifdef NBL_IMPL_GL_AMD_gpu_shader_int16
#define NBL_GL_EXT_shader_explicit_arithmetic_types_int16
#endif

#ifdef NBL_IMPL_GL_NV_shader_thread_group
#define NBL_GL_KHR_shader_subgroup_ballot_subgroup_mask
#define NBL_GL_KHR_shader_subgroup_basic_subgroup_size
#define NBL_GL_KHR_shader_subgroup_basic_subgroup_invocation_id
#define NBL_GL_KHR_shader_subgroup_ballot_subgroup_ballot
#define NBL_GL_KHR_shader_subgroup_ballot_inverse_ballot_bit_count
#endif

#if defined(NBL_IMPL_GL_ARB_shader_ballot) && defined(NBL_IMPL_GL_ARB_shader_int64)
#define NBL_GL_KHR_shader_subgroup_ballot_subgroup_mask
#define NBL_GL_KHR_shader_subgroup_basic_subgroup_size
#define NBL_GL_KHR_shader_subgroup_basic_subgroup_invocation_id
#define NBL_GL_KHR_shader_subgroup_ballot_subgroup_broadcast_first
#define NBL_GL_KHR_shader_subgroup_ballot_subgroup_ballot
#define NBL_GL_KHR_shader_subgroup_ballot_inverse_ballot_bit_count
#endif

#if defined(NBL_IMPL_GL_AMD_gcn_shader) && (defined(NBL_IMPL_GL_AMD_gpu_shader_int64) || defined(NBL_IMPL_GL_NV_gpu_shader5))
#define NBL_GL_KHR_shader_subgroup_basic_subgroup_size
#define NBL_GL_KHR_shader_subgroup_vote_subgroup_any_all_equal_bool
#endif

#ifdef NBL_IMPL_GL_NV_shader_thread_shuffle
#define NBL_GL_KHR_shader_subgroup_ballot_subgroup_broadcast_first
#endif

#ifdef NBL_IMPL_GL_ARB_shader_group_vote
#define NBL_GL_KHR_shader_subgroup_vote_subgroup_any_all_equal_bool
#endif

#if defined(NBL_GL_KHR_shader_subgroup_ballot_subgroup_broadcast_first) && defined(NBL_GL_KHR_shader_subgroup_vote_subgroup_any_all_equal_bool)
#define NBL_GL_KHR_shader_subgroup_vote_subgroup_all_equal_T
#endif

#if defined(NBL_GL_KHR_shader_subgroup_ballot_subgroup_ballot) && defined(NBL_GL_KHR_shader_subgroup_basic_subgroup_invocation_id)
#define NBL_GL_KHR_shader_subgroup_basic_subgroup_elect
#endif

#ifdef NBL_GL_KHR_shader_subgroup_ballot_subgroup_mask
#define NBL_GL_KHR_shader_subgroup_ballot_inverse_ballot
#define NBL_GL_KHR_shader_subgroup_ballot_inclusive_bit_count
#define NBL_GL_KHR_shader_subgroup_ballot_exclusive_bit_count
#endif

#ifdef NBL_GL_KHR_shader_subgroup_ballot_subgroup_ballot
#define NBL_GL_KHR_shader_subgroup_ballot_bit_count
#endif

// the natural extensions
#ifdef NBL_IMPL_GL_KHR_shader_subgroup_basic
#define NBL_GL_KHR_shader_subgroup_basic
#define NBL_GL_KHR_shader_subgroup_basic_subgroup_size
#define NBL_GL_KHR_shader_subgroup_basic_subgroup_invocation_id
#define NBL_GL_KHR_shader_subgroup_basic_subgroup_elect
#endif

#ifdef NBL_IMPL_GL_KHR_shader_subgroup_vote
#define NBL_GL_KHR_shader_subgroup_vote
#define NBL_GL_KHR_shader_subgroup_vote_subgroup_any_all_equal_bool
#define NBL_GL_KHR_shader_subgroup_vote_subgroup_all_equal_T
#endif

#ifdef NBL_IMPL_GL_KHR_shader_subgroup_ballot
#define NBL_GL_KHR_shader_subgroup_ballot
#define NBL_GL_KHR_shader_subgroup_ballot_bit_count
#define NBL_GL_KHR_shader_subgroup_ballot_subgroup_mask
#define NBL_GL_KHR_shader_subgroup_ballot_subgroup_ballot
#define NBL_GL_KHR_shader_subgroup_ballot_inclusive_bit_count
#define NBL_GL_KHR_shader_subgroup_ballot_exclusive_bit_count
#define NBL_GL_KHR_shader_subgroup_ballot_inverse_ballot_bit_count
#define NBL_GL_KHR_shader_subgroup_ballot_subgroup_broadcast_first
#endif

// TODO: do a SPIR-V Cross contribution to do all the fallbacks (later)
#ifdef NBL_IMPL_GL_KHR_shader_subgroup_shuffle
#define NBL_GL_KHR_shader_subgroup_shuffle
#endif

#ifdef NBL_IMPL_GL_KHR_shader_subgroup_shuffle_relative
#define NBL_GL_KHR_shader_subgroup_shuffle_relative
#endif

#ifdef NBL_IMPL_GL_KHR_shader_subgroup_arithmetic
#define NBL_GL_KHR_shader_subgroup_arithmetic
#endif

#ifdef NBL_IMPL_GL_KHR_shader_subgroup_clustered
#define NBL_GL_KHR_shader_subgroup_clustered
#endif

#ifdef NBL_IMPL_GL_KHR_shader_subgroup_quad
#define NBL_GL_KHR_shader_subgroup_quad
#endif
)";