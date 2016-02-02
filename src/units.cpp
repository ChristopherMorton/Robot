#include "units.h"
#include "level.h"
#include "focus.h"
#include "util.h"
#include "effects.h"
#include "animation.h"
#include "clock.h"
#include "log.h"
#include "config.h"

#include "SFML_GlobalRenderWindow.hpp"
#include "SFML_TextureManager.hpp"

#include <SFML/Graphics.hpp>

#include <set>
#include <cmath>

using namespace sf;
using namespace std;

namespace sum
{

int rand_int = 12345;

#define GRID_AT(GRID,X,Y) (GRID[((X) + ((Y) * level_dim_x))])

//////////////////////////////////////////////////////////////////////
// Constants ---

const int c_death_time = 400;
const int c_death_fade_time = 300;

const int c_player_max_health = 100;
const int c_player_max_orders = 500;

const int c_monster_base_health = 400;
const int c_monster_base_memory = 15;
const float c_monster_base_vision = 4.5;
const float c_monster_base_speed = 0.5;
const float c_monster_base_armor = 5.0;

const int c_monster_claw_damage = 15;
const int c_monster_burst_damage = 35;
const float c_monster_harden_time = 0.2;
const float c_monster_max_hardness_reduction = 0.9;

const int c_soldier_base_health = 220;
const int c_soldier_base_memory = 15;
const float c_soldier_base_vision = 5.5;
const float c_soldier_base_speed = 0.6;
const float c_soldier_base_armor = 3.0;

const int c_soldier_axe_damage = 50;
const int c_soldier_spear_damage = 35;

const int c_worm_base_health = 40;
const int c_worm_base_memory = 15;
const float c_worm_base_vision = 5.5;
const float c_worm_base_speed = 0.3;
const float c_worm_base_armor = 1.0;

const int c_worm_poison_duration = 10;

const int c_bird_base_health = 180;
const int c_bird_base_memory = 24;
const float c_bird_base_vision = 7.5;
const float c_bird_base_speed = 0.7;
const float c_bird_base_armor = 1.0;

const int c_bug_base_health = 100;
const int c_bug_base_memory = 18;
const float c_bug_base_vision = 5.5;
const float c_bug_base_speed = 0.8;
const float c_bug_base_armor = 1.0;
const float c_bug_orb_speed = 3.0;

const int c_shock_damage = 85;

const int c_poison_damage = 10;

//////////////////////////////////////////////////////////////////////
// Base Unit ---

int Unit::prepareBasicOrder( Order &o, bool cond_result )
{
   int nest;
   Terrain t;
   TerrainMod tm;
   bool can_follow;

   if (o.action <= WAIT) {
      switch (o.action) {

         // MOVEMENT
         case FOLLOW_PATH:
            if (cond_result == false) return 0;

            can_follow = false;
            t = GRID_AT(terrain_grid,x_grid,y_grid);
            tm = GRID_AT(terrain_mod_grid,x_grid,y_grid);

            // Change direction based on the exits to the path
            // Mod terrain checks first:
            if (tm == TM_TRAIL_N_END && facing == SOUTH) {
               can_follow = true;
            } else if (tm == TM_TRAIL_S_END && facing == NORTH) {
               can_follow = true;
            } else if (tm == TM_TRAIL_E_END && facing == WEST) {
               can_follow = true;
            } else if (tm == TM_TRAIL_W_END && facing == EAST) {
               can_follow = true;
            } else if (tm == TM_TRAIL_EW && (facing == EAST || facing == WEST)) {
               can_follow = true;
            } else if (tm == TM_TRAIL_NS && (facing == NORTH || facing == SOUTH)) {
               can_follow = true;
            } else if (tm == TM_TRAIL_NE) {
               can_follow = true;
               if (facing == WEST) turnTo( NORTH );
               if (facing == SOUTH) turnTo( EAST );
            } else if (tm == TM_TRAIL_SE) {
               can_follow = true;
               if (facing == WEST) turnTo( SOUTH );
               if (facing == NORTH) turnTo( EAST );
            } else if (tm == TM_TRAIL_SW) {
               can_follow = true;
               if (facing == EAST) turnTo( SOUTH );
               if (facing == NORTH) turnTo( WEST );
            } else if (tm == TM_TRAIL_NW) {
               can_follow = true;
               if (facing == EAST) turnTo( NORTH );
               if (facing == SOUTH) turnTo( WEST );
            }
            // Then normal terrain:
            else if (t == TER_PATH_N_END && facing == SOUTH) {
               can_follow = true;
            } else if (t == TER_PATH_S_END && facing == NORTH) {
               can_follow = true;
            } else if (t == TER_PATH_E_END && facing == WEST) {
               can_follow = true;
            } else if (t == TER_PATH_W_END && facing == EAST) {
               can_follow = true;
            } else if (t == TER_PATH_EW && (facing == EAST || facing == WEST)) {
               can_follow = true;
            } else if (t == TER_PATH_NS && (facing == NORTH || facing == SOUTH)) {
               can_follow = true;
            } else if (t == TER_PATH_NE) {
               can_follow = true;
               if (facing == WEST) turnTo( NORTH );
               if (facing == SOUTH) turnTo( EAST );
            } else if (t == TER_PATH_SE) {
               can_follow = true;
               if (facing == WEST) turnTo( SOUTH );
               if (facing == NORTH) turnTo( EAST );
            } else if (t == TER_PATH_SW) {
               can_follow = true;
               if (facing == EAST) turnTo( SOUTH );
               if (facing == NORTH) turnTo( WEST );
            } else if (t == TER_PATH_NW) {
               can_follow = true;
               if (facing == EAST) turnTo( NORTH );
               if (facing == SOUTH) turnTo( WEST );
            }

            if (can_follow == false || !canMove( x_next, y_next, x_grid, y_grid ))
               return 0;

            return 1;

         case MOVE_BACK:
            if (cond_result == true) {
               // Set new next location
               if (facing == NORTH) { turnTo(SOUTH); facing = NORTH; }
               else if (facing == SOUTH) { turnTo(NORTH); facing = SOUTH; }
               else if (facing == EAST) { turnTo(WEST); facing = EAST; }
               else if (facing == WEST) { turnTo(EAST); facing = WEST; }
            }
         case MOVE_FORWARD:
            if (cond_result == true) {
               // Can we move there? If not, reset *_next values and quit
               if (!canMove( x_next, y_next, x_grid, y_grid )) {
                  turnTo(facing);
                  return 0;
               }

               return 1;
            } else {
               return 0;
            }
         case TURN_NORTH:
            if (cond_result == true)
               turnTo(NORTH);
            return 0;
         case TURN_EAST:
            if (cond_result == true)
               turnTo(EAST);
            return 0;
         case TURN_SOUTH:
            if (cond_result == true)
               turnTo(SOUTH);
            return 0;
         case TURN_WEST:
            if (cond_result == true)
               turnTo(WEST);
            return 0;
         case TURN_NEAREST_ENEMY:
            if (cond_result == true) {
               Unit *u = getEnemy( x_grid, y_grid, attack_range, ALL_DIR, this, SELECT_CLOSEST );
               if (u) {
                  Direction d = getDirection( x_grid, y_grid, u->x_grid, u->y_grid );
                  turnTo(d);
               }
            }
            return 0;

         // ATTACKING

         case ATTACK_CLOSEST:
         case ATTACK_FARTHEST:
         case ATTACK_SMALLEST:
         case ATTACK_BIGGEST:
         case ATTACK_MOST_ARMORED:
         case ATTACK_LEAST_ARMORED:
            if (cond_result == true) {
               done_attack = 0;
               return 1;
            } else {
               return 0;
            }

         // CONTROL

         case START_BLOCK:
            if (cond_result == true && (o.iteration < o.count || o.count == -1)) {
               o.iteration++;
               return 0;
            } else {
               o.iteration = 0;
               nest = 1;
               for (int i = current_order + 1; i < order_count; ++i) {
                  if (order_queue[i].action == START_BLOCK)
                     nest++;

                  if (order_queue[i].action == END_BLOCK) {
                     nest--;
                     if (nest == 0) { // Found matching end block - go to it, then skip
                        current_order = i;
                        return 0;
                     }
                  }
               }
               // Should only reach here if the block isn't closed...
               log("Skipped a START_BLOCK with no matching END_BLOCK");   
               current_order = order_count;
               active = 0;
               return 1;
            }
         case END_BLOCK:
            // Find matching start block, and go to it - that's it
            nest = 1;
            for (int i = current_order - 1; i >= 0; --i) {
               if (order_queue[i].action == END_BLOCK)
                  nest++;

               if (order_queue[i].action == START_BLOCK) {
                  nest--;
                  if (nest == 0) { // Found matching start block - go 1 before it, and skip
                     current_order = i-1;
                     return 0;
                  }
               }
            }
            log("Found an END_BLOCK with no matching START_BLOCK - skipping");
            // Ignore it
            return 0;
         case REPEAT:
            if (cond_result == true && (o.iteration < o.count || o.count == -1)) {
               o.iteration++;
               current_order = -1; // Goto 0
               return 0;
            } else {
               o.iteration = 0;
               return 0;
            }

         case WAIT:
         default:
            return 1;
      }

   }
   else return -1;

}

bool testUnitCanMove( Unit *u )
{
   if (NULL == u) return false;

   bool r = canMoveUnit( u->x_next, u->y_next, u->x_grid, u->y_grid, u );
   if (false == r) {
      u->this_turn_order = Order( BUMP );
   }
   return r;
}

int Unit::startBasicOrder( )
{
   if (this_turn_order.action > WAIT) return -1;

   if (this_turn_order.action == MOVE_FORWARD || 
       this_turn_order.action == MOVE_BACK ||
       this_turn_order.action == FOLLOW_PATH)
   {
      testUnitCanMove( this );
   }

   return 0;
}

int Unit::updateBasicOrder( float dtf, Order o )
{
   if (o.action <= WAIT) { 
      switch(o.action) {
         case MOVE_BACK:
            dtf = -dtf;
         case MOVE_FORWARD:
         case FOLLOW_PATH:
            if (facing == NORTH)
               y_real -= dtf;
            else if (facing == SOUTH)
               y_real += dtf;
            else if (facing == WEST)
               x_real -= dtf;
            else if (facing == EAST)
               x_real += dtf;
            return 0;

         case TURN_NORTH:
         case TURN_EAST:
         case TURN_SOUTH:
         case TURN_WEST:
         case TURN_NEAREST_ENEMY:
            log("ERROR: update on turn order");
            return -2;
            
         case ATTACK_CLOSEST:
         case ATTACK_FARTHEST:
         case ATTACK_SMALLEST:
         case ATTACK_BIGGEST:
         case ATTACK_MOST_ARMORED:
         case ATTACK_LEAST_ARMORED:
            if (!done_attack) {
               if (progress >= speed) {
                  doAttack( o );
               }
            }
            return 0;

         case START_BLOCK:
         case END_BLOCK:
         case REPEAT:
            log("ERROR: update on control order");
            return -2;

         case WAIT:
         default:
            return 0;
      }
   }
   else return -1;
}

int Unit::completeBasicOrder( Order &o )
{
   if (o.action <= WAIT) {
      switch (o.action) {
         case MOVE_BACK:
         case MOVE_FORWARD:
         case FOLLOW_PATH:
            moveUnit( this, x_next, y_next );
            x_grid = x_next;
            y_grid = y_next;
            turnTo(facing); 
         default: 
            return 0;
      }
   }
   else return -1;
}

bool Unit::evaluateConditional( Order_Conditional oc )
{
   bool s = true;
   switch (oc) {
      case TRUE:
         return true;
      case ENEMY_NOT_ADJACENT:
         s = false;
      case ENEMY_ADJACENT:
         return s != !(getEnemyAdjacent( x_grid, y_grid, this, SELECT_CLOSEST, false ) != NULL);
      case ENEMY_NOT_AHEAD:
         s = false;
      case ENEMY_AHEAD:
         return s != !(getEnemyLine( x_grid, y_grid, vision_range, facing, this, SELECT_CLOSEST, false ) != NULL);
      case ENEMY_NOT_IN_RANGE:
         s = false;
      case ENEMY_IN_RANGE:
         return s != !(getEnemy( x_grid, y_grid, vision_range, facing, this, SELECT_CLOSEST, false ) != NULL);
      case HEALTH_UNDER_50:
         return (health / max_health) < 50.0;
      case HEALTH_OVER_50:
         return (health / max_health) > 50.0;
      case HEALTH_UNDER_20:
         return (health / max_health) < 20.0;
      case HEALTH_OVER_20:
         return (health / max_health) > 20.0;
      case NOT_BLOCKED_AHEAD:
         return canMove( x_next, y_next, x_grid, y_grid );
      case BLOCKED_AHEAD:
         return !canMove( x_next, y_next, x_grid, y_grid );
      default:
         return true;
   }
}

bool Unit::testInvis()
{
   if (invis) {
      Unit *u = getEnemyAdjacent( x_grid, y_grid, this, SELECT_CLOSEST, false );
      if (u != NULL) {
         invis = false;
         addEffect( SE_GO_MARKER, 0.3, u->x_real, u->y_real, 0, 0.1 );
      }
   }
   return invis;
}

void Unit::activate()
{
   current_order = 0;
   active = 2;

   if (team == 0)
      addEffect( SE_GO_MARKER, 0.3, x_real, y_real, 0, 0.1 );
}

void Unit::clearOrders()
{
   order_count = 0;
   current_order = 0;
   final_order = 0;
   log("Cleared orders");
}

int Unit::turnTo( Direction face )
{
   facing = face;

   if (face == NORTH) {
      x_next = x_grid;
      y_next = y_grid - 1;
   } else if (face == SOUTH) {
      x_next = x_grid;
      y_next = y_grid + 1;
   } else if (face == EAST) {
      x_next = x_grid + 1;
      y_next = y_grid;
   } else if (face == WEST) {
      x_next = x_grid - 1;
      y_next = y_grid;
   } else if (face == ALL_DIR) {
      return -1;
   }

   return 0;
}

Unit::~Unit()
{ }

void Unit::logOrders()
{
   log("Unit printOrders:");
   for( int i = 0; i < order_count; i ++ )
   {
      if (i == current_order)
         log("CURRENT ORDER:");
      if (i == final_order)
         log("FINAL ORDER:");

      order_queue[i].logSelf();
   }
}

int Unit::addOrder( Order o )
{
   if (o.action <= WAIT) {
      if (order_count >= max_orders) // No more memory
         return -2;

      order_queue[order_count] = o;
      order_count++;
      final_order = order_count;

      return 0;
   } else return -1;
}

int Unit::prepareTurn()
{
   if (alive != 1) return 0;

   if (active == 2) active = 1; // Start an active turn

   this_turn_order = Order( WAIT );

   if (aff_poison) {
      aff_poison--;
      takeDamage( c_poison_damage, DMG_POISON );
   }
   if (aff_burning) {
      aff_burning--;
      takeDamage( 10, DMG_FIRE );
   }
   if (aff_confusion) {
      aff_confusion--;
      if (aff_confusion %= 2)
         return 0;
   }

   if (aff_sleep > 0 || aff_timelock > 0) {
      if (aff_sleep > 0) aff_sleep--;
      if (aff_timelock > 0) aff_timelock--;
      return 0;
   }

   set<int> visited_orders;
   while (active == 1 && current_order != final_order 
         && visited_orders.find( current_order ) == visited_orders.end()) {
      this_turn_order = order_queue[current_order];
      visited_orders.insert( current_order );
      bool decision = evaluateConditional(this_turn_order.condition);
      int r = prepareBasicOrder(order_queue[current_order], decision);
      // if prepareBasicOrder returns 0, it's a 0-length instruction (e.g. turn)
      if (r == 0) {
         current_order++;
         continue;
      }
      else 
         break;
   }

   if (current_order == final_order)
      this_turn_order = Order( WAIT );

   progress = 0;
   return 0;
}

int Unit::startTurn()
{
   if (alive != 1) return 0;

   startBasicOrder();
   return 0;
}

int Unit::update( float dtf )
{
   if (alive < 0) {
      alive += (int) (1000.0 * dtf);
      if (alive >= 0) alive = 0;
   }

   if (alive == 0)
      return 1;

   progress += dtf;
   if (active == 1 && current_order != final_order) {
      Order &o = this_turn_order;
      return updateBasicOrder( dtf, o );
   }
   return 0;
}

// Run completeBasicOrder, update iterations, select next order
int Unit::completeTurn()
{
   if (alive != 1) return 0;

   if (aff_sleep == -1)
      return aff_sleep = 0; // Just woke up

   if (aff_sleep > 0 || aff_timelock > 0)
      return 0;

   if (active == 1 && current_order != final_order) {
      int r = completeBasicOrder(this_turn_order);
      Order &o = order_queue[current_order];
      o.iteration++;
      if (o.iteration >= o.count && o.count != -1) { 
         current_order++;
         o.iteration = 0;
      }
      return r;
   }

   return 0;
}

int Unit::displayDamage( int damage, DamageType type )
{
   if (config::display_damage == false)
      return -1;

   // Find empty slot
   int i;
   bool space = false;
   for ( i = 0; i < 12; ++i) {
      if (dmg_display[i] == NULL) {
         space = true;
         break;
      }
   }

   if (!space)
      return -1;

   DamageDisplay *dmg = new DamageDisplay( damage, type, this, i );

   dmg_display[i] = dmg;
   addEffectManual( dmg );

   return 0;
}

int Unit::takeDamage( float damage, DamageType type )
{
   if (type == DMG_LIGHT) damage -= (2 * armor);
   else if (type == DMG_NORMAL) damage -= armor;

   if (damage <= 0) return 0;

   if (type == DMG_HEAL)
      health += damage;
   else
      health -= damage;

   if (health <= 0) {
      // Dead!
      alive = -( c_death_time + c_death_fade_time );
      return -1;
   }

   if (health >= max_health) {
      damage -= (health - max_health);
      health = max_health;
   }

   if (aff_sleep > 0 &&
         !(type == DMG_POISON || type == DMG_FIRE))
      aff_sleep = -1;

   displayDamage( damage, type );

   return 0;
}

string Unit::descriptor()
{
   return "BASIC UNIT";
}

//////////////////////////////////////////////////////////////////////
// AIUnit ---

// All the animations --


// Dwarves
Animation dwarf_spear_anim_idle;
Animation dwarf_spear_anim_move;
Animation dwarf_spear_anim_attack_start;
Animation dwarf_spear_anim_attack_end;
Animation dwarf_spear_anim_death;

void initAIUnitAnimations()
{
   Texture *t = SFML_TextureManager::getSingleton().getTexture( "DwarfSpearAnimIdle.png" );
   dwarf_spear_anim_idle.load( t, 128, 128, 10, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "DwarfSpearAnimMove.png" );
   dwarf_spear_anim_move.load( t, 128, 128, 12, 1000 );

   //t = SFML_TextureManager::getSingleton().getTexture( "DwarfSpearAnimAttackStart.png" );
   //dwarf_spear_anim_attack_start.load( t, 128, 128, 10, 1000 );

   //t = SFML_TextureManager::getSingleton().getTexture( "DwarfSpearAnimAttackEnd.png" );
   //dwarf_spear_anim_attack_end.load( t, 128, 128, 10, 1000 );

   //t = SFML_TextureManager::getSingleton().getTexture( "DwarfSpearAnimDeath.png" );
   //dwarf_spear_anim_death.load( t, 128, 128, 10, c_death_time );
}

// The general stuff --

// Private
AIUnit::AIUnit()
{ }

AIUnit::AIUnit( UnitType t, int x, int y, Direction face, int my_team )
{
   if ( t < R_HUMAN_ARCHER_T || t >= SUMMONMARKER_T ) {
      alive = 0;
      return;
   }

   alive = 1;

   aff_poison = 0;
   aff_burning = 0;
   aff_confusion = 0;
   aff_sleep = 0;
   aff_timelock = 0;
    
   flying = false;
   invis = false;

   type = t;
   x_grid = x;
   y_grid = y;
   x_real = x + 0.5;
   y_real = y + 0.5;

   turnTo(face);

   max_orders = 0;
   order_queue = NULL;
   clearOrders();

   this_turn_order = Order( WAIT );

   active = 1;
   team = my_team;
   group = 1;

   ai_move_style = MV_FREE_ROAM;
   ai_aggro = AGR_PASSIVE;
   ai_leader = NULL;
   clearWaypoints();

   switch (t) {
      case R_HUMAN_ARCHER_T:
         radius = 0.3;
         health = max_health = 100;
         vision_range = 8.5;
         attack_range = 8.5;
         speed = 0.6;
         armor = 1.0;
         break;
      case M_HUMAN_SWORDSMAN_T:
         radius = 0.3;
         health = max_health = 100;
         vision_range = 5.5;
         attack_range = 1.3;
         speed = 0.5;
         armor = 5.0;
         break;
      case M_DWARF_SPEAR_T:
         radius = 0.4;
         health = max_health = 240;
         vision_range = 4.5;
         attack_range = 2.9;
         speed = 0.6;
         armor = 8.0;
         break;
      default:
         break;
   }

   progress = 0;
   
   win_condition = false;

   for (int i = 0; i < 12; ++i)
      dmg_display[i] = NULL;
}

int AIUnit::doAttack( Order o )
{
   int selector = SELECT_CLOSEST;
   Unit *target = NULL;
   switch(o.action) {
      case ATTACK_CLOSEST:
         selector = SELECT_CLOSEST;
         break;
      case ATTACK_FARTHEST:
         selector = SELECT_FARTHEST;
         break;
      case ATTACK_SMALLEST:
         selector = SELECT_SMALLEST;
         break;
      case ATTACK_BIGGEST:
         selector = SELECT_BIGGEST;
         break;
      case ATTACK_MOST_ARMORED:
         selector = SELECT_MOST_ARMORED;
         break;
      case ATTACK_LEAST_ARMORED:
         selector = SELECT_LEAST_ARMORED;
         break;
      default:
         log("ERROR: doAttack called on non-attack order");
         return -1;
   }

   if (type >= R_HUMAN_ARCHER_T && type <= R_HUMAN_ARCHER_T) {
      // Ranged attacker
      target = getEnemy( x_grid, y_grid, attack_range, facing, this, selector );

      if (target) {
         addProjectile( PR_ARROW, team, x_real, y_real, 4.0, attack_range, target, 0.0, 0.1 );
      }
   }

   if (type >= M_HUMAN_SWORDSMAN_T && type <= M_HUMAN_SWORDSMAN_T) {
      // 1-range Melee attacker

      int t_x = x_grid, t_y = y_grid;
      if (addDirection( facing, t_x, t_y ) != -1)
         target = GRID_AT(unit_grid,t_x,t_y);

      if (target) {
         if (type == M_HUMAN_SWORDSMAN_T)
            target->takeDamage( 35 );
      }
   }
   
   if (type >= M_DWARF_SPEAR_T && type <= M_DWARF_SPEAR_T) {
      // 2-range spear-style Melee attacker

      int min_x = x_grid - 2, max_x = x_grid + 2, min_y = y_grid - 2, max_y = y_grid + 2;
      if (facing == NORTH) max_y = y_grid - 1;
      else if (facing == SOUTH) min_y = y_grid + 1;
      else if (facing == WEST) max_x = x_grid - 1;
      else if (facing == EAST) min_x = x_grid + 1;

      target = getEnemyBox( x_grid, y_grid, min_x, max_x, min_y, max_y, attack_range, this, selector );

      if (target) {
         // Get spear trajectory
         int t_x = target->x_grid, t_y = target->y_grid;
         if (t_x == x_grid + 1) {
            if (t_y == y_grid + 1) {
               t_x = x_grid + 2;
               t_y = y_grid + 2;
            } else if (t_y == y_grid - 1) {
               t_x = x_grid + 2;
               t_y = y_grid - 2;
            } else if (t_y == y_grid)
               t_x = x_grid + 2;
         } else if (t_x == x_grid - 1) {
            if (t_y == y_grid + 1) {
               t_x = x_grid - 2;
               t_y = y_grid + 2;
            } else if (t_y == y_grid - 1) {
               t_x = x_grid - 2;
               t_y = y_grid - 2;
            } else if (t_y == y_grid)
               t_x = x_grid - 2;
         } else if (t_x == x_grid) {
            if (t_y == y_grid - 1) t_y = y_grid - 2;
            else if (t_y == y_grid + 1) t_y = y_grid + 2;
         }
         // Get inner target
         int t_x2 = t_x, t_y2 = t_y;
         if (t_x2 == x_grid + 2) t_x2 = x_grid + 1;
         if (t_x2 == x_grid - 2) t_x2 = x_grid - 1;
         if (t_y2 == y_grid + 2) t_y2 = y_grid + 1;
         if (t_y2 == y_grid - 2) t_y2 = y_grid - 1;

         // Damage targets
         Unit *t1 = GRID_AT(unit_grid,t_x,t_y);
         Unit *t2 = GRID_AT(unit_grid,t_x2,t_y2);
         if (NULL != t1)
            t1->takeDamage( c_soldier_spear_damage );
         if (NULL != t2)
            t2->takeDamage( c_soldier_spear_damage );

         // Animate
         float rot = atan( (t_y - y_grid) / (t_x - x_grid + 0.0001) ) * 180 / 3.1415926;
         addEffect( SE_SPEAR_ANIM, 0.15, x_real, y_real, rot, 0.15 );
      }
   }

   done_attack = 1;
   return 0;
}

int AIUnit::takeDamage( float damage, DamageType type )
{
   if (type == DMG_LIGHT) damage -= (2 * armor);
   else if (type == DMG_NORMAL) damage -= armor;

   if (damage <= 0) return 0;

   if (type == DMG_HEAL)
      health += damage;
   else
      health -= damage;

   if (health <= 0) {
      // Dead!
      alive = -( c_death_time + c_death_fade_time );
      return -1;
   }

   if (health >= max_health) {
      damage -= (health - max_health);
      health = max_health;
   }

   if (ai_aggro == AGR_DEFEND_SELF)
      aggroed = 1;

   if (aff_sleep > 0 &&
         !(type == DMG_POISON || type == DMG_FIRE))
      aff_sleep = -1;

   displayDamage( damage, type );

   return 0;
}

// AI calculations
void AIUnit::setAI( AI_Movement move, AI_Aggression aggro, float chase_dis, float attack_dis, Unit* follow )
{
   ai_move_style = move;
   ai_aggro = aggro;

   ai_chase_distance = chase_dis;
   if (ai_chase_distance > vision_range)
      ai_chase_distance = vision_range;

   ai_attack_distance = attack_dis;
   if (ai_attack_distance > vision_range)
      ai_attack_distance = vision_range;

   ai_leader = follow;
}

int AIUnit::addWaypoint( int x, int y, Direction d )
{
   ai_waypoints.push_back( pair<Vector2i,Direction>(Vector2i( x, y ), d ) );

   if (ai_waypoint_next == -1)
      ai_waypoint_next = 0;

   return ai_waypoints.size();
}

void AIUnit::clearWaypoints()
{
   ai_waypoints.clear();
   ai_waypoint_next = -1;
}

int costEstimateH( int from_x, int from_y, int goal_x, int goal_y )
{
   int dx = from_x - goal_x,
       dy = from_y - goal_y;
   /* Orthoganal score
   int dx = abs(from_x - goal_x),
       dy = abs(from_y - goal_y);

   return dx + dy;
       */
   return (dx * dx) + (dy * dy);
}

int AIUnit::reconstructPath( Direction *came_from_grid, int x, int y )
{
   ai_pathing.clear();

   Direction d;
   while ((d = came_from_grid[ x + (y * level_dim_x) ]) != ALL_DIR) {
      ai_pathing.push_front( reverseDirection( d ) );
      addDirection( d, x, y );
   }

   return 0;
}

struct aStarVector {
   int x;
   int y;
   int g_score;
   int f_score;

