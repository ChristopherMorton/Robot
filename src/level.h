#ifndef LEVEL_H__
#define LEVEL_H__

#include "types.h"
#include "units.h"
#include "effects.h"

#include <list>

namespace sum
{ 
   extern int level_dim_x, level_dim_y;
   extern Unit **unit_grid;
   extern Terrain *terrain_grid;
   extern TerrainMod *terrain_mod_grid;
   extern std::list<Unit*> listening_units;

   void setLevelListener( bool set = true );
   void setLevelEditorListener( bool set = true );

   int loadLevel( int level_id );
   int updateLevel( int dt );
   int drawLevel();

   int loadLevelEditor( int level );

#define SELECT_CLOSEST 1
#define SELECT_FARTHEST 2
#define SELECT_BIGGEST 3
#define SELECT_SMALLEST 4
#define SELECT_MOST_ARMORED 5
#define SELECT_LEAST_ARMORED 6
   class Unit;
   Unit* getEnemy( int x, int y, float range, Direction dir, Unit *source, int selector, bool ally = false);
   Unit* getEnemyLine( int x, int y, float range, Direction dir, Unit *source, int selector, bool ally = false);
   Unit* getEnemyAdjacent( int x, int y, Unit *source, int selector, bool ally = false);
   Unit* getEnemyBox( int x, int y, int min_x, int max_x, int min_y, int max_y, float range, Unit *source, int selector, bool ally = false );

   bool isVisible( int x, int y );

   bool canMove( int x, int y, int from_x, int from_y );
   bool canMoveUnit( int x, int y, int from_x, int from_y, Unit* u );

   int moveUnit( Unit *u, int new_x, int new_y );
   int addProjectile( Effect_Type t, int team, float x, float y, float speed, float range, Unit* target, float homing = 0, float fastforward = 0 );
   int addEffect( Effect_Type t, float dur, float x, float y, float rot, float fade = 0.0 );
   int addEffectManual( Effect *e );

   int broadcastOrder( Order o, std::list<Unit*> &listeners = listening_units );
   int startPlayerCommand( Order o );
   int completePlayerCommand( Order o );

   // for birds
   int startPlayerCommandRange( Order o, int x_middle, int y_middle, float range, std::list<Unit*> &listener_list );
   int completePlayerCommandRange( Order o, int x_middle, int y_middle, float range, std::list<Unit*> &listener_list );

   // for bugs
   int addWormhole( int x, int y, int &number, bool open );

   void fitGui_Level();
   void fitGui_LevelEditor();

   // Gui related
   extern Unit *selected_unit;

   int initLevelGui();
   int initLevelEditorGui();

   KeybindTarget drawKeybindButtons();
};

#endif
