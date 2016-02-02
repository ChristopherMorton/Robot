#include "menustate.h"
#include "orders.h"
#include "log.h"
#include "util.h"
#include "types.h"
#include "config.h"

#include "SFML_GlobalRenderWindow.hpp"
#include "SFML_TextureManager.hpp"
#include "IMGuiManager.hpp"

#include <SFML/Graphics.hpp>

#include <string>
#include <sstream>

namespace sum
{

void Order::initOrder( Order_Action a, Order_Conditional c, int cnt )
{
   // 0 ignores groups on player actions, otherwise is nonsense
   if (cnt == 0 && a < PL_ALERT_ALL ) cnt = 1;

   action = a;

   if (a == BUG_MEDITATE)
      cnt *= MEDITATE_DURATION;

   if ((a == MONSTER_GUARD || a == BIRD_FLY) && cnt == 1)
      cnt = 2;

   if (a == BUG_OPEN_WORMHOLE || a == BUG_CLOSE_WORMHOLE)
      cnt = 3;

   condition = c;
   count = cnt;
   iteration = 0;
}

Order::Order( Order_Action a, Order_Conditional c, int cnt )
{
   initOrder( a, c, cnt );
}

Order::Order( Order_Action a, Order_Conditional c )
{
   initOrder( a, c, 1 );
}

Order::Order( Order_Action a, int cnt)
{
   initOrder( a, TRUE, cnt );
}

Order::Order( Order_Action a )
{
   initOrder( a, TRUE, 1 );
}

Order::Order()
{
   initOrder( WAIT, TRUE, 1 ); 
}

/*
Order::Order( Order &right )
{
   initOrder( right.action, right.condition, right.count );
}
*/

void Order::logSelf()
{
   std::stringstream s;
   switch(action) {
      case MOVE_FORWARD:
         s << "MOVE_FORWARD";
         break;
      case MOVE_BACK:
         s << "MOVE_BACK";
         break;
      case TURN_NORTH:
         s << "TURN_NORTH";
         break;
      case TURN_EAST:
         s << "TURN_EAST";
         break;
      case TURN_SOUTH:
         s << "TURN_SOUTH";
         break;
      case TURN_WEST:
         s << "TURN_WEST";
         break;
      case TURN_NEAREST_ENEMY:
         s << "TURN_NEAREST_ENEMY";
         break;
      case FOLLOW_PATH:
         s << "FOLLOW_PATH";
         break;
      case ATTACK_CLOSEST:
         s << "ATTACK_CLOSEST";
         break;
      case ATTACK_FARTHEST:
         s << "ATTACK_FARTHEST";
         break;
      case ATTACK_BIGGEST:
         s << "ATTACK_BIGGEST";
         break;
      case ATTACK_SMALLEST:
         s << "ATTACK_SMALLEST";
         break;
      case ATTACK_MOST_ARMORED:
         s << "ATTACK_MOST_ARMORED";
         break;
      case ATTACK_LEAST_ARMORED:
         s << "ATTACK_LEAST_ARMORED";
         break;
      case START_BLOCK:
         s << "START_BLOCK";
         break;
      case END_BLOCK:
         s << "END_BLOCK";
         break;
      case REPEAT:
         s << "REPEAT";
         break;
      case WAIT:
         s << "WAIT";
         break;
      case BUMP:
         s << "BUMP";
         break;
      case SKIP:
         s << "SKIP";
         break;
      case MONSTER_GUARD:
         s << "MONSTER_GUARD";
         break;
      case MONSTER_BURST:
         s << "MONSTER_BURST";
         break;
      case SOLDIER_SWITCH_AXE:
         s << "SOLDIER_SWITCH_AXE";
         break;
      case SOLDIER_SWITCH_SPEAR:
         s << "SOLDIER_SWITCH_SPEAR";
         break;
      case SOLDIER_SWITCH_BOW:
         s << "SOLDIER_SWITCH_BOW";
         break;
      case WORM_HIDE:
         s << "WORM_HIDE";
         break;
      case WORM_TRAIL_START:
         s << "WORM_TRAIL_START";
         break;
      case WORM_TRAIL_END:
         s << "WORM_TRAIL_END";
         break;
      case BIRD_CMD_SHOUT:
         s << "BIRD_CMD_SHOUT";
         break;
      case BIRD_CMD_QUIET:
         s << "BIRD_CMD_QUIET";
         break;
      case BIRD_FLY:
         s << "BIRD_FLY";
         break;
      case BUG_CAST_FIREBALL:
         s << "BUG_CAST_FIREBALL";
         break;
      case BUG_CAST_SHOCK:
         s << "BUG_CAST_SHOCK";
         break;
      case BUG_CAST_DUST:
         s << "BUG_CAST_DUST";
         break;
      case BUG_OPEN_WORMHOLE:
         s << "BUG_OPEN_WORMHOLE";
         break;
      case BUG_CLOSE_WORMHOLE:
         s << "BUG_CLOSE_WORMHOLE";
         break;
      case BUG_MEDITATE:
         s << "BUG_MEDITATE";
         break;
      case PL_SET_GROUP:
         s << "PL_SET_GROUP";
         break;
      case PL_ALERT_ALL:
         s << "PL_ALERT_ALL";
         break;
      case PL_ALERT_MONSTERS:
         s << "PL_ALERT_MONSTERS";
         break;
      case PL_ALERT_SOLDIERS:
         s << "PL_ALERT_SOLDIERS";
         break;
      case PL_ALERT_WORMS:
         s << "PL_ALERT_WORMS";
         break;
      case PL_ALERT_BIRDS:
         s << "PL_ALERT_BIRDS";
         break;
      case PL_ALERT_BUGS:
         s << "PL_ALERT_BUGS";
         break;
      case PL_CMD_GO:
         s << "PL_CMD_GO";
         break;
      case PL_CMD_GO_ALL:
         s << "PL_CMD_GO_ALL";
         break;
      case PL_CMD_GO_MONSTERS:
         s << "PL_CMD_GO_MONSTERS";
         break;
      case PL_CMD_GO_SOLDIERS:
         s << "PL_CMD_GO_SOLDIERS";
         break;
      case PL_CMD_GO_WORMS:
         s << "PL_CMD_GO_WORMS";
         break;
      case PL_CMD_GO_BIRDS:
         s << "PL_CMD_GO_BIRDS";
         break;
      case PL_CMD_GO_BUGS:
         s << "PL_CMD_GO_BUGS";
         break;
      case PL_DELAY:
         s << "PL_DELAY";
         break;
      case PL_CAST_HEAL:
         s << "PL_CAST_HEAL";
         break;
      case PL_CAST_LIGHTNING:
         s << "PL_CAST_LIGHTNING";
         break;
      case PL_CAST_TIMELOCK:
         s << "PL_CAST_TIMELOCK";
         break;
      case PL_CAST_SCRY:
         s << "PL_CAST_SCRY";
         break;
      case PL_CAST_CONFUSION:
         s << "PL_CAST_CONFUSION";
         break;
      case SUMMON_MONSTER:
         s << "SUMMON_MONSTER";
         break;
      case SUMMON_SOLDIER:
         s << "SUMMON_SOLDIER";
         break;
      case SUMMON_WORM:
         s << "SUMMON_WORM";
         break;
      case SUMMON_BIRD:
         s << "SUMMON_BIRD";
         break;
      case SUMMON_BUG:
         s << "SUMMON_BUG";
         break;
      case FAILED_SUMMON:
         s << "FAILED_SUMMON";
         break;
      case NUM_ACTIONS:
         s << "NUM_ACTIONS";
         break;
   }
   s << " - ";
   switch(condition) {
      case TRUE:
         s << "TRUE";
         break;
      case ENEMY_ADJACENT:
         s << "ENEMY_ADJACENT";
         break;
      case ENEMY_NOT_ADJACENT:
         s << "ENEMY_NOT_ADJACENT";
         break;
      case ENEMY_AHEAD:
         s << "ENEMY_AHEAD";
         break;
      case ENEMY_NOT_AHEAD:
         s << "ENEMY_NOT_AHEAD";
         break;
      case ENEMY_IN_RANGE:
         s << "ENEMY_IN_RANGE";
         break;
      case ENEMY_NOT_IN_RANGE:
         s << "ENEMY_NOT_IN_RANGE";
         break;
         /*
      case ALLY_ADJACENT:
         s << "ALLY_ADJACENT";
         break;
      case ALLY_NOT_ADJACENT:
         s << "ALLY_NOT_ADJACENT";
         break;
      case ALLY_AHEAD:
         s << "ALLY_AHEAD";
         break;
      case ALLY_NOT_AHEAD:
         s << "ALLY_NOT_AHEAD";
         break;
      case ALLY_IN_RANGE:
         s << "ALLY_IN_RANGE";
         break;
      case ALLY_NOT_IN_RANGE:
         s << "ALLY_NOT_IN_RANGE";
         break;
         */
      case HEALTH_OVER_50:
         s << "HEALTH_OVER_50";
         break;
      case HEALTH_UNDER_50:
         s << "HEALTH_UNDER_50";
         break;
      case HEALTH_OVER_20:
         s << "HEALTH_OVER_20";
         break;
      case HEALTH_UNDER_20:
         s << "HEALTH_UNDER_20";
         break;
      case BLOCKED_AHEAD:
         s << "BLOCKED_AHEAD";
         break;
      case NOT_BLOCKED_AHEAD:
         s << "NOT_BLOCKED_AHEAD";
         break;
      case NUM_CONDITIONALS:
         s << "NUM_CONDITIONALS";
         break;
      default:
         s << "default conditional";
         break;
   }
   s <<  " - " << count << "(" << iteration << ")";

   log(s.str());
}

Texture *getOrderTexture( Order o )
{
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();

   Texture *base_tex = NULL;
   switch (o.action)
   {
      case MOVE_FORWARD:
         base_tex = t_manager.getTexture( "OrderMoveForward.png" );
         break;
      case MOVE_BACK:
         base_tex = t_manager.getTexture( "OrderMoveBackward.png" );
         break;
      case TURN_NORTH:
         base_tex = t_manager.getTexture( "OrderTurnNorth.png" );
         break;
      case TURN_EAST:
         base_tex = t_manager.getTexture( "OrderTurnEast.png" );
         break;
      case TURN_SOUTH:
         base_tex = t_manager.getTexture( "OrderTurnSouth.png" );
         break;
      case TURN_WEST:
         base_tex = t_manager.getTexture( "OrderTurnWest.png" );
         break;
      case TURN_NEAREST_ENEMY:
         base_tex = t_manager.getTexture( "OrderTurnNearestEnemy.png" );
         break;
      case FOLLOW_PATH:
         base_tex = t_manager.getTexture( "OrderFollowPath.png" );
         break;

      case ATTACK_CLOSEST:
         base_tex = t_manager.getTexture( "OrderAttackClosest.png" );
         break;
      case ATTACK_FARTHEST:
         base_tex = t_manager.getTexture( "OrderAttackFarthest.png" );
         break;
      case ATTACK_BIGGEST:
         base_tex = t_manager.getTexture( "OrderAttackBiggest.png" );
         break;
      case ATTACK_SMALLEST:
         base_tex = t_manager.getTexture( "OrderAttackSmallest.png" );
         break;
      case ATTACK_MOST_ARMORED:
         base_tex = t_manager.getTexture( "OrderAttackMostArmored.png" );
         break;
      case ATTACK_LEAST_ARMORED:
         base_tex = t_manager.getTexture( "OrderAttackLeastArmored.png" );
         break;

      case START_BLOCK:
         base_tex = t_manager.getTexture( "ControlStartBlock.png" );
         break;
      case END_BLOCK:
         base_tex = t_manager.getTexture( "ControlEndBlock.png" );
         break;
      case REPEAT:
         base_tex = t_manager.getTexture( "ControlRepeat.png" );
         break;

      case WAIT:
         base_tex = t_manager.getTexture( "OrderWait.png" );
         break;

         // Unique unit orders
      case MONSTER_GUARD:
         base_tex = t_manager.getTexture( "MonsterOrderGuard.png" );
         break;
      case MONSTER_BURST:
         base_tex = t_manager.getTexture( "MonsterOrderBurst.png" );
         break;
      case SOLDIER_SWITCH_AXE:
         base_tex = t_manager.getTexture( "SoldierOrderSwitchAxe.png" );
         break;
      case SOLDIER_SWITCH_SPEAR:
         base_tex = t_manager.getTexture( "SoldierOrderSwitchSpear.png" );
         break;
      case SOLDIER_SWITCH_BOW:
         base_tex = t_manager.getTexture( "SoldierOrderSwitchBow.png" );
         break;
      case WORM_TRAIL_START:
         base_tex = t_manager.getTexture( "WormOrderTrailOn.png" );
         break;
      case WORM_TRAIL_END:
         base_tex = t_manager.getTexture( "WormOrderTrailOff.png" );
         break;
      case WORM_HIDE:
         base_tex = t_manager.getTexture( "WormOrderHide.png" );
         break;
      case BIRD_FLY:
         base_tex = t_manager.getTexture( "BirdOrderFly.png" );
         break;
      case BIRD_CMD_SHOUT:
         base_tex = t_manager.getTexture( "BirdControlShout.png" );
         break;
      case BIRD_CMD_QUIET:
         base_tex = t_manager.getTexture( "BirdControlQuiet.png" );
         break;
      case BUG_MEDITATE:
         base_tex = t_manager.getTexture( "BugOrderMeditate.png" );
         break;
      case PL_SET_GROUP:
         base_tex = t_manager.getTexture( "StarFull.png" );
         break;
      case BUG_CAST_FIREBALL:
         base_tex = t_manager.getTexture( "BugOrderFireball.png" );
         break;
      case BUG_CAST_SHOCK:
         base_tex = t_manager.getTexture( "BugOrderSunder.png" );
         break;
      case BUG_CAST_DUST:
         base_tex = t_manager.getTexture( "BugOrderHeal.png" );
         break;
      case BUG_OPEN_WORMHOLE:
         base_tex = t_manager.getTexture( "BugOrderOpenWormhole.png" );
         break;
      case BUG_CLOSE_WORMHOLE:
         base_tex = t_manager.getTexture( "BugOrderCloseWormhole.png" );
         break;

      case PL_ALERT_ALL:
         base_tex = t_manager.getTexture( "PlayerAlert.png" );
         break;
      case PL_ALERT_MONSTERS:
         base_tex = t_manager.getTexture( "MonsterAlertButton.png" );
         break;
      case PL_ALERT_SOLDIERS:
         base_tex = t_manager.getTexture( "SoldierAlertButton.png" );
         break;
      case PL_ALERT_WORMS:
         base_tex = t_manager.getTexture( "WormAlertButton.png" );
         break;
      case PL_ALERT_BIRDS:
         base_tex = t_manager.getTexture( "BirdAlertButton.png" );
         break;
      case PL_ALERT_BUGS:
         base_tex = t_manager.getTexture( "BugAlertButton.png" );
         break;

      case PL_CMD_GO:
         base_tex = t_manager.getTexture( "PlayerGoButton.png" );
         break;
      case PL_CMD_GO_ALL:
         base_tex = t_manager.getTexture( "PlayerGoAllButton.png" );
         break;
      case PL_CMD_GO_MONSTERS:
         base_tex = t_manager.getTexture( "MonsterGoButton.png" );
         break;
      case PL_CMD_GO_SOLDIERS:
         base_tex = t_manager.getTexture( "SoldierGoButton.png" );
         break;
      case PL_CMD_GO_WORMS:
         base_tex = t_manager.getTexture( "WormGoButton.png" );
         break;
      case PL_CMD_GO_BIRDS:
         base_tex = t_manager.getTexture( "BirdGoButton.png" );
         break;
      case PL_CMD_GO_BUGS:
         base_tex = t_manager.getTexture( "BugGoButton.png" );
         break;

      case PL_DELAY:
         base_tex = t_manager.getTexture( "OrderWait.png" );
         break;

      case PL_CAST_HEAL:
         base_tex = t_manager.getTexture( "CastHeal.png" );
         break;
      case PL_CAST_LIGHTNING:
         base_tex = t_manager.getTexture( "CastLightning.png" );
         break;
      case PL_CAST_TIMELOCK:
         base_tex = t_manager.getTexture( "CastTimelock.png" );
         break;
      case PL_CAST_SCRY:
         base_tex = t_manager.getTexture( "CastScry.png" );
         break;
      case PL_CAST_CONFUSION:
         base_tex = t_manager.getTexture( "CastConfusion.png" );
         break;

      case SUMMON_MONSTER:
         base_tex = t_manager.getTexture( "SummonMonster.png" );
         break;
      case SUMMON_SOLDIER:
         base_tex = t_manager.getTexture( "SummonSoldier.png" );
         break;
      case SUMMON_WORM:
         base_tex = t_manager.getTexture( "SummonWorm.png" );
         break;
      case SUMMON_BIRD:
         base_tex = t_manager.getTexture( "SummonBird.png" );
         break;
      case SUMMON_BUG:
         base_tex = t_manager.getTexture( "SummonBug.png" );
         break;
      case FAILED_SUMMON:
         base_tex = t_manager.getTexture( "FailedSummon.png" );
         break;

      case NUM_ACTIONS:
         base_tex = t_manager.getTexture( "EmptyMemory.png" );
         break;

      case SKIP:
      default:
         base_tex = NULL;

   }
   return base_tex;
}

Texture *getOrderButtonTexture( Order o )
{
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();

   switch (o.action)
   {
      case MOVE_FORWARD:
      case MOVE_BACK:
      case TURN_NORTH:
      case TURN_EAST:
      case TURN_SOUTH:
      case TURN_WEST: 
      case TURN_NEAREST_ENEMY: 
      case FOLLOW_PATH:
      case ATTACK_CLOSEST:
      case ATTACK_FARTHEST:
      case ATTACK_BIGGEST:
      case ATTACK_SMALLEST:
      case ATTACK_MOST_ARMORED:
      case ATTACK_LEAST_ARMORED:
      case WAIT:
         return t_manager.getTexture( "OrderButtonBase.png" );

      case START_BLOCK:
      case END_BLOCK:
      case REPEAT:
         return t_manager.getTexture( "ControlButtonBase.png" );

      // Monster
      case MONSTER_GUARD:
      case MONSTER_BURST:
         return t_manager.getTexture( "MonsterOrderButtonBase.png" );

      // Soldier
      case SOLDIER_SWITCH_AXE:
      case SOLDIER_SWITCH_SPEAR:
      case SOLDIER_SWITCH_BOW:
         return t_manager.getTexture( "SoldierOrderButtonBase.png" );

      // Worm
      case WORM_HIDE:
      case WORM_TRAIL_START:
      case WORM_TRAIL_END:
         return t_manager.getTexture( "WormOrderButtonBase.png" );

      // Bird
      case BIRD_FLY:
         return t_manager.getTexture( "BirdOrderButtonBase.png" );
      case BIRD_CMD_SHOUT:
      case BIRD_CMD_QUIET:
         return t_manager.getTexture( "BirdControlButtonBase.png" );

      // Bug
      case BUG_CAST_FIREBALL:
      case BUG_CAST_SHOCK:
      case BUG_CAST_DUST:
      case BUG_OPEN_WORMHOLE:
      case BUG_CLOSE_WORMHOLE:
      case BUG_MEDITATE:
         return t_manager.getTexture( "BugOrderButtonBase.png" );

      // Player spells
      case PL_CAST_HEAL:
      case PL_CAST_LIGHTNING:
      case PL_CAST_TIMELOCK:
      case PL_CAST_SCRY:
      case PL_CAST_CONFUSION:
         return t_manager.getTexture( "CastButtonBase.png" );

      default:
         return NULL;
   };
}

void drawKeybind( KeybindTarget kb, int x, int y, int size, int char_size )
{
   if (kb == KB_NOTHING)
      return;

   Keyboard::Key key = config::getBoundKey( kb );

   if (key == Keyboard::Unknown)
      return;

   Text *key_text = new Text();
   key_text->setString( keyToString( key ) );
   key_text->setFont( *menu_font );
   key_text->setColor( Color::Black );
   key_text->setCharacterSize( char_size );
   float text_x = x + 1,
         text_y = y - 1;
   key_text->setPosition( text_x, text_y );

   FloatRect text_size = key_text->getGlobalBounds();
   if (text_size.width < 5) text_size.width = 5;

   // TODO: Make this look right
   RectangleShape *inner_rect = new RectangleShape();
   inner_rect->setSize( Vector2f((text_size.width + 2), char_size + 1 ));
   inner_rect->setPosition( text_x - 1, text_y + 1 );
   inner_rect->setFillColor( Color( 245, 245, 245, 255 ) );
   inner_rect->setOutlineColor( Color::Black );
   inner_rect->setOutlineThickness( 1.0 );

   ConvexShape *outer_top = new ConvexShape(3);
   outer_top->setPoint( 0, Vector2f (text_x - 3, text_y - 2 ));
   outer_top->setPoint( 1, Vector2f (text_x + text_size.width + 3, text_y - 2 ));
   outer_top->setPoint( 2, Vector2f (text_x + (text_size.width / 2) , text_y + (char_size / 2) + 2 ));
   outer_top->setFillColor( Color( 215, 215, 215, 255 ) );
   outer_top->setOutlineColor( Color::Black );
   outer_top->setOutlineThickness( 1.0 );
   ConvexShape *outer_bottom = new ConvexShape(3);
   outer_bottom->setPoint( 0, Vector2f (text_x - 3, text_y + char_size + 5 ));
   outer_bottom->setPoint( 2, Vector2f (text_x + text_size.width + 3, text_y + char_size + 5 ));
   outer_bottom->setPoint( 1, Vector2f (text_x + (text_size.width / 2) , text_y + (char_size / 2) + 2 ));
   outer_bottom->setFillColor( Color( 135, 135, 135, 255 ) );
   outer_bottom->setOutlineColor( Color::Black );
   outer_bottom->setOutlineThickness( 1.0 );
   ConvexShape *outer_left = new ConvexShape(3);
   outer_left->setPoint( 0, Vector2f (text_x - 4, text_y + char_size + 3 ));
   outer_left->setPoint( 1, Vector2f (text_x - 4, text_y ));
   outer_left->setPoint( 2, Vector2f (text_x + (text_size.width / 2) , text_y + (char_size / 2) + 2 ));
   outer_left->setFillColor( Color( 135, 135, 135, 255 ) );
   outer_left->setOutlineColor( Color::Black );
   outer_left->setOutlineThickness( 1.0 );
   ConvexShape *outer_right = new ConvexShape(3);
   outer_right->setPoint( 0, Vector2f (text_x + text_size.width + 4, text_y + char_size + 3 ));
   outer_right->setPoint( 2, Vector2f (text_x + text_size.width + 4, text_y ));
   outer_right->setPoint( 1, Vector2f (text_x + (text_size.width / 2) , text_y + (char_size / 2) + 2 ));
   outer_right->setFillColor( Color( 215, 215, 215, 255 ) );
   outer_right->setOutlineColor( Color::Black );
   outer_right->setOutlineThickness( 1.0 );

   IMGuiManager::getSingleton().pushSprite( key_text, true );
   IMGuiManager::getSingleton().pushSprite( inner_rect, true );
   IMGuiManager::getSingleton().pushSprite( outer_top, true );
   IMGuiManager::getSingleton().pushSprite( outer_bottom, true );
   IMGuiManager::getSingleton().pushSprite( outer_left, true );
   IMGuiManager::getSingleton().pushSprite( outer_right, true );
}

void drawCount( int count, int x, int y, int size, bool plus, int char_size )
{
   Text *count_text = new Text();
   stringstream ss;
   ss << count;
   if (plus)
      ss << "+";
   count_text->setString( String(ss.str()) );
   count_text->setFont( *menu_font );
   count_text->setColor( Color::Black );
   count_text->setCharacterSize( char_size );
   FloatRect text_size = count_text->getGlobalBounds();
   float text_x = (size - text_size.width) + x,
         text_y = (size - char_size) + y;
   count_text->setPosition( text_x, text_y );

   RectangleShape *count_rect = new RectangleShape();
   count_rect->setSize( Vector2f((text_size.width + 2), (text_size.height + (char_size / 4))) );
   count_rect->setPosition( text_x - 1, text_y + 1 );
   count_rect->setFillColor( Color::White );
   count_rect->setOutlineColor( Color::Black );
   count_rect->setOutlineThickness( 1.0 );

   IMGuiManager::getSingleton().pushSprite( count_text, true );
   IMGuiManager::getSingleton().pushSprite( count_rect, true );
}

void drawCondition( Order_Conditional c, int x_b, int y_b, int size )
{
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();
   Sprite *sp_base = new Sprite(),
          *sp_cond = new Sprite();
   switch (c)
   {
      case ENEMY_ADJACENT:
      case ENEMY_AHEAD:
      case ENEMY_IN_RANGE:
      case ALLY_ADJACENT:
      case ALLY_AHEAD:
      case ALLY_IN_RANGE:
      case HEALTH_UNDER_50:
      case HEALTH_UNDER_20:
      case BLOCKED_AHEAD:
         sp_base->setTexture( *t_manager.getTexture( "ConditionalButtonBase.png" ) );
         break;
      case ENEMY_NOT_ADJACENT:
      case ENEMY_NOT_AHEAD:
      case ENEMY_NOT_IN_RANGE:
      case ALLY_NOT_ADJACENT:
      case ALLY_NOT_AHEAD:
      case ALLY_NOT_IN_RANGE:
      case HEALTH_OVER_50:
      case HEALTH_OVER_20:
      case NOT_BLOCKED_AHEAD:
         sp_base->setTexture( *t_manager.getTexture( "ConditionalSelectedRed.png" ) );
         break;
      default:
         delete sp_base;
         delete sp_cond;
         return;
   }
   switch (c)
   {
      case ENEMY_ADJACENT:
      case ENEMY_NOT_ADJACENT:
         sp_cond->setTexture( *t_manager.getTexture( "ConditionalEnemyAdjacent.png" ) );
         break;
      case ENEMY_AHEAD:
      case ENEMY_NOT_AHEAD:
         sp_cond->setTexture( *t_manager.getTexture( "ConditionalEnemyAhead.png" ) );
         break;
      case ENEMY_IN_RANGE:
      case ENEMY_NOT_IN_RANGE:
         sp_cond->setTexture( *t_manager.getTexture( "ConditionalEnemyInRange.png" ) );
         break;
      case ALLY_ADJACENT:
      case ALLY_NOT_ADJACENT:
         sp_cond->setTexture( *t_manager.getTexture( "ConditionalAllyAdjacent.png" ) );
         break;
      case ALLY_AHEAD:
      case ALLY_NOT_AHEAD:
         sp_cond->setTexture( *t_manager.getTexture( "ConditionalAllyAhead.png" ) );
         break;
      case ALLY_IN_RANGE:
      case ALLY_NOT_IN_RANGE:
         sp_cond->setTexture( *t_manager.getTexture( "ConditionalAllyInRange.png" ) );
         break;
      case HEALTH_OVER_50:
      case HEALTH_UNDER_50:
         sp_cond->setTexture( *t_manager.getTexture( "ConditionalHalfHealth.png" ) );
         break;
      case HEALTH_OVER_20:
      case HEALTH_UNDER_20:
         sp_cond->setTexture( *t_manager.getTexture( "Conditional20Health.png" ) );
         break;
      case BLOCKED_AHEAD:
      case NOT_BLOCKED_AHEAD:
         sp_cond->setTexture( *t_manager.getTexture( "ConditionalBlockedAhead.png" ) );
         break;
      default:
         delete sp_base;
         delete sp_cond;
         return;
   }

   // If size is 1.0, goes from -.1 to .4 x and .6 to 1.1 y
   int x = x_b - (size / 10), y = y_b + (size * 6 / 10);
   int s = size / 2;
   normalizeTo1x1( sp_base );
   sp_base->scale( s, s );
   normalizeTo1x1( sp_cond );
   sp_cond->scale( s, s );
   sp_base->setPosition( x, y );
   sp_cond->setPosition( x, y );
   IMGuiManager::getSingleton().pushSprite( sp_cond, true );
   IMGuiManager::getSingleton().pushSprite( sp_base, true );
}

void drawOrder( Order o, int x, int y, int size )
{
   // Base order
   Texture *base_tex = getOrderTexture( o );
   if (NULL == base_tex)
      return;

   Texture *button_tex = getOrderButtonTexture( o );
   if (NULL != button_tex) {
      // Requires a backing button

      Sprite sp_button( *button_tex );
      normalizeTo1x1( &sp_button );
      sp_button.scale( size, size );
      sp_button.setPosition( x, y );

      SFML_GlobalRenderWindow::get()->draw( sp_button );
   }

   Sprite sp_order( *base_tex );
   normalizeTo1x1( &sp_order );
   sp_order.scale( size, size );
   sp_order.setPosition( x, y );
   SFML_GlobalRenderWindow::get()->draw( sp_order );

   if (o.count != 1 && o.action <= PL_DELAY)
      drawCount( o.count, x, y, size );

   // Condition
   if (o.condition != TRUE && o.action < PL_ALERT_ALL)
      drawCondition( o.condition, x, y, size );
}

sum::KeybindTarget orderToKeybindTarget( Order o )
{
   switch(o.action)
   {
      case MOVE_FORWARD:
         return KB_BTN_MOVE_FORWARD;
      case MOVE_BACK:
         return KB_BTN_MOVE_BACK;
      case TURN_NORTH:
         return KB_BTN_TURN_NORTH;
      case TURN_EAST:
         return KB_BTN_TURN_EAST;
      case TURN_SOUTH:
         return KB_BTN_TURN_SOUTH;
      case TURN_WEST:
         return KB_BTN_TURN_WEST;
      case TURN_NEAREST_ENEMY:
         return KB_BTN_TURN_NEAREST_ENEMY;
      case FOLLOW_PATH:
         return KB_BTN_FOLLOW_PATH;
      case ATTACK_CLOSEST:
         return KB_BTN_ATTACK_CLOSEST;
      case ATTACK_FARTHEST:
         return KB_BTN_ATTACK_FARTHEST;
      case ATTACK_BIGGEST:
         return KB_BTN_ATTACK_BIGGEST;
      case ATTACK_SMALLEST:
         return KB_BTN_ATTACK_SMALLEST;
      case ATTACK_MOST_ARMORED:
         return KB_BTN_ATTACK_MOST_ARMORED;
      case ATTACK_LEAST_ARMORED:
         return KB_BTN_ATTACK_LEAST_ARMORED;
      case START_BLOCK:
         return KB_BTN_START_BLOCK;
      case END_BLOCK:
         return KB_BTN_END_BLOCK;
      case REPEAT:
         return KB_BTN_REPEAT;
      case WAIT:
         return KB_BTN_WAIT;
      case MONSTER_GUARD:
         return KB_BTN_MONSTER_GUARD;
      case MONSTER_BURST:
         return KB_BTN_MONSTER_BURST;
      case SOLDIER_SWITCH_AXE:
         return KB_BTN_SOLDIER_SWITCH_AXE;
      case SOLDIER_SWITCH_SPEAR:
         return KB_BTN_SOLDIER_SWITCH_SPEAR;
      case SOLDIER_SWITCH_BOW:
         return KB_BTN_SOLDIER_SWITCH_BOW;
      case WORM_HIDE:
         return KB_BTN_WORM_HIDE;
      case WORM_TRAIL_START:
         return KB_BTN_WORM_TRAIL_START;
      case WORM_TRAIL_END:
         return KB_BTN_WORM_TRAIL_END;
      case BIRD_CMD_SHOUT:
         return KB_BTN_BIRD_CMD_SHOUT;
      case BIRD_CMD_QUIET:
         return KB_BTN_BIRD_CMD_QUIET;
      case BIRD_FLY:
         return KB_BTN_BIRD_FLY;
      case BUG_CAST_FIREBALL:
         return KB_BTN_BUG_CAST_FIREBALL;
      case BUG_CAST_SHOCK:
         return KB_BTN_BUG_CAST_SHOCK;
      case BUG_CAST_DUST:
         return KB_BTN_BUG_CAST_DUST;
      case BUG_OPEN_WORMHOLE:
         return KB_BTN_BUG_OPEN_WORMHOLE;
      case BUG_CLOSE_WORMHOLE:
         return KB_BTN_BUG_CLOSE_WORMHOLE;
      case BUG_MEDITATE:
         return KB_BTN_BUG_MEDITATE;
      case PL_ALERT_ALL:
         return KB_BTN_PL_ALERT_ALL;
      case PL_ALERT_MONSTERS:
         return KB_BTN_PL_ALERT_MONSTERS;
      case PL_ALERT_SOLDIERS:
         return KB_BTN_PL_ALERT_SOLDIERS;
      case PL_ALERT_WORMS:
         return KB_BTN_PL_ALERT_WORMS;
      case PL_ALERT_BIRDS:
         return KB_BTN_PL_ALERT_BIRDS;
      case PL_ALERT_BUGS:
         return KB_BTN_PL_ALERT_BUGS;
      case PL_CMD_GO:
         return KB_BTN_PL_GO;
      case PL_CMD_GO_ALL:
         return KB_BTN_PL_GO_ALL;
      case PL_CMD_GO_MONSTERS:
         return KB_BTN_PL_GO_MONSTERS;
      case PL_CMD_GO_SOLDIERS:
         return KB_BTN_PL_GO_SOLDIERS;
      case PL_CMD_GO_WORMS:
         return KB_BTN_PL_GO_WORMS;
      case PL_CMD_GO_BIRDS:
         return KB_BTN_PL_GO_BIRDS;
      case PL_CMD_GO_BUGS:
         return KB_BTN_PL_GO_BUGS;
      case PL_SET_GROUP:
         return KB_BTN_PL_SET_GROUP;
      case PL_DELAY:
         return KB_BTN_PL_DELAY;
      default:
         return KB_NOTHING;
   }
}

};
