#ifndef ORDERS_H__
#define ORDERS_H__

#include "types.h"

namespace sf { class Texture; };

namespace sum
{ 
   enum Order_Action
   {
      // Basic Movement
      MOVE_FORWARD,
      MOVE_BACK,
      TURN_NORTH,
      TURN_EAST,
      TURN_SOUTH,
      TURN_WEST,
      TURN_NEAREST_ENEMY,

      FOLLOW_PATH,

      // Basic Fighting
      ATTACK_CLOSEST,
      ATTACK_FARTHEST,
      ATTACK_BIGGEST,
      ATTACK_SMALLEST,
      ATTACK_MOST_ARMORED,
      ATTACK_LEAST_ARMORED,

      // Basic Control Flow
      START_BLOCK,
      END_BLOCK,
      REPEAT,

      WAIT,

      BUMP,

      // Unit specific

      // Monster
      MONSTER_GUARD,
      MONSTER_BURST,

      // Soldier
      SOLDIER_SWITCH_AXE,
      SOLDIER_SWITCH_SPEAR,
      SOLDIER_SWITCH_BOW,

      // Worm
      WORM_HIDE,
      WORM_TRAIL_START,
      WORM_TRAIL_END,

      // Bird
      BIRD_CMD_SHOUT,
      BIRD_CMD_QUIET,
      BIRD_FLY,

      // Bug
      BUG_CAST_FIREBALL,
      BUG_CAST_SHOCK,
      BUG_CAST_DUST,
      BUG_OPEN_WORMHOLE,
      BUG_CLOSE_WORMHOLE,
      BUG_MEDITATE,

      // Player Commands
      // Alert
      PL_ALERT_ALL,
      PL_ALERT_MONSTERS,
      PL_ALERT_SOLDIERS,
      PL_ALERT_WORMS,
      PL_ALERT_BIRDS,
      PL_ALERT_BUGS,
      // Activate
      PL_CMD_GO, // Currently alert units
      PL_CMD_GO_ALL,
      PL_CMD_GO_MONSTERS,
      PL_CMD_GO_SOLDIERS,
      PL_CMD_GO_WORMS,
      PL_CMD_GO_BIRDS,
      PL_CMD_GO_BUGS,

      PL_SET_GROUP,

      PL_DELAY,

      // Player spells
      PL_CAST_HEAL,
      PL_CAST_LIGHTNING,
      PL_CAST_TIMELOCK,
      PL_CAST_SCRY,
      PL_CAST_CONFUSION,

      // Unit summoning
      SUMMON_MONSTER,
      SUMMON_SOLDIER,
      SUMMON_WORM,
      SUMMON_BIRD,
      SUMMON_BUG,

      SKIP,

      FAILED_SUMMON,

      NUM_ACTIONS
   };

   enum Order_Conditional
   {
      TRUE,
      ENEMY_ADJACENT,
      ENEMY_NOT_ADJACENT,
      ENEMY_AHEAD,
      ENEMY_NOT_AHEAD,
      ENEMY_IN_RANGE,
      ENEMY_NOT_IN_RANGE,
      // Remove these?
      ALLY_ADJACENT,
      ALLY_NOT_ADJACENT,
      ALLY_AHEAD,
      ALLY_NOT_AHEAD,
      ALLY_IN_RANGE,
      ALLY_NOT_IN_RANGE,
      //
      HEALTH_UNDER_50,
      HEALTH_OVER_50,
      HEALTH_UNDER_20,
      HEALTH_OVER_20,
      BLOCKED_AHEAD,
      NOT_BLOCKED_AHEAD,

      NUM_CONDITIONALS
   };

#define CAST_DURATION 2
#define MEDITATE_DURATION 10

   struct Order
   {
      Order_Action action;
      Order_Conditional condition;
      int count; // How many times to repeat/sustain action
      int iteration; // Local iteration count - needed for loops

      void initOrder( Order_Action, Order_Conditional, int );
      void logSelf();

      Order( Order_Action, Order_Conditional, int );
      Order( Order_Action, Order_Conditional );
      Order( Order_Action, int );
      Order( Order_Action );
      Order();

      //Order( Order& r );
   };

   sf::Texture *getOrderTexture( Order o );
   void drawOrder( Order o, int x, int y, int size );
   void drawCount( int count, int x, int y , int size, bool plus = false, int char_size = 10 );
   KeybindTarget orderToKeybindTarget( Order o );
   void drawKeybind( KeybindTarget kb, int x, int y, int size, int char_size = 10 );
};

#endif 