   aStarVector( int _x, int _y, int _g_score, int _f_score ) :
      x(_x), y(_y),
      g_score(_g_score),
      f_score(_f_score) {}
};
inline bool operator== (const aStarVector& lhs, const aStarVector& rhs)
{
   return (lhs.x == rhs.x && lhs.y == rhs.y);
}
bool asvLess (const aStarVector& lhs, const aStarVector& rhs)
{
   return ((lhs.x + (lhs.y * 1000)) < (rhs.x + (rhs.y * 1000)));
}
inline bool operator< (const aStarVector& lhs, const aStarVector& rhs)
{
   return asvLess( lhs, rhs );
}

int AIUnit::aStar( int start_x, int start_y, int goal_x, int goal_y )
{
   set<aStarVector> closed_set;
   set<aStarVector> open_set;

   Direction *came_from_grid = new Direction[level_dim_x * level_dim_y];
   for (int i = 0; i < level_dim_x * level_dim_y; ++i) came_from_grid[i] = ALL_DIR;

   closed_set.clear();
   open_set.clear();
   open_set.insert( aStarVector( start_x, start_y, 0, costEstimateH( start_x, start_y, goal_x, goal_y )));

   while( !open_set.empty() ) {
      // Manually search here for a lowest f_score
      set<aStarVector>::iterator it_next, it_best;
      int it_f_score = 1000;
      for (it_next = open_set.begin(); it_next != open_set.end(); ++it_next) {
         if ((*it_next).f_score < it_f_score) {
            it_best = it_next;
            it_f_score = (*it_next).f_score;
         }
      }
      /* Alternate scoring, when f_score is orthoganal distance, split ties on g_score
      int it_f_score = 1000, it_g_score = -1;
      for (it_next = open_set.begin(); it_next != open_set.end(); ++it_next) {
         if ((*it_next).f_score < it_f_score) {
            it_best = it_next;
            it_f_score = (*it_next).f_score;
            it_g_score = (*it_next).g_score;
         } else if ((*it_next).f_score == it_f_score && (*it_next).g_score > it_g_score) {
            it_best = it_next;
            it_f_score = (*it_next).f_score;
            it_g_score = (*it_next).g_score;
         }
      }
      */


      aStarVector asv = (*it_best);

      // Goal reached
      if (asv.x == goal_x && asv.y == goal_y)
         return reconstructPath(came_from_grid, goal_x, goal_y);

      open_set.erase( it_best );
      closed_set.insert( asv );
      // Find neighbors in each Direction
      for (Direction d = NORTH; d != ALL_DIR; d = (Direction)((int)d + 1)) {
         int neighbor_x = asv.x,
             neighbor_y = asv.y;
         addDirection( d, neighbor_x, neighbor_y );
         aStarVector neighbor( neighbor_x, neighbor_y, 0, 0 );
         if (!canMove( neighbor.x, neighbor.y, asv.x, asv.y ))
            continue;
         if (find( closed_set.begin(), closed_set.end(), neighbor) != closed_set.end())
            continue; // Already closed this node

         // Node has not been closed yet
         int tentative_g_score = asv.g_score + 1;

         set<aStarVector>::iterator it_open = find( open_set.begin(), open_set.end(), neighbor);
         if (it_open == open_set.end() || (*it_open).g_score > tentative_g_score) {
            came_from_grid[ neighbor.x + (neighbor.y * level_dim_x) ] = reverseDirection( d );
            neighbor.g_score = tentative_g_score;
            neighbor.f_score = neighbor.g_score + costEstimateH( neighbor.x, neighbor.y, goal_x, goal_y );
            if (it_open != open_set.end())
               open_set.erase( it_open );
            open_set.insert( neighbor );
         }
      }
   }

   return -1;
}
 
int AIUnit::aiCalculatePathing( bool next_waypoint )
{
   if (ai_waypoints.size() == 0) return -1;

   if (next_waypoint) {
      ai_waypoint_next++;
      if (ai_waypoint_next >= ai_waypoints.size())
         ai_waypoint_next = 0;
   }

   Vector2i destination = ai_waypoints.at( ai_waypoint_next ).first;

   aStar( x_grid, y_grid, destination.x, destination.y );

   return 0;
}

int AIUnit::aiFollowPathing()
{
   if (ai_pathing.size() == 0) return -1;

   Direction d = ai_pathing.front();
   turnTo(d);

   if (canMove( x_next, y_next, x_grid, y_grid )) {
      this_turn_order = Order( MOVE_FORWARD );
      ai_pathing.pop_front();
   }
   else
      this_turn_order = Order( BUMP );


   return 0;
}

std::string AIUnit::descriptor()
{
   if (type == R_HUMAN_ARCHER_T)
      return "Human Archer";
   if (type == M_HUMAN_SWORDSMAN_T)
      return "Human Swordsman";
   if (type == M_DWARF_SPEAR_T)
      return "Dwarf Spearman";

   return "I am Error";
}

int AIUnit::ai()
{
   // First see if their aggro is procced
   if (ai_aggro == AGR_FLEE) {
      Unit* enemy = getEnemy( x_grid, y_grid, vision_range, facing, this, SELECT_CLOSEST, false );
      if (enemy != NULL) {
         Direction d = getDirection( enemy->x_grid, enemy->y_grid, x_grid, y_grid );
         turnTo(d);
         if (canMove( x_next, y_next, x_grid, y_grid ))
            this_turn_order = Order( MOVE_FORWARD );
         aggroed = 1;
         return 0;
      }

      if (aggroed == 1) {
         aiCalculatePathing();
      }
      aggroed = 0;
   }

   if (ai_aggro == AGR_PASSIVE)
      aggroed = 0;

   if (ai_aggro == AGR_ATTACK_IN_RANGE || (ai_aggro == AGR_DEFEND_SELF && aggroed == 1)) {
      // Try to attack
      Unit* enemy = getEnemy( x_grid, y_grid, ai_attack_distance, facing, this, SELECT_CLOSEST, false );
      if (enemy != NULL) {
         this_turn_order = Order( ATTACK_CLOSEST );
         done_attack = 0;
         aggroed = 1;
         return 0;
      }

      if (aggroed == 1) {
         aiCalculatePathing();
      }
      aggroed = 0;
   }

   if (ai_aggro == AGR_PURSUE_VISIBLE) {
      // Try to attack
      Unit* enemy = getEnemy( x_grid, y_grid, ai_attack_distance, facing, this, SELECT_CLOSEST, false );
      if (enemy != NULL) {
         this_turn_order = Order( ATTACK_CLOSEST );
         done_attack = 0;
         aggroed = 1;
         return 0;
      }

      // Try to chase
      enemy = getEnemy( x_grid, y_grid, ai_chase_distance, facing, this, SELECT_CLOSEST, false );
      if (enemy != NULL)
      {
         Direction d = getDirection( x_grid, y_grid, enemy->x_grid, enemy->y_grid );
         turnTo(d);
         if (canMove( x_next, y_next, x_grid, y_grid )) {
            this_turn_order = Order( MOVE_FORWARD );
         } else {
            this_turn_order = Order( ATTACK_CLOSEST );
            done_attack = 0;
         }
         aggroed = 1;
         return 0;
      }

      if (aggroed == 1) {
         aiCalculatePathing();
      }
      aggroed = 0;
   }


   // Second, if not aggroed, move in the customary way
   if (aggroed == 0) {

      if (ai_move_style == MV_HOLD_POSITION || ai_move_style == MV_PATROL) {
         if (ai_waypoints.size() == 0) return -1;

         Vector2i h_pos = ai_waypoints.at( ai_waypoint_next ).first;
         if (h_pos.x == x_grid && h_pos.y == y_grid) {// at position
            if (ai_move_style == MV_HOLD_POSITION) {
               turnTo( ai_waypoints.at( ai_waypoint_next ).second );
               this_turn_order = Order( WAIT );
            } else {
               aiCalculatePathing( true );
               return aiFollowPathing();
            }
            return 0;
         }
         else
            return aiFollowPathing();
      }

      if (ai_move_style == MV_PATROL_PATH) {
         if (ai_waypoints.size() == 0) return -1;

         Vector2i h_pos = ai_waypoints.front().first;
         if (h_pos.x == x_grid && h_pos.y == y_grid) {// at position
            this_turn_order = Order( FOLLOW_PATH );
            if (prepareBasicOrder(this_turn_order, true) == 0)
               this_turn_order = Order( WAIT );
            return 0;
         }
         else
            return aiFollowPathing();
      }

      if (ai_move_style == MV_FREE_ROAM) {
         this_turn_order = Order( WAIT );
         return 0;
      }
   }

   return 0;
}

int AIUnit::prepareTurn()
{
   if (alive != 1) return 0;

   if (aff_poison) {
      aff_poison--;
      takeDamage( c_poison_damage, DMG_POISON );
   }
   if (aff_burning) {
      aff_burning--;
      takeDamage( 10, DMG_FIRE );
   }
   if (aff_confusion) {
      aff_confusion--;
      if (aff_confusion %= 2)
         return 0;
   }

   if (ai_overridden == true)
      return 0; // Received orders from elsewhere

   this_turn_order = Order( WAIT );

   if (aff_sleep > 0 || aff_timelock > 0) {
      if (aff_sleep > 0) aff_sleep--;
      if (aff_timelock > 0) aff_timelock--;
      return 0;
   }
   
   ai();

   return 0;
}

int AIUnit::update( float dtf )
{
   if (alive < 0) {
      alive += (int) (1000.0 * dtf);
      if (alive >= 0) alive = 0;
   }

   if (alive == 0)
      return 1;

   progress += dtf;
   Order &o = this_turn_order;
   return updateBasicOrder( dtf, o );
}

int AIUnit::completeTurn()
{
   if (alive != 1) return 0;

   if (aff_sleep == -1)
      return aff_sleep = 0; // Just woke up

   if (aff_sleep > 0 || aff_timelock > 0)
      return 0;

   ai_overridden = false; // reset override

   completeBasicOrder(this_turn_order);

   if (this_turn_order.action == FOLLOW_PATH) {
      // Set this location as the current waypoint
      ai_waypoints.front() = pair<Vector2i,Direction>( Vector2i( x_grid, y_grid ), facing );
   }

   return 0;
}

sf::Texture* AIUnit::getTexture()
{
   if (type == M_DWARF_SPEAR_T)
      return SFML_TextureManager::getSingleton().getTexture( "DwarfSpearStatic.png" );
}

Sprite *sp_human_archer = NULL;

int AIUnit::draw()
{
   // Select sprite
   Sprite *sp_unit = NULL;

   int rotation;
   if (facing == EAST) rotation = 0;
   if (facing == SOUTH) rotation = 90;
   if (facing == WEST) rotation = 180;
   if (facing == NORTH) rotation = 270;

   if (type == M_DWARF_SPEAR_T)
   {
      if (alive < 0) {
         // Death animation
         int t = alive + c_death_time + c_death_fade_time;
         if (t >= c_death_time) t = c_death_time - 1;
         sp_unit = dwarf_spear_anim_death.getSprite( t );

         int alpha = 255;
         if (alive > -c_death_fade_time)
            alpha = 255 - ((c_death_fade_time + alive) * 256 / c_death_fade_time);
         sp_unit->setColor( Color( 255, 255, 255, alpha ) );
      } else if (aff_timelock > 0) {
         sp_unit = dwarf_spear_anim_idle.getSprite( 0 );
         sp_unit->setColor( Color( 255, 126, 0 ) );
      } else if (this_turn_order.action == MOVE_FORWARD || this_turn_order.action == FOLLOW_PATH) {
         sp_unit = dwarf_spear_anim_move.getSprite( (int)(progress * 2000) );
      } else if (this_turn_order.action == MOVE_BACK) {
         sp_unit = dwarf_spear_anim_move.getSprite( 999 - (int)(progress * 2000) );
      } else if (this_turn_order.action >= ATTACK_CLOSEST && this_turn_order.action <= ATTACK_LEAST_ARMORED) {
         if (done_attack) {
            int d_anim = (int)( ((progress - speed) / (1-speed)) * 1000);
            if (d_anim >= 1000) d_anim = 999;
            sp_unit = dwarf_spear_anim_attack_end.getSprite( d_anim );
         } else {
            int d_anim = (int)( (progress / speed) * 1000);
            if (d_anim >= 1000) d_anim = 999;
            sp_unit = dwarf_spear_anim_attack_start.getSprite( d_anim );
         }
      } else
         sp_unit = dwarf_spear_anim_idle.getSprite( (int)(progress * 1000) );

      // TODO: Generalize this part later
      if (NULL == sp_unit) return -1;

      // Move/scale sprite
      sp_unit->setPosition( x_real, y_real );
      Vector2u dim (dwarf_spear_anim_idle.image_size_x, dwarf_spear_anim_idle.image_size_y);
      sp_unit->setOrigin( dim.x / 2.0, dim.y / 2.0 );
      sp_unit->setScale( 1.0 / dim.x, 1.0 / dim.y );

      sp_unit->setRotation( rotation );

      SFML_GlobalRenderWindow::get()->draw( *sp_unit );
   }
   else if (type == R_HUMAN_ARCHER_T)
   {
      if (NULL == sp_human_archer) {
         sp_human_archer = new Sprite(*getTexture());
         Vector2u dim = getTexture()->getSize();
         sp_human_archer->setOrigin( dim.x / 2.0, dim.y / 2.0 );
         normalizeTo1x1( sp_human_archer );
         //sp_human_archer->scale( 0.5, 0.5 );
      }

      sp_human_archer->setPosition( x_real, y_real );

      int rotation;
      if (facing == EAST) rotation = 0;
      if (facing == SOUTH) rotation = 90;
      if (facing == WEST) rotation = 180;
      if (facing == NORTH) rotation = 270;
      sp_human_archer->setRotation( rotation );

      SFML_GlobalRenderWindow::get()->draw( *sp_human_archer );
   }

   return 0;
}

AIUnit::~AIUnit()
{ }

//////////////////////////////////////////////////////////////////////
// Player ---

// Static

Player *thePlayer = NULL;

Animation player_anim_idle;

void initPlayerAnimations()
{
   Texture *t = SFML_TextureManager::getSingleton().getTexture( "BugScratchWiggleAnimation.png" );
   player_anim_idle.load( t, 128, 128, 3, 1000 );
}

// Private
Player::Player()
{ }

Player::~Player()
{
   type = PLAYER_T;
}

Player* Player::initPlayer( int grid_x, int grid_y, Direction facing )
{
   if (NULL == thePlayer)
      thePlayer = new Player();
   
   thePlayer->init( grid_x, grid_y, facing );
   return thePlayer;
}

int Player::init( int x, int y, Direction face )
{
   alive = 1;

   bird_shout_level = 0;

   aff_poison = 0;
   aff_burning = 0;
   aff_confusion = 0;
   aff_sleep = 0;
   aff_timelock = 0;
    
   flying = false;
   invis = false;

   radius = 0.4;

   x_grid = x;
   y_grid = y;
   x_real = x + 0.5;
   y_real = y + 0.5;
   turnTo(face);

   health = max_health = c_player_max_health;

   vision_range = 0.5;
   attack_range = 3.5;

   speed = 0.99;
   armor = 0;

   max_orders = c_player_max_orders;
   order_queue = new Order[max_orders];
   clearOrders();

   this_turn_order = Order( WAIT );

   initPlayerAnimations();

   active = 0;
   team = 0; 
   group = 1;
   progress = 0;
   
   win_condition = false;

   for (int i = 0; i < 12; ++i)
      dmg_display[i] = NULL;

   return 0;
}

int Player::addOrder( Order o )
{
   // Player uses a looping queue instead of a list
   if (order_count >= c_player_max_orders) // No more memory!!
      return -2;

   order_queue[final_order] = o;
   order_count++;
   final_order++;
   if (final_order == c_player_max_orders)
      final_order = 0;

   return 0;
}

int Player::doAttack( Order o )
{
   // Has no attack
   return 0;
}

int Player::prepareTurn()
{
   if (current_order == final_order) { // Nothing to do
      active = 0;
      return -1;
   }
   else active = 1;

   // Here is where we shout a new command
   Order o = order_queue[current_order];
   if (o.action == PL_DELAY)
      return 0;

   if (o.action < PL_ALERT_ALL || bird_shout_level > 0)
      broadcastOrder( o );
   else {
      if (startPlayerCommand( o ) == -2)
         order_queue[current_order].action = FAILED_SUMMON;
   }

   if (o.action == BIRD_CMD_SHOUT)
      bird_shout_level++;
   if (o.action == BIRD_CMD_QUIET) {
      bird_shout_level--;
      if (bird_shout_level < 0) bird_shout_level = 0;
   }

   progress = 0;
   return 0;
}

int Player::startTurn()
{

   return 0;
}

int Player::completeTurn()
{
   if (current_order == final_order) // Nothing to do
      return -1;

   if (active) { // started the turn doing something
      Order &o = order_queue[current_order];
      if (o.action == PL_DELAY) {
         o.count--;
         if (o.count > 0)
            return 0;
      }

      if (o.action >= PL_ALERT_ALL && bird_shout_level == 0)
         completePlayerCommand(o);

      do {
         current_order++;
         order_count--;
         if (current_order == c_player_max_orders)
            current_order = 0; // loop
      } while (order_queue[current_order].action == SKIP);
   }

   return 0;
}

int Player::update( float dtf )
{
   if (alive == 0)
      return 1;

   progress += dtf;
   return 0;
}

sf::Texture* Player::getTexture()
{
   return SFML_TextureManager::getSingleton().getTexture( "BugScratch.png" );
}

int Player::draw()
{ 
   Sprite *sp_player = player_anim_idle.getSprite( (int)(progress * 1000) );
   if (NULL == sp_player) return -1;

   Vector2u dim (player_anim_idle.image_size_x, player_anim_idle.image_size_y);
   sp_player->setOrigin( dim.x / 2.0, dim.y / 2.0 );
   sp_player->setScale( 1.0 / dim.x, 1.0 / dim.y );

   sp_player->setPosition( x_real, y_real );

   int rotation;
   if (facing == EAST) rotation = 0;
   if (facing == SOUTH) rotation = 90;
   if (facing == WEST) rotation = 180;
   if (facing == NORTH) rotation = 270;
   sp_player->setRotation( rotation );

   SFML_GlobalRenderWindow::get()->draw( *sp_player );

   return 0;
}

string Player::descriptor()
{
   return "Sheherezad";
}

//////////////////////////////////////////////////////////////////////
// Monster ---

Animation monster_anim_idle;
Animation monster_anim_move;
Animation monster_anim_attack_start;
Animation monster_anim_attack_end;
Animation monster_anim_death;

Animation monster_anim_burst_start;
Animation monster_anim_burst_end;

Animation monster_anim_guard_start;

Sprite *sp_monster_guard_bubble;

void initMonsterAnimations()
{
   Texture *t = SFML_TextureManager::getSingleton().getTexture( "MonsterAnimIdle.png" );
   monster_anim_idle.load( t, 128, 128, 8, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "MonsterAnimMove.png" );
   monster_anim_move.load( t, 128, 128, 14, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "MonsterAnimAttackStart.png" );
   monster_anim_attack_start.load( t, 128, 128, 8, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "MonsterAnimAttackEnd.png" );
   monster_anim_attack_end.load( t, 128, 128, 7, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "MonsterAnimDeath.png" );
   monster_anim_death.load( t, 128, 128, 7, c_death_time );

   t = SFML_TextureManager::getSingleton().getTexture( "MonsterAnimBurstStart.png" );
   monster_anim_burst_start.load( t, 128, 128, 11, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "MonsterAnimBurstEnd.png" );
   monster_anim_burst_end.load( t, 128, 128, 8, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "MonsterAnimGuardStart.png" );
   monster_anim_guard_start.load( t, 128, 128, 12, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "MonsterGuardBubble.png" );
   sp_monster_guard_bubble = new Sprite( *t );
   Vector2u dim = t->getSize();
   sp_monster_guard_bubble->setOrigin( dim.x / 2.0, dim.y / 2.0 );
   sp_monster_guard_bubble->setScale( 1.0 / dim.x, 1.0 / dim.y );
}

// *tors
Monster::Monster()
{
   Monster( -1, -1, SOUTH );
}

Monster::Monster( int x, int y, Direction face )
{
   log("Creating Monster");

   type = MONSTER_T;

   alive = 1;

   aff_poison = 0;
   aff_burning = 0;
   aff_confusion = 0;
   aff_sleep = 0;
   aff_timelock = 0;
    
   flying = false;
   invis = false;

   radius = 0.5;

   x_grid = x;
   y_grid = y;
   x_real = x + 0.5;
   y_real = y + 0.5;
   turnTo(face);

   health = max_health = c_monster_base_health * ( 1.0 + ( focus_toughness * 0.02 ) );

   vision_range = c_monster_base_vision * (1.0 + ((float)focus_perception / 25.0));
   attack_range = 1.3;

   speed = c_monster_base_speed * ( 1.0 - ( focus_speed * 0.02 ) );
   armor = c_monster_base_armor + (focus_toughness * 0.2);

   max_orders = c_monster_base_memory * ( 1.0 + ( focus_memory * 0.08 ) );
   order_queue = new Order[max_orders];
   clearOrders();

   this_turn_order = Order( WAIT );

   active = 0;
   team = 0;
   group = 1;
   progress = 0;

   win_condition = false;

   for (int i = 0; i < 12; ++i)
      dmg_display[i] = NULL;

   hardness = 0.0;
}

Monster::~Monster()
{
   if (order_queue)
      delete order_queue;
}

void Monster::doBurst()
{
   MonsterBurst *ef_mb = new MonsterBurst( x_real, y_real );
   addEffectManual( ef_mb );

   for (int x = x_grid - 1; x <= x_grid + 1; ++x) {
      if (x < 0 || x >= level_dim_x) continue;
      for (int y = y_grid - 1; y <= y_grid + 1; ++y) {
         if (y < 0 || y >= level_dim_y) continue;
         Unit *u = GRID_AT(unit_grid,x,y);
         if (u && u != this)
            u->takeDamage( c_monster_burst_damage, DMG_LIGHT );
      }
   }

   done_attack = 1;
}

float Monster::setHardness( float hard )
{
   if (hard < 0.0) hard = 0.0;
   if (hard > 1.0) hard = 1.0;

   return hardness = hard;
}

// Virtual methods

int Monster::takeDamage( float damage, DamageType type )
{
   if (hardness > 0.01) {
      float multiplier = 1.0 - (hardness * c_monster_max_hardness_reduction); 
      damage *= multiplier;
   }

   if (type == DMG_LIGHT) damage -= (2 * armor);
   else if (type == DMG_NORMAL) damage -= armor;

   if (damage <= 0) return 0;

   if (type == DMG_HEAL)
      health += damage;
   else
      health -= damage;

   if (health <= 0) {
      // Dead!
      alive = -( c_death_time + c_death_fade_time );
      return -1;
   }

   if (health >= max_health) {
      damage -= (health - max_health);
      health = max_health;
   }

   if (aff_sleep > 0 &&
         !(type == DMG_POISON || type == DMG_FIRE))
      aff_sleep = -1;

   displayDamage( damage, type );

   return 0;
}

int Monster::addOrder( Order o )
{
   if ((o.action <= WAIT) || (o.action >= MONSTER_GUARD && o.action <= MONSTER_BURST)) {
      if (order_count >= max_orders) // No more memory
         return -2;

      order_queue[order_count] = o;
      order_count++;
      final_order = order_count;

      return 0;
   } else return -1;
}

int Monster::doAttack( Order o )
{
   log("Monster doAttack");
   Unit *target = NULL;

   int t_x = x_grid, t_y = y_grid;
   if (addDirection( facing, t_x, t_y ) != -1)
      target = GRID_AT(unit_grid,t_x,t_y);

   if (target) {
      log("Monster attack");
      target->takeDamage( c_monster_claw_damage );
      // TODO: Animate
   }

   done_attack = 1;
   return 0;
}

int Monster::prepareTurn()
{
   if (alive != 1) return 0;

   if (active == 2) active = 1; // Start an active turn

   this_turn_order = Order( WAIT );

   if (aff_poison) {
      aff_poison--;
      takeDamage( c_poison_damage, DMG_POISON );
   }
   if (aff_burning) {
      aff_burning--;
      takeDamage( 10, DMG_FIRE );
   }
   if (aff_confusion) {
      aff_confusion--;
      if (aff_confusion %= 2)
         return 0;
   }

   if (aff_sleep > 0 || aff_timelock > 0) {
      if (aff_sleep > 0) aff_sleep--;
      if (aff_timelock > 0) aff_timelock--;
      return 0;
   }

   set<int> visited_orders;
   while (active == 1 && current_order != final_order 
         && visited_orders.find( current_order ) == visited_orders.end()) {
      this_turn_order = order_queue[current_order];
      visited_orders.insert( current_order );
      bool decision = evaluateConditional(this_turn_order.condition);
      // MONSTER UNIQUE CODE
      int r = 1;
      if (this_turn_order.action <= WAIT)
         r = prepareBasicOrder(order_queue[current_order], decision);
      else if (this_turn_order.action == MONSTER_BURST) {
         if (decision)
            done_attack = 0;
         else
            r = 0;
      } else if (this_turn_order.action == MONSTER_GUARD) { // take a turn to end it
         if (!decision && this_turn_order.iteration > 0)
            this_turn_order.iteration = this_turn_order.count - 1;
      }
      // END
      // if prepareBasicOrder returns 0, it's a 0-length instruction (e.g. turn)
      if (r == 0) {
         current_order++;
         continue;
      }
      else 
         break;
   }

   if (current_order == final_order) {
      this_turn_order = Order( WAIT );
      if (hardness > 0.5) {
         this_turn_order = Order( MONSTER_GUARD, 2 );
         this_turn_order.iteration = 1;
      }
   }

   progress = 0;
   return 0;
}

/*
int Monster::startTurn()
{
   if (current_order < order_count) {
      Order o = order_queue[current_order];
      startBasicOrder(o);
   }

   return 0;
}

int Monster::completeTurn()
{

   return 0;
}
*/

int Monster::update( float dtf )
{
   if (alive < 0) {
      alive += (int) (1000.0 * dtf);
      if (alive >= 0) alive = 0;
   }

   if (alive == 0)
      return 1;

   progress += dtf;
   if ((active == 1 && current_order != final_order) || this_turn_order.action == MONSTER_GUARD) {
      Order &o = this_turn_order;
      // Monster-specific
      if (this_turn_order.action == MONSTER_BURST) {
         if (!done_attack) {
            if (progress >= speed) {
               doBurst();
            }
         }
         return 0;
      }
      if (this_turn_order.action == MONSTER_GUARD) {
         if (this_turn_order.iteration == 0) {
            if (progress > speed && progress < speed + c_monster_harden_time) {
               float new_hardness = (progress - speed) / c_monster_harden_time;
               if (new_hardness > 1.0) new_hardness = 1.0;
               setHardness( new_hardness );
            }
         } else if (this_turn_order.iteration == this_turn_order.count - 1) {
            if (progress < (1.0 - speed) && progress > (1.0 - speed - c_monster_harden_time) ) {
               float new_hardness = 1.0 - ( (progress - (1.0 - speed - c_monster_harden_time)) / c_monster_harden_time);
               if (new_hardness < 0.0) new_hardness = 0.0;
               setHardness( new_hardness );
            }
         }
         return 0;
      }
      // End monster-specific
      return updateBasicOrder( dtf, o );
   }
   return 0;
}

sf::Texture* Monster::getTexture()
{
   return SFML_TextureManager::getSingleton().getTexture( "MonsterStatic.png" );
}

int Monster::draw()
{
   // Select sprite
   Sprite *sp_monster = NULL;

   int rotation;
   if (facing == EAST) rotation = 0;
   if (facing == SOUTH) rotation = 90;
   if (facing == WEST) rotation = 180;
   if (facing == NORTH) rotation = 270;

   if (alive < 0) {
      // Death animation
      int t = alive + c_death_time + c_death_fade_time;
      if (t >= c_death_time) t = c_death_time - 1;
      sp_monster = monster_anim_death.getSprite( t );

      int alpha = 255;
      if (alive > -c_death_fade_time)
         alpha = 255 - ((c_death_fade_time + alive) * 256 / c_death_fade_time);
      sp_monster->setColor( Color( 255, 255, 255, alpha ) );
   } else if (aff_timelock > 0) {
      sp_monster = monster_anim_idle.getSprite( 0 );
      sp_monster->setColor( Color( 255, 126, 0 ) );
   } else if (this_turn_order.action == MOVE_FORWARD || this_turn_order.action == FOLLOW_PATH) {
      sp_monster = monster_anim_move.getSprite( (int)(progress * 1000) );
   } else if (this_turn_order.action == MOVE_BACK) {
      sp_monster = monster_anim_move.getSprite( 999 - (int)(progress * 1000) );
   } else if (this_turn_order.action >= ATTACK_CLOSEST && this_turn_order.action <= ATTACK_LEAST_ARMORED) {
      if (done_attack) {
         int d_anim = (int)( ((progress - speed) / (1-speed)) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_monster = monster_anim_attack_end.getSprite( d_anim );
      } else {
         int d_anim = (int)( (progress / speed) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_monster = monster_anim_attack_start.getSprite( d_anim );
      }
   } else if (this_turn_order.action == MONSTER_BURST) {
      if (done_attack) {
         int d_anim = (int)( ((progress - speed) / (1-speed)) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_monster = monster_anim_burst_end.getSprite( d_anim );
      } else {
         int d_anim = (int)( (progress / speed) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_monster = monster_anim_burst_start.getSprite( d_anim );
      }
   } else if (this_turn_order.action == MONSTER_GUARD) {
      if (this_turn_order.iteration == 0) {
         if (progress > speed) {
            int alpha = (int) (255 * hardness);
            sp_monster_guard_bubble->setColor( Color( 255, 255, 255, alpha ) ); 

            sp_monster_guard_bubble->setPosition( x_real, y_real );
            sp_monster_guard_bubble->setRotation( rotation );
            SFML_GlobalRenderWindow::get()->draw( *sp_monster_guard_bubble );

            sp_monster = monster_anim_guard_start.getSprite( 999 );
         } else {
            int d_anim = (int)( (progress / speed) * 1000);
            if (d_anim >= 1000) d_anim = 999;
            sp_monster = monster_anim_guard_start.getSprite( d_anim );
         }
      } else if (this_turn_order.iteration == this_turn_order.count - 1) {
         if (progress < (1.0 - speed)) {
            int alpha = (int) (255 * hardness);
            sp_monster_guard_bubble->setColor( Color( 255, 255, 255, alpha ) ); 

            sp_monster_guard_bubble->setPosition( x_real, y_real );
            sp_monster_guard_bubble->setRotation( rotation );
            SFML_GlobalRenderWindow::get()->draw( *sp_monster_guard_bubble );

            sp_monster = monster_anim_guard_start.getSprite( 999 );
         } else {
            int d_anim = (int)( ((progress - (1.0 - speed)) / speed) * 1000);
            if (d_anim >= 1000) d_anim = 999;
            sp_monster = monster_anim_guard_start.getSprite( 999 - d_anim );
         }
      } else {
         sp_monster_guard_bubble->setColor( Color::White );
         sp_monster_guard_bubble->setPosition( x_real, y_real );
         sp_monster_guard_bubble->setRotation( rotation );
         SFML_GlobalRenderWindow::get()->draw( *sp_monster_guard_bubble );

         sp_monster = monster_anim_guard_start.getSprite( 999 );
      }
   } else
      sp_monster = monster_anim_idle.getSprite( (int)(progress * 1000) );
   if (NULL == sp_monster) return -1;

   // Move/scale sprite
   sp_monster->setPosition( x_real, y_real );
   Vector2u dim (monster_anim_idle.image_size_x, monster_anim_idle.image_size_y);
   sp_monster->setOrigin( dim.x / 2.0, dim.y / 2.0 );
   sp_monster->setScale( 1.0 / dim.x, 1.0 / dim.y );

   sp_monster->setRotation( rotation );

   SFML_GlobalRenderWindow::get()->draw( *sp_monster );

   return 0;
}

string Monster::descriptor()
{
   return "Allied Monster";
}

//////////////////////////////////////////////////////////////////////
// Soldier ---

Animation soldier_anim_idle;
Animation soldier_anim_idle_axe;
Animation soldier_anim_move_axe;
Animation soldier_anim_attack_start_axe;
Animation soldier_anim_attack_end_axe;
Animation soldier_anim_idle_spear;
Animation soldier_anim_move_spear;
Animation soldier_anim_attack_start_spear;
Animation soldier_anim_attack_end_spear;
Animation soldier_anim_idle_bow;
Animation soldier_anim_move_bow;
Animation soldier_anim_attack_start_bow;
Animation soldier_anim_attack_end_bow;
Animation soldier_anim_death;

void initSoldierAnimations()
{
   Texture *t = SFML_TextureManager::getSingleton().getTexture( "SoldierStatic.png" );
   soldier_anim_idle.load( t, 128, 128, 1, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimIdleAxe.png" );
   soldier_anim_idle_axe.load( t, 128, 128, 10, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimMoveAxe.png" );
   soldier_anim_move_axe.load( t, 128, 128, 16, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimAttackStartAxe.png" );
   soldier_anim_attack_start_axe.load( t, 128, 128, 10, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimAttackEndAxe.png" );
   soldier_anim_attack_end_axe.load( t, 128, 128, 9, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimIdleSpear.png" );
   soldier_anim_idle_spear.load( t, 128, 128, 10, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimMoveSpear.png" );
   soldier_anim_move_spear.load( t, 128, 128, 16, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimAttackStartSpear.png" );
   soldier_anim_attack_start_spear.load( t, 128, 128, 10, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimAttackEndSpear.png" );
   soldier_anim_attack_end_spear.load( t, 128, 128, 9, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimIdleBow.png" );
   soldier_anim_idle_bow.load( t, 128, 128, 10, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimMoveBow.png" );
   soldier_anim_move_bow.load( t, 128, 128, 16, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimAttackStartBow.png" );
   soldier_anim_attack_start_bow.load( t, 128, 128, 11, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimAttackEndBow.png" );
   soldier_anim_attack_end_bow.load( t, 128, 128, 11, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "SoldierAnimDeath.png" );
   soldier_anim_death.load( t, 128, 128, 9, c_death_time );
}

// *tors
Soldier::Soldier()
{
   Soldier( -1, -1, SOUTH );
}

Soldier::Soldier( int x, int y, Direction face )
{
   log("Creating Soldier");

   type = SOLDIER_T;

   alive = 1;

   aff_poison = 0;
   aff_burning = 0;
   aff_confusion = 0;
   aff_sleep = 0;
   aff_timelock = 0;
    
   flying = false;
   invis = false;

   radius = 0.2;

   x_grid = x;
   y_grid = y;
   x_real = x + 0.5;
   y_real = y + 0.5;
   turnTo(face);

   health = max_health = c_soldier_base_health * ( 1.0 + ( focus_toughness * 0.02 ) );

   stance = 0;

   vision_range = c_soldier_base_vision * (1.0 + ((float)focus_perception / 25.0));
   attack_range = 1.3;

   speed = c_soldier_base_speed * ( 1.0 - ( focus_speed * 0.02 ) );
   armor = c_soldier_base_armor + (focus_toughness * 0.1);

   max_orders = c_soldier_base_memory * ( 1.0 + ( focus_memory * 0.08 ) );
   order_queue = new Order[max_orders];
   clearOrders();

   this_turn_order = Order( WAIT );

   active = 0;
   team = 0;
   group = 1;
   progress = 0;

   win_condition = false;

   for (int i = 0; i < 12; ++i)
      dmg_display[i] = NULL;
}

Soldier::~Soldier()
{
   if (order_queue)
      delete order_queue;
}

// Virtual methods

int Soldier::addOrder( Order o )
{
   if ((o.action <= WAIT) || (o.action >= SOLDIER_SWITCH_AXE && o.action <= SOLDIER_SWITCH_BOW)) {
      if (order_count >= max_orders) // No more memory
         return -2;

      order_queue[order_count] = o;
      order_count++;
      final_order = order_count;

      return 0;
   } else return -1;
}

int Soldier::doAttack( Order o )
{
   Unit *target = NULL;
   int selector = SELECT_CLOSEST;
   switch(o.action) {
      case ATTACK_CLOSEST:
         selector = SELECT_CLOSEST;
         break;
      case ATTACK_FARTHEST:
         selector = SELECT_FARTHEST;
         break;
      case ATTACK_SMALLEST:
         selector = SELECT_SMALLEST;
         break;
      case ATTACK_BIGGEST:
         selector = SELECT_BIGGEST;
         break;
      case ATTACK_MOST_ARMORED:
         selector = SELECT_MOST_ARMORED;
         break;
      case ATTACK_LEAST_ARMORED:
         selector = SELECT_LEAST_ARMORED;
         break;
      default:
         log("ERROR: doAttack called on non-attack order");
         return -1;
   }

   if (stance == 0) {
      int t_x = x_grid, t_y = y_grid;
      if (addDirection( facing, t_x, t_y ) != -1)
         target = GRID_AT(unit_grid,t_x,t_y);

      if (target) {
         log("Soldier axe attack");
         target->takeDamage( c_soldier_axe_damage, DMG_HEAVY );
         // TODO: Animate
      }
   }
   else if (stance == 1) {
      int min_x = x_grid - 2, max_x = x_grid + 2, min_y = y_grid - 2, max_y = y_grid + 2;
      if (facing == NORTH) max_y = y_grid - 1;
      else if (facing == SOUTH) min_y = y_grid + 1;
      else if (facing == WEST) max_x = x_grid - 1;
      else if (facing == EAST) min_x = x_grid + 1;

      target = getEnemyBox( x_grid, y_grid, min_x, max_x, min_y, max_y, attack_range, this, selector );

      if (target) {
         log("Soldier spear attack");
         // Get spear trajectory
         int t_x = target->x_grid, t_y = target->y_grid;
         if (t_x == x_grid + 1) {
            if (t_y == y_grid + 1) {
               t_x = x_grid + 2;
               t_y = y_grid + 2;
            } else if (t_y == y_grid - 1) {
               t_x = x_grid + 2;
               t_y = y_grid - 2;
            } else if (t_y == y_grid)
               t_x = x_grid + 2;
         } else if (t_x == x_grid - 1) {
            if (t_y == y_grid + 1) {
               t_x = x_grid - 2;
               t_y = y_grid + 2;
            } else if (t_y == y_grid - 1) {
               t_x = x_grid - 2;
               t_y = y_grid - 2;
            } else if (t_y == y_grid)
               t_x = x_grid - 2;
         } else if (t_x == x_grid) {
            if (t_y == y_grid - 1) t_y = y_grid - 2;
            else if (t_y == y_grid + 1) t_y = y_grid + 2;
         }
         // Get inner target
         int t_x2 = t_x, t_y2 = t_y;
         if (t_x2 == x_grid + 2) t_x2 = x_grid + 1;
         if (t_x2 == x_grid - 2) t_x2 = x_grid - 1;
         if (t_y2 == y_grid + 2) t_y2 = y_grid + 1;
         if (t_y2 == y_grid - 2) t_y2 = y_grid - 1;

         // Damage targets
         Unit *t1 = GRID_AT(unit_grid,t_x,t_y);
         Unit *t2 = GRID_AT(unit_grid,t_x2,t_y2);
         if (NULL != t1)
            t1->takeDamage( c_soldier_spear_damage );
         if (NULL != t2)
            t2->takeDamage( c_soldier_spear_damage );

         // Animate
         float rot = atan( (t_y - y_grid) / (t_x - x_grid + 0.0001) ) * 180 / 3.1415926;
         addEffect( SE_SPEAR_ANIM, 0.15, x_real, y_real, rot, 0.15 );
      }

   }
   else if (stance == 2) {
      target = getEnemy( x_grid, y_grid, attack_range, facing, this, selector );

      if (target) {
         log("Soldier bow attack");
         addProjectile( PR_ARROW, team, x_real, y_real, 3.0, attack_range, target, 0.0, 0.1 );
      }
   }

   done_attack = 1;
   return 0;
}

int Soldier::prepareTurn()
{
   if (alive != 1) return 0;

   if (active == 2) active = 1; // Start an active turn

   this_turn_order = Order( WAIT );

   if (aff_poison) {
      aff_poison--;
      takeDamage( c_poison_damage, DMG_POISON );
   }
   if (aff_burning) {
      aff_burning--;
      takeDamage( 10, DMG_FIRE );
   }
   if (aff_confusion) {
      aff_confusion--;
      if (aff_confusion %= 2)
         return 0;
   }

   if (aff_sleep > 0 || aff_timelock > 0) {
      if (aff_sleep > 0) aff_sleep--;
      if (aff_timelock > 0) aff_timelock--;
      return 0;
   }

   set<int> visited_orders;
   while (active == 1 && current_order != final_order 
         && visited_orders.find( current_order ) == visited_orders.end()) {
      this_turn_order = order_queue[current_order];
      visited_orders.insert( current_order );
      bool decision = evaluateConditional(this_turn_order.condition);
      // SOLDIER UNIQUE CODE
      int r = 1;
      if (this_turn_order.action <= WAIT)
         r = prepareBasicOrder(order_queue[current_order], decision);
      else if (decision == false) {
         r = 0;
      } else if (this_turn_order.action == SOLDIER_SWITCH_AXE) {
         r = 0;
         stance = 0;
         attack_range = 1.3;
      } else if (this_turn_order.action == SOLDIER_SWITCH_SPEAR) {
         r = 0;
         stance = 1;
         attack_range = 2.9;
      } else if (this_turn_order.action == SOLDIER_SWITCH_BOW) {
         r = 0;
         stance = 2;
         attack_range = vision_range;
      }
      // END
      // if prepareBasicOrder returns 0, it's a 0-length instruction (e.g. turn)
      if (r == 0) {
         current_order++;
         continue;
      }
      else 
         break;
   }

   if (current_order == final_order)
      this_turn_order = Order( WAIT );

   progress = 0;
   return 0;
}
/*
int Soldier::startTurn()
{
   if (current_order < order_count) {
      Order o = order_queue[current_order];
      startBasicOrder(o);
   }

   return 0;
}

int Soldier::completeTurn()
{

   return 0;
}

int Soldier::update( float dtf )
{
   if (alive < 0) {
      alive += (int) (1000.0 * dtf);
      if (alive >= 0) alive = 0;
   }

   if (alive == 0)
      return 1;

   progress += dtf;
   if (active == 1 && current_order != final_order) {
      Order &o = this_turn_order;
      return updateBasicOrder( dtf, o );
   }
   return 0;
}
*/

sf::Texture* Soldier::getTexture()
{
   return SFML_TextureManager::getSingleton().getTexture( "SoldierStatic.png" );
}

int Soldier::draw()
{
   // Select sprite
   Sprite *sp_soldier = NULL;

   int rotation;
   if (facing == EAST) rotation = 0;
   if (facing == SOUTH) rotation = 90;
   if (facing == WEST) rotation = 180;
   if (facing == NORTH) rotation = 270;

   if (alive < 0) {
      // Death animation
      int t = alive + c_death_time + c_death_fade_time;
      if (t >= c_death_time) t = c_death_time - 1;
      sp_soldier = soldier_anim_death.getSprite( t );

      int alpha = 255;
      if (alive > -c_death_fade_time)
         alpha = 255 - ((c_death_fade_time + alive) * 256 / c_death_fade_time);
      sp_soldier->setColor( Color( 255, 255, 255, alpha ) );
   } else if (aff_timelock > 0) {
      if (stance == 0) sp_soldier = soldier_anim_idle_axe.getSprite( 0 );
      if (stance == 1) sp_soldier = soldier_anim_idle_spear.getSprite( 0 );
      if (stance == 2) sp_soldier = soldier_anim_idle_bow.getSprite( 0 );
      sp_soldier->setColor( Color( 255, 126, 0 ) );
   } else if (this_turn_order.action == MOVE_FORWARD || this_turn_order.action == FOLLOW_PATH) {
      if (stance == 0) sp_soldier = soldier_anim_move_axe.getSprite( (int)(progress * 1000) );
      if (stance == 1) sp_soldier = soldier_anim_move_spear.getSprite( (int)(progress * 1000) );
      if (stance == 2) sp_soldier = soldier_anim_move_bow.getSprite( (int)(progress * 1000) );
   } else if (this_turn_order.action == MOVE_BACK) {
      if (stance == 0) sp_soldier = soldier_anim_move_axe.getSprite( 999 - (int)(progress * 1000) );
      if (stance == 1) sp_soldier = soldier_anim_move_spear.getSprite( 999 - (int)(progress * 1000) );
      if (stance == 2) sp_soldier = soldier_anim_move_bow.getSprite( 999 - (int)(progress * 1000) );
   } else if (this_turn_order.action >= ATTACK_CLOSEST && this_turn_order.action <= ATTACK_LEAST_ARMORED) {
      if (done_attack) {
         int d_anim = (int)( ((progress - speed) / (1-speed)) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         if (stance == 0) sp_soldier = soldier_anim_attack_end_axe.getSprite( d_anim );
         if (stance == 1) sp_soldier = soldier_anim_attack_end_spear.getSprite( d_anim );
         if (stance == 2) sp_soldier = soldier_anim_attack_end_bow.getSprite( d_anim );
      } else {
         int d_anim = (int)( (progress / speed) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         if (stance == 0) sp_soldier = soldier_anim_attack_start_axe.getSprite( d_anim );
         if (stance == 1) sp_soldier = soldier_anim_attack_start_spear.getSprite( d_anim );
         if (stance == 2) sp_soldier = soldier_anim_attack_start_bow.getSprite( d_anim );
      }
   } else {
      if (stance == 0) sp_soldier = soldier_anim_idle_axe.getSprite( (int)(progress * 1000) );
      if (stance == 1) sp_soldier = soldier_anim_idle_spear.getSprite( (int)(progress * 1000) );
      if (stance == 2) sp_soldier = soldier_anim_idle_bow.getSprite( (int)(progress * 1000) );
   }
   if (NULL == sp_soldier) return -1;

   // Move/scale sprite
   sp_soldier->setPosition( x_real, y_real );
   Vector2u dim (soldier_anim_idle.image_size_x, soldier_anim_idle.image_size_y);
   sp_soldier->setOrigin( dim.x / 2.0, dim.y / 2.0 );
   sp_soldier->setScale( 0.8 / dim.x, 0.8 / dim.y );

   sp_soldier->setRotation( rotation );

   SFML_GlobalRenderWindow::get()->draw( *sp_soldier );

   return 0;
}

string Soldier::descriptor()
{
   return "Allied Soldier";
}


//////////////////////////////////////////////////////////////////////
// Worm ---

Animation worm_anim_idle;
Animation worm_anim_idle_invis;
Animation worm_anim_move;
Animation worm_anim_move_trail;
Animation worm_anim_attack_start;
Animation worm_anim_attack_end;
Animation worm_anim_death;

void initWormAnimations()
{
   Texture *t = SFML_TextureManager::getSingleton().getTexture( "WormAnimIdle.png" );
   worm_anim_idle.load( t, 128, 128, 8, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "WormAnimIdleInvis.png" );
   worm_anim_idle_invis.load( t, 128, 128, 8, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "WormAnimMove.png" );
   worm_anim_move.load( t, 128, 128, 11, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "WormAnimMoveTrail.png" );
   worm_anim_move_trail.load( t, 128, 128, 12, 333 );

   t = SFML_TextureManager::getSingleton().getTexture( "WormAnimAttackStart.png" );
   worm_anim_attack_start.load( t, 128, 128, 8, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "WormAnimAttackEnd.png" );
   worm_anim_attack_end.load( t, 128, 128, 8, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "WormAnimDeath.png" );
   worm_anim_death.load( t, 128, 128, 8, c_death_time );
}

// *tors
Worm::Worm()
{
   Worm( -1, -1, SOUTH );
}

Worm::Worm( int x, int y, Direction face )
{
   log("Creating Worm");

   type = WORM_T;

   alive = 1;

   aff_poison = 0;
   aff_burning = 0;
   aff_confusion = 0;
   aff_sleep = 0;
   aff_timelock = 0;
    
   flying = false;
   invis = false;

   radius = 0.2;

   x_grid = x;
   y_grid = y;
   x_real = x + 0.5;
   y_real = y + 0.5;
   turnTo(face);

   health = max_health = c_worm_base_health * ( 1.0 + ( focus_toughness * 0.02 ) );

   vision_range = c_worm_base_vision * (1.0 + ((float)focus_perception / 25.0));
   attack_range = 1.3;

   speed = c_worm_base_speed * ( 1.0 - ( focus_speed * 0.02 ) );
   armor = c_worm_base_armor + (focus_toughness * 0.1);

   max_orders = c_worm_base_memory * ( 1.0 + ( focus_memory * 0.08 ) );
   order_queue = new Order[max_orders];
   clearOrders();

   this_turn_order = Order( WAIT );

   active = 0;
   team = 0;
   group = 1;
   progress = 0;
   
   win_condition = false;

   for (int i = 0; i < 12; ++i)
      dmg_display[i] = NULL;

   invis = false;
   trail = false;
}

Worm::~Worm()
{
   if (order_queue)
      delete order_queue;
}

// Virtual methods

int Worm::prepareTurn()
{
   if (alive != 1) return 0;

   if (active == 2) active = 1; // Start an active turn

   this_turn_order = Order( WAIT );

   if (aff_poison) {
      aff_poison--;
      takeDamage( c_poison_damage, DMG_POISON );
   }
   if (aff_burning) {
      aff_burning--;
      takeDamage( 10, DMG_FIRE );
   }
   if (aff_confusion) {
      aff_confusion--;
      if (aff_confusion %= 2)
         return 0;
   }

   if (aff_sleep > 0 || aff_timelock > 0) {
      if (aff_sleep > 0) aff_sleep--;
      if (aff_timelock > 0) aff_timelock--;
      return 0;
   }

   set<int> visited_orders;
   while (active == 1 && current_order != final_order 
         && visited_orders.find( current_order ) == visited_orders.end()) {
      this_turn_order = order_queue[current_order];
      visited_orders.insert( current_order );
      bool decision = evaluateConditional(this_turn_order.condition);
      // WORM UNIQUE CODE
      int r = 1;
      log("Worm unique code - prepareTurn");
      if (this_turn_order.action <= WAIT)
         r = prepareBasicOrder(order_queue[current_order], decision);
      else if (decision == false)
         r = 0;
      else if (this_turn_order.action == WORM_TRAIL_START) {
         r = 0;
         trail = true;
         log( "Trail on" );
      } else if (this_turn_order.action == WORM_TRAIL_END) {
         r = 0;
         trail = false;
      }
      // END
      // if prepareBasicOrder returns 0, it's a 0-length instruction (e.g. turn)
      if (r == 0) {
         current_order++;
         continue;
      }
      else 
         break;
   }

   if (current_order == final_order)
      this_turn_order = Order( WAIT );

   progress = 0;
   return 0;
}

int Worm::startTurn()
{
   if (alive != 1) return 0;

   if (this_turn_order.action != WORM_HIDE)
      invis = false;

   startBasicOrder();
   return 0;
}

TerrainMod combineIntoTrail( Direction d, TerrainMod original )
{
   if (original == TM_NONE) {
      if (d == NORTH) return TM_TRAIL_S_END;
      else if (d == SOUTH) return TM_TRAIL_N_END;
      else if (d == EAST) return TM_TRAIL_W_END;
      else if (d == WEST) return TM_TRAIL_E_END;
   }

   if ((d == NORTH && original == TM_TRAIL_S_END)
         || (d == SOUTH && original == TM_TRAIL_N_END)
         || (d == EAST && original == TM_TRAIL_W_END)
         || (d == WEST && original == TM_TRAIL_E_END)) 
      return original; //overwrite self

   if (original >= TM_TRAIL_W_END && original <= TM_TRAIL_N_END) {
      // S_END
      if (original == TM_TRAIL_S_END && d == SOUTH) return TM_TRAIL_NS;
      if (original == TM_TRAIL_S_END && d == EAST) return TM_TRAIL_NE;
      if (original == TM_TRAIL_S_END && d == WEST) return TM_TRAIL_NW;
      // N_END
      if (original == TM_TRAIL_N_END && d == NORTH) return TM_TRAIL_NS;
      if (original == TM_TRAIL_N_END && d == EAST) return TM_TRAIL_SE;
      if (original == TM_TRAIL_N_END && d == WEST) return TM_TRAIL_SW;
      // W_END
      if (original == TM_TRAIL_W_END && d == NORTH) return TM_TRAIL_NE;
      if (original == TM_TRAIL_W_END && d == SOUTH) return TM_TRAIL_SE;
      if (original == TM_TRAIL_W_END && d == WEST) return TM_TRAIL_EW;
      // E_END
      if (original == TM_TRAIL_E_END && d == NORTH) return TM_TRAIL_NW;
      if (original == TM_TRAIL_E_END && d == SOUTH) return TM_TRAIL_SW;
      if (original == TM_TRAIL_E_END && d == EAST) return TM_TRAIL_EW;
   }
   
   return TM_NONE;
}

int Worm::completeTurn()
{
   if (alive != 1) return 0;

   if (aff_sleep == -1)
      return aff_sleep = 0; // Just woke up

   if (aff_sleep > 0 || aff_timelock > 0)
      return 0;

   if (active == 1 && current_order != final_order) {
      // Worm stuff
      if (this_turn_order.action == WORM_HIDE)
         invis = true;
      testInvis();
      // Trail section -- 
      if (trail && (this_turn_order.action == MOVE_FORWARD || this_turn_order.action == FOLLOW_PATH)) {
         Direction d = facing;
         TerrainMod tm_from = GRID_AT(terrain_mod_grid,x_grid,y_grid);
         TerrainMod tm_to = GRID_AT(terrain_mod_grid,x_next,y_next);
         // From
         GRID_AT(terrain_mod_grid,x_grid,y_grid) = combineIntoTrail( d, tm_from );
         TerrainMod result = combineIntoTrail( reverseDirection( d ), tm_to );
         GRID_AT(terrain_mod_grid,x_next,y_next) = result;

         if (result >= TM_TRAIL_EW && result < NUM_TERRAIN_MODS)
            trail = false; // Turn off when loop is completed, so you don't overwrite + destroy it
      }
      // end --
      int r = completeBasicOrder(this_turn_order);
      Order &o = order_queue[current_order];
      o.iteration++;
      if (o.iteration >= o.count && o.count != -1) { 
         current_order++;
         o.iteration = 0;
      }
      return r;
   }

   return 0;

}

int Worm::takeDamage( float damage, DamageType type )
{
   if (type == DMG_LIGHT) damage -= (2 * armor);
   else if (type == DMG_NORMAL) damage -= armor;

   if (damage <= 0) return 0;

   if (type == DMG_HEAL)
      health += damage;
   else
      health -= damage;

   invis = false;
   if (this_turn_order.action == WORM_HIDE)
      this_turn_order.action = WAIT;

   if (health <= 0) {
      // Dead!
      alive = -( c_death_time + c_death_fade_time );
      return -1;
   }

   if (health >= max_health) {
      damage -= (health - max_health);
      health = max_health;
   }

   if (aff_sleep > 0 &&
         !(type == DMG_POISON || type == DMG_FIRE))
      aff_sleep = -1;

   displayDamage( damage, type );

   return 0;
}

int Worm::addOrder( Order o )
{
   if ((o.action <= WAIT) || (o.action >= WORM_HIDE && o.action <= WORM_TRAIL_END)) {
      if (order_count >= max_orders) // No more memory
         return -2;

      order_queue[order_count] = o;
      order_count++;
      final_order = order_count;

      return 0;
   } else return -1;
}

int Worm::doAttack( Order o )
{
   log("Worm doAttack");
   Unit *target = NULL;

   int t_x = x_grid, t_y = y_grid;
   if (addDirection( facing, t_x, t_y ) != -1)
      target = GRID_AT(unit_grid,t_x,t_y);

   if (target) {
      log("Worm attack");
      target->takeDamage( 10 );
      target->aff_poison += c_worm_poison_duration;
      // TODO: Animate
   }

   done_attack = 1;
   return 0;
}

sf::Texture* Worm::getTexture()
{
   return SFML_TextureManager::getSingleton().getTexture( "WormStatic.png" );
}

int Worm::draw()
{
   // Select sprite
   Sprite *sp_worm = NULL,
          *sp_worm_invis = NULL;
   if (alive < 0) {
      // Death animation
      int t = alive + c_death_time + c_death_fade_time;
      if (t >= c_death_time) t = c_death_time - 1;
      sp_worm = worm_anim_death.getSprite( t );

      int alpha = 255;
      if (alive > -c_death_fade_time)
         alpha = 255 - ((c_death_fade_time + alive) * 256 / c_death_fade_time);
      sp_worm->setColor( Color( 255, 255, 255, alpha ) );
   } else if (aff_timelock > 0) {
      sp_worm = worm_anim_idle.getSprite( 0 );
      sp_worm->setColor( Color( 255, 126, 0 ) );
   } else if (this_turn_order.action == MOVE_FORWARD || this_turn_order.action == FOLLOW_PATH) {
      if (trail) {
         sp_worm = worm_anim_move_trail.getSprite( (int)(progress * 1000) );
      } else
         sp_worm = worm_anim_move.getSprite( (int)(progress * 1000) );
   } else if (this_turn_order.action == MOVE_BACK) {
      // TODO: Worm should have different retreat animation
      sp_worm = worm_anim_move_trail.getSprite( 999 - (int)(progress * 1000) );
   } else if (this_turn_order.action >= ATTACK_CLOSEST && this_turn_order.action <= ATTACK_LEAST_ARMORED) {
      if (done_attack) {
         int d_anim = (int)( ((progress - speed) / (1-speed)) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_worm = worm_anim_attack_end.getSprite( d_anim );
      } else {
         int d_anim = (int)( (progress / speed) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_worm = worm_anim_attack_start.getSprite( d_anim );
      }
   } else if (this_turn_order.action == WORM_HIDE) {
      if (invis) {
         sp_worm = worm_anim_idle.getSprite( (int)(progress * 1000) );
         sp_worm->setColor( Color( 255, 255, 255, 127 ) );
         sp_worm_invis = worm_anim_idle_invis.getSprite( (int)(progress * 1000) );
         sp_worm_invis->setColor( Color::White );
      } else {
         int alpha = 255 - (progress * 192);
         sp_worm = worm_anim_idle.getSprite( (int)(progress * 1000) );
         sp_worm->setColor( Color( 255, 255, 255, alpha ) );
         sp_worm_invis = worm_anim_idle_invis.getSprite( (int)(progress * 1000) );
         sp_worm_invis->setColor( Color( 255, 255, 255, 255 - alpha ) );
      }
   } else {
      sp_worm = worm_anim_idle.getSprite( (int)(progress * 1000) );
      sp_worm->setColor( Color::White );
   }
   if (NULL == sp_worm) return -1;

   // Move/scale sprite
   Vector2u dim (worm_anim_idle.image_size_x, worm_anim_idle.image_size_y);
   sp_worm->setOrigin( dim.x / 2.0, dim.y / 2.0 );
   sp_worm->setScale( 0.3 / dim.x, 0.3 / dim.y );

   sp_worm->setPosition( x_real, y_real );

   int rotation;
   if (facing == EAST) rotation = 0;
   if (facing == SOUTH) rotation = 90;
   if (facing == WEST) rotation = 180;
   if (facing == NORTH) rotation = 270;
   sp_worm->setRotation( rotation );

   SFML_GlobalRenderWindow::get()->draw( *sp_worm );

   if (sp_worm_invis != NULL) {
      sp_worm_invis->setOrigin( dim.x / 2.0, dim.y / 2.0 );
      sp_worm_invis->setScale( 0.3 / dim.x, 0.3 / dim.y );

      sp_worm_invis->setPosition( x_real, y_real );

      int rotation;
      if (facing == EAST) rotation = 0;
      if (facing == SOUTH) rotation = 90;
      if (facing == WEST) rotation = 180;
      if (facing == NORTH) rotation = 270;
      sp_worm_invis->setRotation( rotation );

      SFML_GlobalRenderWindow::get()->draw( *sp_worm_invis );
   }

   return 0;
}

string Worm::descriptor()
{
   return "Allied Worm";
}

//////////////////////////////////////////////////////////////////////
// Bird ---

Animation bird_anim_idle1;
Animation bird_anim_idle2;
Animation bird_anim_idle3;
Animation bird_anim_move;
Animation bird_anim_attack_start;
Animation bird_anim_death;
Animation bird_anim_take_off;
Animation bird_anim_land;
Animation bird_anim_fly;
Animation bird_anim_shout;

void initBirdAnimations()
{
   Texture *t = SFML_TextureManager::getSingleton().getTexture( "BirdAnimIdle1.png" );
   bird_anim_idle1.load( t, 128, 128, 7, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "BirdAnimIdle2.png" );
   bird_anim_idle2.load( t, 128, 128, 16, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "BirdAnimIdle3.png" );
   bird_anim_idle3.load( t, 128, 128, 16, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "BirdAnimMove.png" );
   bird_anim_move.load( t, 128, 128, 10, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "BirdAnimAttack.png" );
   bird_anim_attack_start.load( t, 128, 128, 14, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "BirdAnimTakeOff.png" );
   bird_anim_take_off.load( t, 128, 128, 12, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "BirdAnimLand.png" );
   bird_anim_land.load( t, 128, 128, 10, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "BirdAnimFly.png" );
   bird_anim_fly.load( t, 128, 128, 10, 500 );

   t = SFML_TextureManager::getSingleton().getTexture( "BirdAnimDeath.png" );
   bird_anim_death.load( t, 128, 128, 8, c_death_time );

   t = SFML_TextureManager::getSingleton().getTexture( "BirdAnimShout1.png" );
   bird_anim_shout.load( t, 128, 128, 13, 1000 );
}

// *tors
Bird::Bird()
{
   Bird( -1, -1, SOUTH );
}

Bird::Bird( int x, int y, Direction face )
{
   log("Creating Bird");

   type = BIRD_T;

   alive = 1;

   aff_poison = 0;
   aff_burning = 0;
   aff_confusion = 0;
   aff_sleep = 0;
   aff_timelock = 0;

   flying = false;
   invis = false;

   radius = 0.2;

   x_grid = x;
   y_grid = y;
   x_real = x + 0.5;
   y_real = y + 0.5;
   turnTo(face);

   health = max_health = c_bird_base_health * ( 1.0 + ( focus_toughness * 0.02 ) );

   vision_range = c_bird_base_vision * (1.0 + ((float)focus_perception / 25.0));
   attack_range = vision_range;

   speed = c_bird_base_speed * ( 1.0 - ( focus_speed * 0.02 ) );
   armor = c_bird_base_armor + (focus_toughness * 0.1);

   max_orders = c_bird_base_memory * ( 1.0 + ( focus_memory * 0.08 ) );
   order_queue = new Order[max_orders];
   clearOrders();

   this_turn_order = Order( WAIT );

   active = 0;
   team = 0;
   group = 1;
   progress = 0;
   anim_data = 0;
   
   win_condition = false;

   for (int i = 0; i < 12; ++i)
      dmg_display[i] = NULL;

   shout_nesting = 0;
}

Bird::~Bird()
{
   if (order_queue)
      delete order_queue;
}

// Virtual methods

int Bird::addOrder( Order o )
{
   //if ((o.action <= WAIT) || (o.action >= BIRD_CMD_SHOUT && o.action <= BIRD_FLY)) {
      if (order_count >= max_orders) // No more memory
         return -2;

      order_queue[order_count] = o;
      order_count++;
      final_order = order_count;

      return 0;
   //} else return -1;
}

int Bird::doAttack( Order o )
{
   log("Bird doAttack");
   int selector = SELECT_CLOSEST;
   Unit *target = NULL;
   switch(o.action) {
      case ATTACK_CLOSEST:
         selector = SELECT_CLOSEST;
         break;
      case ATTACK_FARTHEST:
         selector = SELECT_FARTHEST;
         break;
      case ATTACK_SMALLEST:
         selector = SELECT_SMALLEST;
         break;
      case ATTACK_BIGGEST:
         selector = SELECT_BIGGEST;
         break;
      case ATTACK_MOST_ARMORED:
         selector = SELECT_MOST_ARMORED;
         break;
      case ATTACK_LEAST_ARMORED:
         selector = SELECT_LEAST_ARMORED;
         break;
      default:
         log("ERROR: doAttack called on non-attack order");
         return -1;
   }

   target = getEnemy( x_grid, y_grid, attack_range, facing, this, selector );

   if (target) {
      log("Bird attack");
      addProjectile( PR_WIND_SLASH, team, x_real, y_real, 3.0, attack_range, target, 0.0, 0.1 );
   }

   done_attack = 1;
   return 0;
}

int Bird::prepareTurn()
{
   if (alive != 1) return 0;

   if (active == 2) active = 1; // Start an active turn

   this_turn_order = Order( WAIT );

   if (aff_poison) {
      aff_poison--;
      takeDamage( c_poison_damage, DMG_POISON );
   }
   if (aff_burning) {
      aff_burning--;
      takeDamage( 10, DMG_FIRE );
   }
   if (aff_confusion) {
      aff_confusion--;
      if (aff_confusion %= 2)
         return 0;
   }

   if (aff_sleep > 0 || aff_timelock > 0) {
      if (aff_sleep > 0) aff_sleep--;
      if (aff_timelock > 0) aff_timelock--;
      return 0;
   }

   set<int> visited_orders;
   while (active == 1 && current_order != final_order 
         && visited_orders.find( current_order ) == visited_orders.end()) {
      this_turn_order = order_queue[current_order];
      visited_orders.insert( current_order );
      bool decision = evaluateConditional(this_turn_order.condition);
      int r = 1;
      // BIRD UNIQUE CODE
      if (shout_nesting > 0 && this_turn_order.action != BIRD_CMD_QUIET) {
         if (this_turn_order.action < PL_ALERT_ALL || shout_nesting > 1)
            broadcastOrder( this_turn_order, shout_listeners );
         else
            startPlayerCommandRange( this_turn_order, x_grid, y_grid, vision_range, shout_listeners );

         return 1;
      }

      if (this_turn_order.action <= WAIT)
         r = prepareBasicOrder(order_queue[current_order], decision);
      else if (this_turn_order.action == BIRD_CMD_SHOUT) {
         if (decision) {
            shout_nesting++;
            r = 0;
         }
      } else if (this_turn_order.action == BIRD_CMD_QUIET) {
         if (decision) {
            shout_nesting--;
            if (shout_nesting < 0) shout_nesting = 0;
            r = 0;
         }
      } else if (this_turn_order.action == BIRD_FLY) {
         if (!decision && this_turn_order.iteration > 0)
            this_turn_order.iteration = this_turn_order.count - 1;
      }
      // END
      // if prepareBasicOrder returns 0, it's a 0-length instruction (e.g. turn)
      if (r == 0) {
         current_order++;
         continue;
      }
      else 
         break;
   }

   if (current_order == final_order) {
      this_turn_order = Order( WAIT );
      if (flying) {
         this_turn_order = Order( BIRD_FLY, 2 );
         this_turn_order.iteration = 1;
      }
   }

   progress = 0;
   return 0;
}

int Bird::startTurn()
{
   if (alive != 1) return 0;

   startBasicOrder();

   if (this_turn_order.action == WAIT) {
      anim_data = (anim_data + 1) % 7;

      rand_int++;
      if (rand_int % 13 == 0)
         anim_data = -1;

      if (anim_data == 6 && rand_int % 2 == 0)
         anim_data = 7;
   }

   return 0;
}

int Bird::completeTurn()
{
   if (alive != 1) return 0;

   if (aff_sleep == -1)
      return aff_sleep = 0; // Just woke up

   if (aff_sleep > 0 || aff_timelock > 0)
      return 0;

   if (active == 1 && current_order != final_order) { 
      Order &o = order_queue[current_order]; 

      if (shout_nesting == 0) {
         int r = completeBasicOrder(this_turn_order);
         if (this_turn_order.action == BIRD_FLY
               && this_turn_order.iteration != this_turn_order.count - 1)
            flying = true;
         else flying = false;
         o.iteration++;
         if (o.iteration >= o.count && o.count != -1) { 
            current_order++;
            o.iteration = 0;
         }
         return r;
      }
      if (shout_nesting == 1 && o.action == PL_DELAY) {
         o.iteration++;
         if (o.iteration < o.count || o.count == -1)
            return 0;

         current_order++;
         o.iteration = 0;
         return 0;
      }
      if (shout_nesting > 0) {
         if (o.action >= PL_ALERT_ALL)
            completePlayerCommandRange( o, x_grid, y_grid, vision_range, shout_listeners );
         current_order++;
         return 0;
      }
   }

   return 0;
}

int Bird::update( float dtf )
{
   if (alive < 0) {
      alive += (int) (1000.0 * dtf);
      if (alive >= 0) alive = 0;
   }

   if (alive == 0)
      return 1;

   progress += dtf;
   if (active == 1 && current_order != final_order) {
      Order &o = this_turn_order;
      // Bird Specific
      if (shout_nesting > 0) return 0;

      if (o.action == MOVE_FORWARD || o.action == MOVE_BACK || this_turn_order.action == FOLLOW_PATH) {
         float d_real = 0.0, d_offset = 0.0;
         if (progress < .1) return 0;
         if (progress >= .1 && progress < .3) {
            d_real = (progress - .1) * 1.5; // 0->0.3
            d_offset = (progress - .1) * -0.5; // 0->-0.1
         } else if (progress > .3 && progress <= .4) {
            d_real = 0.3;
            d_offset = -0.1;
         } else if (progress >= .4 && progress < .6) {
            d_real = (progress - .25) * 2; // 0.3->0.7
            d_offset = (progress - .5); // -0.1->0.1
         } else if (progress > .6 && progress <= .7) {
            d_real = 0.7;
            d_offset = 0.1;
         } else if (progress >= .7 && progress < .9) {
            d_real = (progress - .2333) * 1.5; // 0.7->1.0
            d_offset = (progress - .9) * -0.5; // 0.1->0
         } else if (progress >= .9) {
            d_real = 1.0;
            d_offset = 0.0;
         }

         if (o.action == MOVE_BACK) d_real = -d_real;
         if (facing == NORTH) {
            y_real = y_grid + 0.5 - d_real;
            x_real = x_grid + 0.5 - d_offset;
         } else if (facing == SOUTH) {
            y_real = y_grid + 0.5 + d_real;
            x_real = x_grid + 0.5 + d_offset;
         } else if (facing == WEST) {
            x_real = x_grid + 0.5 - d_real;
            y_real = y_grid + 0.5 - d_offset;
         } else if (facing == EAST) {
            x_real = x_grid + 0.5 + d_real;
            y_real = y_grid + 0.5 + d_offset;
         }

         return 0;
      }
      // End
      return updateBasicOrder( dtf, o );
   }
   return 0;
}

sf::Texture* Bird::getTexture()
{
   return SFML_TextureManager::getSingleton().getTexture( "BirdStatic.png" );
}

Sprite *sp_bird = NULL;

int Bird::draw()
{
   // Select sprite
   Sprite *sp_bird = NULL;

   int rotation;
   if (facing == EAST) rotation = 0;
   if (facing == SOUTH) rotation = 90;
   if (facing == WEST) rotation = 180;
   if (facing == NORTH) rotation = 270;

   float scale = 0.6; // affected by height (flying)

   if (alive < 0) {
      // Death animation
      int t = alive + c_death_time + c_death_fade_time;
      if (t >= c_death_time) t = c_death_time - 1;
      sp_bird = bird_anim_death.getSprite( t );

      int alpha = 255;
      if (alive > -c_death_fade_time)
         alpha = 255 - ((c_death_fade_time + alive) * 256 / c_death_fade_time);
      sp_bird->setColor( Color( 255, 255, 255, alpha ) );
   } else if (aff_timelock > 0) {
      sp_bird = bird_anim_idle1.getSprite( 0 );
      sp_bird->setColor( Color( 255, 126, 0 ) );
   } else if (shout_nesting > 0) {
      sp_bird = bird_anim_shout.getSprite( (int)(progress * 1000) );
   } else if (this_turn_order.action == MOVE_FORWARD || this_turn_order.action == FOLLOW_PATH) {
      sp_bird = bird_anim_move.getSprite( (int)(progress * 1000) );
   } else if (this_turn_order.action == MOVE_BACK) {
      sp_bird = bird_anim_move.getSprite( 999 - (int)(progress * 1000) );
   } else if (this_turn_order.action >= ATTACK_CLOSEST && this_turn_order.action <= ATTACK_LEAST_ARMORED) {
      if (done_attack) {
         int d_anim = (int)( ((progress - speed) / (1-speed)) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_bird = bird_anim_attack_start.getSprite( 999 - d_anim );
      } else {
         int d_anim = (int)( (progress / speed) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_bird = bird_anim_attack_start.getSprite( d_anim );
      }
   } else if (this_turn_order.action == BIRD_FLY) {
      if (this_turn_order.iteration == 0) {
         sp_bird = bird_anim_take_off.getSprite( (int)(progress * 1000) );
         if (progress > 0.5)
            scale = 0.6 + ((progress - 0.5) * 0.4);
      } else if (this_turn_order.iteration == this_turn_order.count - 1) {
         sp_bird = bird_anim_land.getSprite( (int)(progress * 1000) );
         if (progress < 0.5)
            scale = 0.6 + ((0.5 - progress) * 0.4);
      } else {
         sp_bird = bird_anim_fly.getSprite( (int)(progress * 1000) );
         if (progress < 0.25)
            scale = 0.7 + ((0.25 - progress) * 0.4);
         else if (progress < 0.5)
            scale = 0.7 + ((progress - 0.25) * 0.4);
         else if (progress < 0.75)
            scale = 0.7 + ((0.75 - progress) * 0.4);
         else
            scale = 0.7 + ((progress - 0.75) * 0.4);
      }
   } else {
      if (anim_data >= 6) sp_bird = bird_anim_idle3.getSprite( (int)(progress * 1000) );
      else if (anim_data == 3) sp_bird = bird_anim_idle2.getSprite( (int)(progress * 1000) );
      else sp_bird = bird_anim_idle1.getSprite( (int)(progress * 1000) );
   }
   if (NULL == sp_bird) return -1;

   // Move/scale sprite
   sp_bird->setPosition( x_real, y_real );
   Vector2u dim (bird_anim_idle1.image_size_x, bird_anim_idle1.image_size_y);
   sp_bird->setOrigin( dim.x / 2.0, dim.y / 2.0 );
   sp_bird->setScale( scale / dim.x, scale / dim.y );

   if (anim_data % 2 == 1)
      sp_bird->scale( 1, -1 );

   sp_bird->setRotation( rotation );

   SFML_GlobalRenderWindow::get()->draw( *sp_bird );

   return 0;
}

string Bird::descriptor()
{
   return "Allied Bird";
} 

//////////////////////////////////////////////////////////////////////
// Bug ---

Animation bug_anim_idle1;
Animation bug_anim_idle1_stars[5];

Animation bug_anim_idle2;
Animation bug_anim_idle2_stars[5];

Animation bug_anim_move;
Animation bug_anim_move_stars[5];

Animation bug_anim_cast_start;
Animation bug_anim_cast_start_stars[5];

Animation bug_anim_death;
Animation bug_anim_death_stars[5];

void initBugAnimations()
{
   Texture *t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle1.png" );
   bug_anim_idle1.load( t, 128, 128, 6, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle1_Stars4.png" );
   bug_anim_idle1_stars[4].load( t, 128, 128, 6, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle1_Stars3.png" );
   bug_anim_idle1_stars[3].load( t, 128, 128, 6, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle1_Stars2.png" );
   bug_anim_idle1_stars[2].load( t, 128, 128, 6, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle1_Stars1.png" );
   bug_anim_idle1_stars[1].load( t, 128, 128, 6, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle1_Stars0.png" );
   bug_anim_idle1_stars[0].load( t, 128, 128, 6, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle2.png" );
   bug_anim_idle2.load( t, 128, 128, 15, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle2_Stars4.png" );
   bug_anim_idle2_stars[4].load( t, 128, 128, 15, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle2_Stars3.png" );
   bug_anim_idle2_stars[3].load( t, 128, 128, 15, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle2_Stars2.png" );
   bug_anim_idle2_stars[2].load( t, 128, 128, 15, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle2_Stars1.png" );
   bug_anim_idle2_stars[1].load( t, 128, 128, 15, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimIdle2_Stars0.png" );
   bug_anim_idle2_stars[0].load( t, 128, 128, 15, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimMove.png" );
   bug_anim_move.load( t, 128, 128, 12, 500 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimMove_Stars4.png" );
   bug_anim_move_stars[4].load( t, 128, 128, 12, 500 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimMove_Stars3.png" );
   bug_anim_move_stars[3].load( t, 128, 128, 12, 500 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimMove_Stars2.png" );
   bug_anim_move_stars[2].load( t, 128, 128, 12, 500 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimMove_Stars1.png" );
   bug_anim_move_stars[1].load( t, 128, 128, 12, 500 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimMove_Stars0.png" );
   bug_anim_move_stars[0].load( t, 128, 128, 12, 500 );

   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimCastStart.png" );
   bug_anim_cast_start.load( t, 128, 128, 12, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimCastStart_Stars4.png" );
   bug_anim_cast_start_stars[4].load( t, 128, 128, 12, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimCastStart_Stars3.png" );
   bug_anim_cast_start_stars[3].load( t, 128, 128, 12, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimCastStart_Stars2.png" );
   bug_anim_cast_start_stars[2].load( t, 128, 128, 12, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimCastStart_Stars1.png" );
   bug_anim_cast_start_stars[1].load( t, 128, 128, 12, 1000 );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimCastStart_Stars0.png" );
   bug_anim_cast_start_stars[0].load( t, 128, 128, 12, 1000 );

   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimDeath.png" );
   bug_anim_death.load( t, 128, 128, 7, c_death_time );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimDeath_Stars4.png" );
   bug_anim_death_stars[4].load( t, 128, 128, 7, c_death_time );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimDeath_Stars3.png" );
   bug_anim_death_stars[3].load( t, 128, 128, 7, c_death_time );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimDeath_Stars2.png" );
   bug_anim_death_stars[2].load( t, 128, 128, 7, c_death_time );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimDeath_Stars1.png" );
   bug_anim_death_stars[1].load( t, 128, 128, 7, c_death_time );
   t = SFML_TextureManager::getSingleton().getTexture( "BugAnimDeath_Stars0.png" );
   bug_anim_death_stars[0].load( t, 128, 128, 7, c_death_time );
}

// *tors
Bug::Bug()
{
   Bug( -1, -1, NORTH );
}

Bug::Bug( int x, int y, Direction face )
{
   log("Creating Bug");

   type = BUG_T;

   alive = 1;

   aff_poison = 0;
   aff_burning = 0;
   aff_confusion = 0;
   aff_sleep = 0;
   aff_timelock = 0;
    
   flying = false;
   invis = false;

   radius = 0.4;

   x_grid = x;
   y_grid = y;
   x_real = x + 0.5;
   y_real = y + 0.5;
   turnTo(face);

   health = max_health = c_bug_base_health * ( 1.0 + ( focus_toughness * 0.02 ) );

   vision_range = c_bug_base_vision * (1.0 + ((float)focus_perception / 25.0));

   attack_range = c_bug_base_vision;
   orb_speed = c_bug_orb_speed * ( 1.0 + ( focus_speed * 0.01) );

   speed = c_bug_base_speed * ( 1.0 - ( focus_speed * 0.02 ) );
   armor = c_bug_base_armor + (focus_toughness * 0.1);

   max_orders = c_bug_base_memory * ( 1.0 + ( focus_memory * 0.08 ) );
   order_queue = new Order[max_orders];
   clearOrders();

   this_turn_order = Order( WAIT );

   active = 0;
   team = 0;
   group = 1;
   progress = 0;
   anim_data = 0;

   win_condition = false;

   for (int i = 0; i < 12; ++i)
      dmg_display[i] = NULL;

   setStarCount( 4 );

   wormhole_number = -1;
}

Bug::~Bug()
{
   if (order_queue)
      delete order_queue;
}

int Bug::setStarCount( int count )
{
   if (count > 4) count = 4;
   if (count < 0) count = 0;

   return star_count = count;
}

// Virtual methods

int Bug::addOrder( Order o )
{
   if ((o.action <= WAIT) || (o.action >= BUG_CAST_FIREBALL && o.action <= BUG_MEDITATE)) {
      if (order_count >= max_orders) // No more memory
         return -2;

      if (o.action == BUG_OPEN_WORMHOLE || o.action == BUG_CLOSE_WORMHOLE)
         o.count = 3;

      order_queue[order_count] = o;
      order_count++;
      final_order = order_count;

      return 0;
   } else return -1;
}

int Bug::doAttack( Order o )
{
   log("Bug doAttack");
   int selector = SELECT_CLOSEST;
   Unit *target = NULL;
   switch(o.action) {
      case ATTACK_CLOSEST:
         selector = SELECT_CLOSEST;
         break;
      case ATTACK_FARTHEST:
         selector = SELECT_FARTHEST;
         break;
      case ATTACK_SMALLEST:
         selector = SELECT_SMALLEST;
         break;
      case ATTACK_BIGGEST:
         selector = SELECT_BIGGEST;
         break;
      case ATTACK_MOST_ARMORED:
         selector = SELECT_MOST_ARMORED;
         break;
      case ATTACK_LEAST_ARMORED:
         selector = SELECT_LEAST_ARMORED;
         break;
      default:
         log("ERROR: doAttack called on non-attack order");
         return -1;
   }

   target = getEnemy( x_grid, y_grid, attack_range, facing, this, selector );

   if (target) {
      log("Bug pre-generate");
      float gen_x = x_real, gen_y = y_real;
      addDirectionF( facing, gen_x, gen_y, 0.3 );
      
      addProjectile( PR_HOMING_ORB, team, gen_x, gen_y, orb_speed, attack_range, target, 100, 0.0 );
   }

   log("Bug finishing attack");

   done_attack = 1;
   return 0;
}

int Bug::prepareTurn()
{
   if (alive != 1) return 0;

   if (active == 2) active = 1; // Start an active turn

   this_turn_order = Order( WAIT );

   if (aff_poison) {
      aff_poison--;
      takeDamage( c_poison_damage, DMG_POISON );
   }
   if (aff_burning) {
      aff_burning--;
      takeDamage( 10, DMG_FIRE );
   }
   if (aff_confusion) {
      aff_confusion--;
      if (aff_confusion %= 2)
         return 0;
   }

   if (aff_sleep > 0 || aff_timelock > 0) {
      if (aff_sleep > 0) aff_sleep--;
      if (aff_timelock > 0) aff_timelock--;
      return 0;
   }

   set<int> visited_orders;
   while (active == 1 && current_order != final_order 
         && visited_orders.find( current_order ) == visited_orders.end()) {
      this_turn_order = order_queue[current_order];
      visited_orders.insert( current_order );
      bool decision = evaluateConditional(this_turn_order.condition);
      int r = 1;
      // BUG UNIQUE CODE
      if (this_turn_order.action <= WAIT)
         r = prepareBasicOrder(order_queue[current_order], decision);
      else if ((this_turn_order.action >= BUG_CAST_FIREBALL 
             && this_turn_order.action <= BUG_CLOSE_WORMHOLE)
             && star_count == 0)
      {
         r = 0;
      }
      else if (this_turn_order.action >= BUG_CAST_FIREBALL 
              && this_turn_order.action <= BUG_CAST_DUST)
      {
         if (decision)
            done_attack = false;
         else
            r = 0;
      }
      // END
      // if prepareBasicOrder returns 0, it's a 0-length instruction (e.g. turn)
      if (r == 0) {
         current_order++;
         continue;
      }
      else 
         break;
   }

   if (current_order == final_order) {
      this_turn_order = Order( WAIT );
   }

   progress = 0;
   return 0;

}

int Bug::startTurn()
{
   if (alive != 1) return 0;

   startBasicOrder();

   if (this_turn_order.action == WAIT) {
      anim_data = (anim_data + 1) % 6;

      rand_int++;
      if (rand_int % 13 == 0)
         anim_data = -1;
   }
   return 0;
}

int Bug::completeTurn()
{
   if (alive != 1) return 0;

   if (aff_sleep == -1)
      return aff_sleep = 0; // Just woke up

   if (aff_sleep > 0 || aff_timelock > 0)
      return 0;

   if (active == 1 && current_order != final_order) {
      int r = completeBasicOrder(this_turn_order);
      if (this_turn_order.action == BUG_MEDITATE && this_turn_order.iteration % 10 == 9)
         setStarCount( star_count + 1 );
      else if (this_turn_order.action >= BUG_CAST_FIREBALL 
            && this_turn_order.action <= BUG_CAST_DUST)
         setStarCount( star_count - 1 );
      else if (this_turn_order.action >= BUG_OPEN_WORMHOLE
            && this_turn_order.action <= BUG_CLOSE_WORMHOLE 
            && this_turn_order.iteration % 3 == 2) {
         setStarCount( star_count - 1 );
         addWormhole( x_grid, y_grid, wormhole_number, (this_turn_order.action == BUG_OPEN_WORMHOLE) );
      }
      Order &o = order_queue[current_order];
      o.iteration++;
      if (o.iteration >= o.count && o.count != -1) { 
         current_order++;
         o.iteration = 0;
      }
      return r;
   }

   return 0;
}

int Bug::update( float dtf )
{
   if (alive < 0) {
      alive += (int) (1000.0 * dtf);
      if (alive >= 0) alive = 0;
   }

   if (alive == 0)
      return 1;

   progress += dtf;
   if (active == 1 && current_order != final_order) {
      Order &o = this_turn_order;
      // Bug Specific
      if (o.action == BUG_CAST_FIREBALL || o.action == BUG_CAST_SHOCK || o.action == BUG_CAST_DUST) {
         if (!done_attack) {
            if (progress >= speed) {
               // Cast
               if (o.action == BUG_CAST_FIREBALL) {
                  float tgt_x = x_real, tgt_y = y_real;
                  addDirectionF( facing, tgt_x, tgt_y, 20 );
                  Unit* u = TargettingAid::get( tgt_x, tgt_y );
                  addProjectile( PR_FIREBALL_TRACER, team, x_real, y_real, orb_speed / 2, attack_range, u, -1, 0.1 );
               } else if (o.action == BUG_CAST_SHOCK) {
                  ShockWave *sw = new ShockWave( x_real, y_real, facing, attack_range );
                  addEffectManual( sw );
                  int x = x_grid, y = y_grid;
                  for (int i = 1; i < attack_range; ++i) {
                     if (addDirection( facing, x, y ) == 0) {
                        Unit *u = GRID_AT(unit_grid,x,y);
                        if (u) u->takeDamage( c_shock_damage, DMG_HEAVY );
                     }
                  }
               } else if (o.action == BUG_CAST_DUST) {
                  addEffect( EF_DUST_BURST, 0.4, x_real, y_real, 0, 0.3 );
               }

               done_attack = true;
            }
         }
         return 0;
      }
      // End
      return updateBasicOrder( dtf, o );
   }
   return 0;
}

sf::Texture* Bug::getTexture()
{
   if (star_count == 4) return SFML_TextureManager::getSingleton().getTexture( "BugStatic_Stars4.png" );
   if (star_count == 3) return SFML_TextureManager::getSingleton().getTexture( "BugStatic_Stars3.png" );
   if (star_count == 2) return SFML_TextureManager::getSingleton().getTexture( "BugStatic_Stars2.png" );
   if (star_count == 1) return SFML_TextureManager::getSingleton().getTexture( "BugStatic_Stars1.png" );
   return SFML_TextureManager::getSingleton().getTexture( "BugStatic_Stars0.png" );
}

Sprite *sp_bug = NULL;
Sprite *sp_bug_stars = NULL;

int Bug::draw()
{
   // Select sprite
   int rotation;
   if (facing == EAST) rotation = 0;
   if (facing == SOUTH) rotation = 90;
   if (facing == WEST) rotation = 180;
   if (facing == NORTH) rotation = 270;

   if (alive < 0) {
      // Death animation
      int t = alive + c_death_time + c_death_fade_time;
      if (t >= c_death_time) t = c_death_time - 1;
      sp_bug = bug_anim_death.getSprite( t );
      sp_bug_stars = bug_anim_death_stars[star_count].getSprite( t );

      int alpha = 255;
      if (alive > -c_death_fade_time)
         alpha = 255 - ((c_death_fade_time + alive) * 256 / c_death_fade_time);
      sp_bug->setColor( Color( 255, 255, 255, alpha ) );
      sp_bug_stars->setColor( Color( 255, 255, 255, alpha ) );
   } else if (aff_timelock > 0) {
      sp_bug = bug_anim_idle1.getSprite( 0 );
      sp_bug_stars = bug_anim_idle1_stars[star_count].getSprite( 0 );
      sp_bug->setColor( Color( 255, 126, 0 ) );
      sp_bug_stars->setColor( Color( 255, 126, 0 ) );
   } else if (this_turn_order.action == MOVE_FORWARD || this_turn_order.action == FOLLOW_PATH) {
      sp_bug = bug_anim_move.getSprite( (int)(progress * 1000) );
      sp_bug_stars = bug_anim_move_stars[star_count].getSprite( (int)(progress * 1000) );
   } else if (this_turn_order.action == MOVE_BACK) {
      sp_bug = bug_anim_move.getSprite( 999 - (int)(progress * 1000) );
      sp_bug_stars = bug_anim_move_stars[star_count].getSprite( 999 - (int)(progress * 1000) );
   } else if (this_turn_order.action >= ATTACK_CLOSEST && this_turn_order.action <= ATTACK_LEAST_ARMORED) {
      if (done_attack) {
         int d_anim = (int)( ((progress - speed) / (1-speed)) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_bug = bug_anim_cast_start.getSprite( 999 - d_anim );
         sp_bug_stars = bug_anim_cast_start_stars[star_count].getSprite( 999 - d_anim );
      } else {
         int d_anim = (int)( (progress / speed) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_bug = bug_anim_cast_start.getSprite( d_anim );
         sp_bug_stars = bug_anim_cast_start_stars[star_count].getSprite( d_anim );
      }
   } else if (this_turn_order.action >= BUG_CAST_FIREBALL && this_turn_order.action <= BUG_CAST_DUST) {
      if (done_attack) {
         int d_anim = (int)( ((progress - speed) / (1-speed)) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_bug = bug_anim_cast_start.getSprite( 999 - d_anim );
         sp_bug_stars = bug_anim_cast_start_stars[star_count].getSprite( 999 - d_anim );
      } else {
         int d_anim = (int)( (progress / speed) * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_bug = bug_anim_cast_start.getSprite( d_anim );
         sp_bug_stars = bug_anim_cast_start_stars[star_count].getSprite( d_anim );
      }
   } else if (this_turn_order.action == BUG_OPEN_WORMHOLE || this_turn_order.action == BUG_CLOSE_WORMHOLE) {
      if (this_turn_order.iteration == 2) {
         int d_anim = (int)(progress * 1000);
         if (d_anim >= 1000) d_anim = 999;
         sp_bug = bug_anim_cast_start.getSprite( 999 - d_anim );
         sp_bug_stars = bug_anim_cast_start_stars[star_count].getSprite( 999 - d_anim );

         // TODO: Draw wormhole
         Sprite sp_wormhole( *SFML_TextureManager::getSingleton().getTexture( "WormholeColorless.png" ) );
         normalizeTo1x1( &sp_wormhole );
         sp_wormhole.setPosition( x_grid, y_grid );
         int alpha = (int) ((d_anim * 255) / 1000);
         if (alpha > 255) alpha = 255;
         if (alpha < 0) alpha = 0;
         sp_wormhole.setColor( Color( 255, 255, 255, alpha ) );
         SFML_GlobalRenderWindow::get()->draw( sp_wormhole );

         // Draw glow receding
         Sprite sp_glow( *SFML_TextureManager::getSingleton().getTexture( "WormholeGlow.png" ) );
         normalizeTo1x1( &sp_glow );
         sp_glow.setPosition( x_grid, y_grid );
         alpha = (int) (((199 - d_anim) * 255) / 200);
         if (alpha > 255) alpha = 255;
         if (alpha < 0) alpha = 0;
         sp_glow.setColor( Color( 255, 255, 255, alpha ) );
         SFML_GlobalRenderWindow::get()->draw( sp_glow );

      } else {
         int d_anim = (int)((progress + this_turn_order.iteration) * 500) ; // out of 1000
         if (d_anim >= 1000) d_anim = 999;
         sp_bug = bug_anim_cast_start.getSprite( d_anim );
         sp_bug_stars = bug_anim_cast_start_stars[star_count].getSprite( d_anim );

         // Draw glow
         Sprite sp_glow( *SFML_TextureManager::getSingleton().getTexture( "WormholeGlow.png" ) );
         normalizeTo1x1( &sp_glow );
         sp_glow.setPosition( x_grid, y_grid );
         int alpha = (int) ((d_anim * 255) / 1000);
         if (alpha > 255) alpha = 255;
         if (alpha < 0) alpha = 0;
         sp_glow.setColor( Color( 255, 255, 255, alpha ) );
         SFML_GlobalRenderWindow::get()->draw( sp_glow );
         
      }
   } else {
      if (anim_data == 5) {
         sp_bug = bug_anim_idle2.getSprite( (int)(progress * 1000) );
         sp_bug_stars = bug_anim_idle2_stars[star_count].getSprite( (int)(progress * 1000) );
      } else {
         sp_bug = bug_anim_idle1.getSprite( (int)(progress * 1000) );
         sp_bug_stars = bug_anim_idle1_stars[star_count].getSprite( (int)(progress * 1000) );
      }
   }
   if (NULL == sp_bug) return -1;

   // Move/scale sprite
   sp_bug->setPosition( x_real, y_real );
   Vector2u dim (bug_anim_idle1.image_size_x, bug_anim_idle1.image_size_y);
   sp_bug->setOrigin( dim.x / 2.0, dim.y / 2.0 );
   sp_bug->setScale( 0.6 / dim.x, 0.6 / dim.y );

   sp_bug->setRotation( rotation );

   SFML_GlobalRenderWindow::get()->draw( *sp_bug );

   if (NULL != sp_bug_stars) {
      sp_bug_stars->setPosition( x_real, y_real );
      Vector2u dim (bug_anim_idle1.image_size_x, bug_anim_idle1.image_size_y);
      sp_bug_stars->setOrigin( dim.x / 2.0, dim.y / 2.0 );
      sp_bug_stars->setScale( 0.6 / dim.x, 0.6 / dim.y );

      sp_bug_stars->setRotation( rotation );

      SFML_GlobalRenderWindow::get()->draw( *sp_bug_stars );
   }

   return 0;
}

string Bug::descriptor()
{
   return "Allied Bug";
}

//////////////////////////////////////////////////////////////////////
// SummonMarker ---

SummonMarker::SummonMarker( )
{
   type = SUMMONMARKER_T;

   x_grid = 0;
   y_grid = 0;
   x_real = x_grid + 0.5;
   y_real = y_grid + 0.5;
   turnTo( SOUTH );

   alive = 0;

   aff_poison = 0;
   aff_burning = 0;
   aff_confusion = 0;
   aff_sleep = 0;
   aff_timelock = 0;
    
   flying = false;
   invis = false;

   health = max_health = 0;

   vision_range = 0;
   attack_range = 0;

   radius = 0.45;

   speed = 0.5;

   max_orders = 0;
   order_queue = NULL;
   clearOrders();

   this_turn_order = Order( WAIT );

   active = 0;
   team = 0;
   group = 1;
   progress = 0;
   
   win_condition = false;

   for (int i = 0; i < 12; ++i)
      dmg_display[i] = NULL;
}

SummonMarker *theSummonMarker = NULL;

SummonMarker* SummonMarker::get( int x, int y )
{
   if (NULL == theSummonMarker)
      theSummonMarker = new SummonMarker();
   
   theSummonMarker->x_grid = x;
   theSummonMarker->x_real = x + 0.5;

   theSummonMarker->y_grid = y;
   theSummonMarker->y_real = y + 0.5;

   theSummonMarker->progress = 0;
   theSummonMarker->rotation = 0;

   return theSummonMarker;
}

int SummonMarker::addOrder( Order o )
{
   // Does no orders
   return 0;
}

int SummonMarker::doAttack( Order o )
{
   // Does no attacks
   return 0;
}

sf::Texture* SummonMarker::getTexture()
{
   return SFML_TextureManager::getSingleton().getTexture( "SummoningCircle.png" );
}

int SummonMarker::update( float dtf )
{
   progress += dtf;
   rotation += (dtf * 450);
   return 0;
}

Sprite *sp_summon_marker_outside = NULL;
Sprite *sp_summon_marker_inside = NULL;

int SummonMarker::draw()
{
   if (NULL == sp_summon_marker_inside) {
      sp_summon_marker_inside = new Sprite(*getTexture());
      Vector2u dim = getTexture()->getSize();
      sp_summon_marker_inside->setOrigin( dim.x / 2.0, dim.y / 2.0 );
   }
   if (NULL == sp_summon_marker_outside) {
      sp_summon_marker_outside = new Sprite(*getTexture());
      Vector2u dim = getTexture()->getSize();
      sp_summon_marker_outside->setOrigin( dim.x / 2.0, dim.y / 2.0 );
   }

   normalizeTo1x1( sp_summon_marker_inside );
   normalizeTo1x1( sp_summon_marker_outside );

   sp_summon_marker_inside->setPosition( x_real, y_real );
   sp_summon_marker_outside->setPosition( x_real, y_real );

   if (progress < 0.6) {
      sp_summon_marker_inside->setRotation( rotation );
      sp_summon_marker_outside->setRotation( -rotation );

      float out_scale, in_scale;
      out_scale = (5.0 - (progress/0.3)) / 5.0;
      in_scale = (0.0 + (progress/0.2)) / 5.0;

      sp_summon_marker_outside->scale( out_scale, out_scale );
      sp_summon_marker_inside->scale( in_scale, in_scale );

      SFML_GlobalRenderWindow::get()->draw( *sp_summon_marker_outside );
      SFML_GlobalRenderWindow::get()->draw( *sp_summon_marker_inside );

      sp_summon_marker_outside->scale( -out_scale, -out_scale );
      sp_summon_marker_inside->scale( -in_scale, -in_scale );
   }
   else
   {
      int alpha = (int) ( ((progress - 0.6)/0.4) * 256);
      Color c_yellow( 255, 255, 0, alpha );
      CircleShape cir( 0.25 );
      cir.setFillColor( c_yellow );
      cir.setOrigin( 0.25, 0.25 );
      cir.setPosition( x_real, y_real );
      SFML_GlobalRenderWindow::get()->draw( cir );

      sp_summon_marker_inside->setRotation( 270 );
      sp_summon_marker_outside->setRotation( -270 );

      sp_summon_marker_outside->scale( 0.6, 0.6 );
      sp_summon_marker_inside->scale( 0.6, 0.6 );

      SFML_GlobalRenderWindow::get()->draw( *sp_summon_marker_outside );
      SFML_GlobalRenderWindow::get()->draw( *sp_summon_marker_inside );

      sp_summon_marker_outside->scale( -0.6, -0.6 );
      sp_summon_marker_inside->scale( -0.6, -0.6 );
   }


   return 0;
}

SummonMarker::~SummonMarker()
{

}

string SummonMarker::descriptor()
{
   return "Summon Marker";
}

//////////////////////////////////////////////////////////////////////
// TargetPractice ---

TargetPractice::TargetPractice( int x, int y, Direction face )
{
   type = TARGETPRACTICE_T;

   x_grid = x;
   y_grid = y;
   x_real = x_grid + 0.5;
   y_real = y_grid + 0.5;
   turnTo( face );

   alive = 1;

   aff_poison = 0;
   aff_burning = 0;
   aff_confusion = 0;
   aff_sleep = 0;
   aff_timelock = 0;
    
   flying = false;
   invis = false;

   health = max_health = 100000;

   vision_range = 0;
   attack_range = 0;

   radius = 0.45;

   speed = 0.5;

   max_orders = 0;
   order_queue = NULL;
   clearOrders();

   this_turn_order = Order( WAIT );

   active = 0;
   team = 99;
   group = 1;
   progress = 0;
   
   win_condition = false;

   for (int i = 0; i < 12; ++i)
      dmg_display[i] = NULL;
}

int TargetPractice::addOrder( Order o )
{
   // Does no orders
   return 0;
}

int TargetPractice::doAttack( Order o )
{
   // Does no attacks
   return 0;
}

sf::Texture* TargetPractice::getTexture()
{
   return SFML_TextureManager::getSingleton().getTexture( "TargetPractice.png" );
}

Sprite *sp_targ = NULL;

int TargetPractice::draw()
{
   if (NULL == sp_targ) {
      sp_targ = new Sprite(*getTexture());
      Vector2u dim = getTexture()->getSize();
      sp_targ->setOrigin( dim.x / 2.0, dim.y / 2.0 );
      normalizeTo1x1( sp_targ );
   }

   sp_targ->setPosition( x_real, y_real );
   SFML_GlobalRenderWindow::get()->draw( *sp_targ );

   return 0;

}

TargetPractice::~TargetPractice()
{

}

string TargetPractice::descriptor()
{
   return "Target";
}

//////////////////////////////////////////////////////////////////////
// TargettingAid ---

TargettingAid::TargettingAid( )
{
   type = TARGETTINGAID_T;

   alive = 0;
}

int TargettingAid::doAttack( Order o )
{
   return 0;
}

int TargettingAid::update( float dtf )
{
   return 0;
}

sf::Texture* TargettingAid::getTexture()
{
   return NULL;
}

TargettingAid *theTargettingAid = NULL;

TargettingAid* TargettingAid::get( int x, int y )
{
   if (NULL == theTargettingAid)
      theTargettingAid = new TargettingAid();
   
   theTargettingAid->x_grid = x;
   theTargettingAid->x_real = x + 0.5;

   theTargettingAid->y_grid = y;
   theTargettingAid->y_real = y + 0.5;

   return theTargettingAid;
}

int TargettingAid::draw()
{
   return 0;
}

TargettingAid::~TargettingAid()
{
}

string TargettingAid::descriptor()
{
   return "Targetting Aid";
}

//////////////////////////////////////////////////////////////////////
// The rest

Unit *genBaseUnit( UnitType t, int grid_x, int grid_y, Direction face )
{
   if (t == PLAYER_T) return NULL;
   if (t == MONSTER_T) return new Monster( grid_x, grid_y, face );
   if (t == SOLDIER_T) return new Soldier( grid_x, grid_y, face );
   if (t == WORM_T) return new Worm( grid_x, grid_y, face );
   if (t == BIRD_T) return new Bird( grid_x, grid_y, face );
   if (t == BUG_T) return new Bug( grid_x, grid_y, face );
   if (t == TARGETPRACTICE_T) return new TargetPractice( grid_x, grid_y, face );
   if (t == R_HUMAN_ARCHER_T) {
      AIUnit *u = new AIUnit( t, grid_x, grid_y, face, 1 );
      u->setAI( MV_HOLD_POSITION, AGR_PURSUE_VISIBLE, 10, 4, NULL );
      u->addWaypoint( grid_x, grid_y, face );
      return u;
   }
   if (t == M_HUMAN_SWORDSMAN_T) return NULL;
   if (t == M_DWARF_SPEAR_T) {
      AIUnit *u = new AIUnit( t, grid_x, grid_y, face, 1 );
      u->setAI( MV_HOLD_POSITION, AGR_PURSUE_VISIBLE, 10, 4, NULL );
      u->addWaypoint( grid_x, grid_y, face );
      return u;
   }
   if (t == SUMMONMARKER_T) return NULL;

   return NULL;
}

int initUnits()
{
   initMonsterAnimations();
   initSoldierAnimations();
   initWormAnimations();
   initBirdAnimations();
   initBugAnimations();

   initAIUnitAnimations();

   return 0;
}

};
