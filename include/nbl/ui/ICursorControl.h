#ifndef __NBL_SYSTEM_I_CURSOR_CONTROL_H_INCLUDED__
#define __NBL_SYSTEM_I_CURSOR_CONTROL_H_INCLUDED__
#include "nbl/core/declarations.h"
#include "nbl/core/decl/Types.h"
#include "nbl/ui/IWindow.h"
namespace nbl::ui
{
	class ICursorControl : public core::IReferenceCounted
	{
	public:
		struct SPosition
		{
			int32_t x, y;
		};
		struct SRelativePosition
		{
			float x, y;
		};
		virtual void setVisible(bool visible) = 0;
		virtual bool isVisible() const = 0;

		// Native OS screen position
		virtual void setPosition(SPosition pos) = 0;
		// NDC vulkan-like coordinates
		virtual void setRelativePosition(SRelativePosition pos) = 0;
		
		// Native OS screen position
		virtual SPosition getPosition() = 0;
		// NDC vulkan-like coordinates
		virtual SRelativePosition getRelativePosition() = 0;

		virtual ~ICursorControl() = default;
	};
}
#endif