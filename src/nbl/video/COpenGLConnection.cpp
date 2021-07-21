#include "nbl/video/COpenGLConnection.h"

namespace nbl::video
{

core::smart_refctd_ptr<IAPIConnection> createOpenGLConnection(const SDebugCallback& dbgCb)
{
    return core::make_smart_refctd_ptr<COpenGLConnection>(dbgCb);
}


}