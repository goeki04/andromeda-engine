#include "a_FrameConstants.hpp"
#include "a_shader_generated.hpp"
namespace Andromeda
{
    void FrameConstants::initialize()
    {
        cameraUBO.initialize(sizeof(Generated::CameraBuffer));
        modelUBO.initialize(sizeof(Generated::ObjectBuffer));
        lightUBO.initialize(sizeof(Generated::lights));
    }
}