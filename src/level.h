#ifndef LEVEL_H__
#define LEVEL_H__

#include "types.h"

namespace robot
{ 
   void setLevelListeners( bool set = true );

   int createLevel();
   int updateLevel( int dt );
   int drawLevel();

   int initLevelGui();
};

#endif
