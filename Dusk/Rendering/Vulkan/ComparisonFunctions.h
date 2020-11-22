/*
    Dusk Source Code
    Copyright (C) 2019 Prevost Baptiste
*/
#pragma once

#if DUSK_VULKAN
#include <Rendering/RenderDevice.h>
#include "vulkan.h"

static constexpr VkCompareOp COMPARISON_FUNCTION_LUT[eComparisonFunction::COMPARISON_FUNCTION_COUNT] = {
    VkCompareOp::VK_COMPARE_OP_NEVER,
    VkCompareOp::VK_COMPARE_OP_ALWAYS,

    VkCompareOp::VK_COMPARE_OP_LESS,
    VkCompareOp::VK_COMPARE_OP_GREATER,

    VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL,
    VkCompareOp::VK_COMPARE_OP_GREATER_OR_EQUAL,

    VkCompareOp::VK_COMPARE_OP_NOT_EQUAL,
    VkCompareOp::VK_COMPARE_OP_EQUAL,
};
#endif
