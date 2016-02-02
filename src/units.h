#ifndef __UNITS_H
#define __UNITS_H

#include "types.h"
#include "orders.h"
#include "effects.h"

#include <string>
#include <deque>
#include <list>

#include <SFML/System/Vector2.hpp>

namespace sf { class Texture; };

namespace sum
{

enum UnitType {
   // Unique
   PLAYER_T,
   MONSTER_T,
   SOLDIER_T,
   WORM_T,
   BIRD_T,
   BUG_T,
   TARGETPRACTICE_T,
   TARGETTINGAID_T,
   // Ranged Units
   R_HUMAN_ARCHER_T,
   // Melee Units
   M_HUMAN_SWORDSMAN_T,
   M_DWARF_SPEAR_T,
   // Special
   SUMMONMARKER_T
};

// BASE CLASS

class Unit
{
public:
   int x_grid, y_grid, x_next, y_next;
   float x_real, y_real;
   Direction facing;

   int team; // 0 = player
   int group; // 1 = main group
   UnitType type;

   int alive;

   int aff_poison;
   int aff_burning;
   int aff_confusion;
   int aff_sleep;
   int aff_timelock;

   bool flying, invis;

   float health, max_health;
   float speed; // 0.0-1.0 = when do moves complete?
   float vision_range;
   float attack_range;
   float armor;

   float radius;

   float progress;
   int done_attack; // One attack per turn!

   int anim_data;

   bool win_condition; // If true, must die to win the level

   Order *order_queue;
   Order this_turn_order;
   int current_order, final_order, max_orders, order_count;
   int current_iteration;
   int active;

   DamageDisplay *dmg_display[12]; // 6 vertical x 2 horizontal

   int turnTo( Direction face );

   int displayDamage( int damage, DamageType type );

   int prepareBasicOrder( Order &o, bool cond );
   int startBasicOrder( );
   int updateBasicOrder( float dtf, Order o );
   int completeBasicOrder( Order &o );

   bool evaluateConditional( Order_Conditional oc );
   bool testInvis();

   void activate();
   void clearOrders();

   void logOrders();
   virtual int addOrder( Order o );

   virtual int doAttack( Order o ) = 0;

   virtual int takeDamage( float damage, DamageType type = DMG_NORMAL );

   virtual std::string descriptor();

   virtual int prepareTurn();
   virtual int startTurn();
   virtual int completeTurn();
   virtual int update( float dtf );
   virtual sf::Texture* getTexture() = 0;
   virtual int draw() = 0;

   virtual ~Unit();
};

// GENERIC UNIT CLASSES

enum AI_Movement {
   MV_HOLD_POSITION,
   MV_PATROL,
   MV_PATROL_PATH,
   MV_FOLLOW_ALLY,
   MV_FREE_ROAM
};

enum AI_Aggression {
   AGR_FLEE,
   AGR_PASSIVE,
   AGR_DEFEND_SELF,
   AGR_ATTACK_IN_RANGE,
   AGR_PURSUE_VISIBLE
};

class AIUnit : public Unit
{
private:
   AIUnit(); // disallowed

public:
   // AI movement data
   AI_Movement ai_move_style;
   std::deque< std::pair<sf::Vector2i,Direction> > ai_waypoints;
   int ai_waypoint_next;
   std::deque<Direction> ai_pathing;

   // AI other data
   AI_Aggression ai_aggro;
   float ai_chase_distance;
   float ai_attack_distance;
   Unit* ai_leader;
   bool ai_overridden;
   int ai_counter;
   int aggroed;

   void setAI( AI_Movement move, AI_Aggression aggro, float chase_dis, float attack_dis, Unit* follow = NULL );

   int addWaypoint( int x, int y, Direction d = ALL_DIR );
   void clearWaypoints();
   int reconstructPath( Direction *came_from_grid, int goal_x, int goal_y );
   int aStar( int start_x, int start_y, int goal_x, int goal_y );
   int aiCalculatePathing( bool next = false );
   int aiFollowPathing();

   virtual int ai();

   AIUnit( UnitType t, int x, int y, Direction face, int my_team );

   //virtual int addOrder( Order o );

   virtual int doAttack( Order o );

   virtual int takeDamage( float damage, DamageType type = DMG_NORMAL );

   virtual std::string descriptor();

   virtual int prepareTurn();
   //virtual int startTurn();
   virtual int completeTurn();
   virtual int update( float dtf );
   virtual sf::Texture* getTexture();
   virtual int draw();

   virtual ~AIUnit();
};

// SPECIALIZED CLASSES

class Player : public Unit
{
private:
   Player();
   int init( int grid_x, int grid_y, Direction facing );

