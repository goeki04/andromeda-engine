#pragma once
#include "a_rhi_constant_buffer.hpp"
namespace Andromeda
{
    struct FrameConstants
    {
        RHIConstantBuffer cameraUBO;
        RHIConstantBuffer lightUBO;
        RHIConstantBuffer modelUBO;

        void initialize();
    };
}