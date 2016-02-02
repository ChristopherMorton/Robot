#ifndef TYPES_H__
#define TYPES_H__

namespace robot
{

//////////////////////////////////////////////////////////////////////
// Keybind targets ---

typedef enum
{
   KB_NOTHING,

   KB_FORCE_QUIT,

   KB_PAUSE,

   // Debug
   KB_DEBUG_TOGGLE_FRAMERATE,

   KB_COUNT
} KeybindTarget;

}

#endif 