   int bird_shout_level;
public:
   static Player* initPlayer( int grid_x, int grid_y, Direction facing );

   virtual int addOrder( Order o );

   virtual int doAttack( Order o );

   virtual std::string descriptor();

   virtual int prepareTurn();
   virtual int startTurn();
   virtual int completeTurn();
   virtual int update( float dtf );
   virtual sf::Texture* getTexture();
   virtual int draw();

   ~Player();
};

class SummonMarker : public Unit
{
private:
   SummonMarker();
public:
   float rotation;
   static SummonMarker* get( int grid_x, int grid_y );

   virtual std::string descriptor();

   virtual int addOrder( Order o );
   virtual int doAttack( Order o );
   virtual int update( float dtf );
   virtual sf::Texture* getTexture();
   virtual int draw();

   ~SummonMarker();
};

class Monster : public Unit
{
private:
   Monster(); // Disallowed 

   float hardness;
public:
   Monster( int grid_x, int grid_y, Direction face );

   void doBurst();
   float setHardness( float hard );

   virtual int takeDamage( float damage, DamageType type = DMG_NORMAL );

   virtual int addOrder( Order o );

   virtual int doAttack( Order o );

   virtual std::string descriptor();

   virtual int prepareTurn();
   //virtual int startTurn();
   //virtual int completeTurn();
   virtual int update( float dtf );
   virtual sf::Texture* getTexture();
   virtual int draw();

   ~Monster();

};

class Soldier : public Unit
{
private:
   Soldier(); // Disallowed 

public:
   int stance; // 0 - axe, 1 - spear, 2 - bow

   Soldier( int grid_x, int grid_y, Direction face );

   virtual int addOrder( Order o );

   virtual int doAttack( Order o );

   virtual std::string descriptor();

   virtual int prepareTurn();
   //virtual int startTurn();
   //virtual int completeTurn();
   //virtual int update( float dtf );
   virtual sf::Texture* getTexture();
   virtual int draw();

   ~Soldier();

};

class Worm : public Unit
{
private:
   Worm(); // Disallowed 

public:
   bool trail;

   Worm( int grid_x, int grid_y, Direction face );

   virtual int takeDamage( float damage, DamageType type = DMG_NORMAL );

   virtual int addOrder( Order o );

   virtual int doAttack( Order o );

   virtual std::string descriptor();

   virtual int prepareTurn();
   virtual int startTurn();
   virtual int completeTurn();
   //virtual int update( float dtf );
   virtual sf::Texture* getTexture();
   virtual int draw();

   ~Worm();

};

class Bird : public Unit
{
private:
   Bird(); // Disallowed 

public:
   int shout_nesting;
   std::list<Unit*> shout_listeners;

   Bird( int grid_x, int grid_y, Direction face );

   virtual int addOrder( Order o );

   virtual int doAttack( Order o );

   virtual std::string descriptor();

   virtual int prepareTurn();
   virtual int startTurn();
   virtual int completeTurn();
   virtual int update( float dtf );
   virtual sf::Texture* getTexture();
   virtual int draw();

   ~Bird();

};

class Bug : public Unit
{
private:
   Bug(); // Disallowed 
   int star_count;
   int wormhole_number;

public:
   float orb_speed;
   
   int setStarCount( int count );

   Bug( int grid_x, int grid_y, Direction face );

   virtual int addOrder( Order o );

   virtual int doAttack( Order o );

   virtual std::string descriptor();

   virtual int prepareTurn();
   virtual int startTurn();
   virtual int completeTurn();
   virtual int update( float dtf );
   virtual sf::Texture* getTexture();
   virtual int draw();

   ~Bug();
};

class TargetPractice : public Unit
{
private:
   TargetPractice();
public:
   TargetPractice( int grid_x, int grid_y, Direction face );

   virtual std::string descriptor();

   virtual int addOrder( Order o );
   virtual int doAttack( Order o );
   virtual sf::Texture* getTexture();
   virtual int draw();
   ~TargetPractice();
};

class TargettingAid : public Unit
{
public:
   TargettingAid();
   virtual std::string descriptor();
   virtual int update( float dtf );
   virtual int doAttack( Order o );
   virtual sf::Texture* getTexture();
   virtual int draw();

   static TargettingAid* get( int x, int y );

   ~TargettingAid();
};

Unit *genBaseUnit( UnitType t, int grid_x, int grid_y, Direction face );

// OTHER

int initUnits();
bool testUnitCanMove( Unit *u );

};

#endif
