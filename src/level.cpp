#include "shutdown.h"
#include "level.h"
#include "orders.h"
#include "types.h"
#include "units.h"
#include "util.h"
#include "log.h"
#include "menustate.h"
#include "config.h"
#include "clock.h"

#include "pugixml.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include "SFML_GlobalRenderWindow.hpp"
#include "SFML_WindowEventManager.hpp"
#include "SFML_TextureManager.hpp"
#include "IMGuiManager.hpp"
#include "IMCursorManager.hpp"
#include "IMImageButton.hpp"
#include "IMTextButton.hpp"
#include "IMEdgeButton.hpp"
#include "IMEdgeTextButton.hpp"

#include "stdio.h"
#include "math.h"
#include <sstream>
#include <fstream>
#include <list>
#include <set>

using namespace sf;
using namespace std;

namespace sum {

#define GRID_AT(GRID,X,Y) (GRID[((X) + ((Y) * level_dim_x))])

//////////////////////////////////////////////////////////////////////
// Global app-state variables ---

View *level_view;
float view_rel_x_to_y;

int turn, turn_progress;
int pause_state = 0;

//////////////////////////////////////////////////////////////////////
// Constants ---

const float c_min_zoom_x = 10;
const int c_fully_paused = 15;
const int c_turn_length = 1000;
const int c_mouse_down_select_time = 150;
const int c_max_dimension = 253;
const int c_wormhole_locations = 12;

//////////////////////////////////////////////////////////////////////
// Level Data ---

int level_init = 0;

bool between_turns = false;

int level_num = -1;
int level_dim_x = -1;
int level_dim_y = -1;
float level_fog_dim = 0;

Sprite **terrain_sprites;
Sprite **terrain_mod_sprites;

BaseTerrain base_terrain;
Sprite *base_grass_sprite;
Sprite *base_mountain_sprite;
Sprite *base_underground_sprite;
// Grids - row-major order, size = dim_x X dim_y
Terrain* terrain_grid;
TerrainMod* terrain_mod_grid;
Unit** unit_grid;
// Lists
list<Unit*> unit_list;
list<Effect*> effect_list;

// Vision
Vision* vision_grid;
bool vision_enabled;
Vision* ai_vision_grid;

list<Unit*> listening_units;
Unit *player = NULL;

Unit *selected_unit;

int num_wormholes;
Vector2i* wormhole_v;

// Debugging
bool show_framerate;

//////////////////////////////////////////////////////////////////////
// UI Data ---

int left_mouse_down = 0;
int left_mouse_down_time = 0;

int right_mouse_down = 0;
int right_mouse_down_time = 0;

int key_shift_down = 0;

// Framerate calculation
#define MAXSAMPLES 100
int tickindex = 0;
int ticksum = 0;
int ticklist[MAXSAMPLES];

void clearFramerate()
{
   for (int i = 0; i < MAXSAMPLES; ++i)
      ticklist[i] = 0;

   ticksum = 0;
}

void calculateFramerate(int newtick)
{
   ticksum -= ticklist[tickindex];  /* subtract value falling off */
   ticksum += newtick;              /* add new value */
   ticklist[tickindex] = newtick;   /* save new value so it can be subtracted later */
   if(++tickindex == MAXSAMPLES)    /* inc buffer index */
      tickindex = 0;
}

float getFramerate()
{
   return 1000.0*(float)MAXSAMPLES/(float)ticksum;
}

int tickcount = 0;
int tickfps = 0;

//////////////////////////////////////////////////////////////////////
// Utility ---

Vector2f coordsWindowToView( int window_x, int window_y )
{
   RenderWindow *r_window = SFML_GlobalRenderWindow::get();
   float view_x = (((float)window_x / (float)(r_window->getSize().x))* (float)(level_view->getSize().x)) 
                + ((level_view->getCenter().x) - (level_view->getSize().x / 2.0));
   float view_y = (((float)window_y / (float)(r_window->getSize().y))* (float)(level_view->getSize().y)) 
                + ((level_view->getCenter().y) - (level_view->getSize().y / 2.0));

   return Vector2f( view_x, view_y );
}

Vector2i coordsWindowToLevel( int window_x, int window_y )
{
   Vector2f view_coords = coordsWindowToView( window_x, window_y );

   int x = (view_coords.x < 0)?((int)view_coords.x - 1):(int)view_coords.x,
       y = (view_coords.y < 0)?((int)view_coords.y - 1):(int)view_coords.y;
   return Vector2i(x, y);
}

Vector2u coordsViewToWindow( float view_x, float view_y )
{
   RenderWindow *r_window = SFML_GlobalRenderWindow::get();
   int window_x = (int)(((view_x - ((level_view->getCenter().x) - (level_view->getSize().x / 2.0)))
                         / (level_view->getSize().x)) * (r_window->getSize().x));
   int window_y = (int)(((view_y - ((level_view->getCenter().y) - (level_view->getSize().y / 2.0)))
                         / (level_view->getSize().y)) * (r_window->getSize().y));
   return Vector2u( window_x, window_y );
}

void drawSquare( int x, int y, int size, Color line_color )
{
   RectangleShape rect;
   rect.setSize( Vector2f( size, size ) );
   rect.setPosition( x, y );
   rect.setFillColor( Color::Transparent );
   rect.setOutlineColor( line_color );
   rect.setOutlineThickness( 1.0 );
   SFML_GlobalRenderWindow::get()->draw( rect );
}

//////////////////////////////////////////////////////////////////////
// View management ---

int setView( float x_size, Vector2f center )
{
   float y_size = x_size * view_rel_x_to_y;

   float x_add = 0, y_add = 0;
   float x_base = center.x - (x_size / 2),
         y_base = center.y - (y_size / 2),
         x_top = center.x + (x_size / 2),
         y_top = center.y + (y_size / 2);

   // Don't need to check for: top AND bottom overlap -> failure
   if (x_base < -level_fog_dim)
      x_add = -(x_base + level_fog_dim);

   if (x_top > (level_dim_x + level_fog_dim))
      x_add = (level_dim_x + level_fog_dim) - x_top;

   if (y_base < -level_fog_dim)
      y_add = -(y_base + level_fog_dim);

   if (y_top > (level_dim_y + level_fog_dim))
      y_add = (level_dim_y + level_fog_dim) - y_top;

   center.x += x_add;
   center.y += y_add;

   level_fog_dim = (y_size / 4);
   level_view->setSize( x_size, y_size );
   level_view->setCenter( center.x, center.y );

   return 0;
}

// Shifts in terms of view coords
int shiftView( float x_shift, float y_shift )
{
   Vector2f old_center = level_view->getCenter(),
            old_size = level_view->getSize();
   Vector2f new_center (old_center.x + x_shift, old_center.y + y_shift);

   return setView( old_size.x, new_center );
}

// Positive is zoom in negative is zoom out
int zoomView( int ticks, Vector2f zoom_around )
{
   float zoom_val = pow(1.5, -ticks);
   Vector2f old_center = level_view->getCenter(),
            old_size = level_view->getSize();
   Vector2f new_center ((old_center.x + zoom_around.x) / 2.0, (old_center.y + zoom_around.y) / 2.0);
   Vector2f new_size (old_size*zoom_val);

   // Fit map
   if (new_size.x > (level_dim_x + (2*level_fog_dim))) {
      float x_fit = (level_dim_x + (2*level_fog_dim)) / new_size.x;
      new_size *= x_fit;
   }
   if (new_size.y > (level_dim_y + (2*level_fog_dim))) {
      float y_fit = (level_dim_y + (2*level_fog_dim)) / new_size.y;
      new_size *= y_fit;
   }

   // Zoom limits
   if (new_size.x < c_min_zoom_x) {
      float min_fit = c_min_zoom_x / new_size.x;
      new_size *= min_fit;
   }

   return setView( new_size.x, new_center );
}

//////////////////////////////////////////////////////////////////////
// Winning ---

bool victory = false;

set<Unit*> win_if_killed;
Vector2i win_if_reached;

void clearWinConditions()
{
   victory = false;

   win_if_killed.clear();
   win_if_reached = Vector2i( -1, -1 );
}

void addUnitWinCondition( Unit *u )
{
   if (NULL == u) return;

   win_if_killed.insert( u );
   u->win_condition = true;
}

void removeUnitWinCondition( Unit *u )
{
   if (NULL == u) return;

   set<Unit*>::iterator it = find( win_if_killed.begin(), win_if_killed.end(), u );
   if (it != win_if_killed.end()) {
      win_if_killed.erase( it );
      u->win_condition = false;
   }
}

void addLocationWinCondition( int x, int y )
{
   if (x < 0 || x > c_max_dimension || y < 0 || y > c_max_dimension)
      win_if_reached = Vector2i( -1, -1 );
   else
      win_if_reached = Vector2i( x, y );
}

bool checkWinConditions()
{
   for (set<Unit*>::iterator it=win_if_killed.begin(); it != win_if_killed.end(); ++it) {
      list<Unit*>::iterator it2=find( unit_list.begin(), unit_list.end(), (*it) );
      if (it2 != unit_list.end()) // A target is still alive
         return false;
   }

   // If we reach here, all targets are dead
   if (win_if_reached.x == -1 || win_if_reached.y == -1) 
      return true; // No location required

   Unit *u = GRID_AT(unit_grid,win_if_reached.x,win_if_reached.y);
   if (u && u->team == 0)
      return true; // Location reached

   return false;
}

//////////////////////////////////////////////////////////////////////
// Adding stuff ---

int removeUnit (list<Unit*>::iterator it)
{
   Unit *u = (*it);
   if (u) {
      unit_list.erase( it );

      if (GRID_AT(unit_grid, u->x_grid, u->y_grid) == u)
         GRID_AT(unit_grid, u->x_grid, u->y_grid) = NULL;

      return 0;
   }
   return -1;
}

int addPlayer()
{
   log( "Added Player" );
   if (player) {
      int cx = player->x_grid, cy = player->y_grid;
      if (GRID_AT(unit_grid, cx, cy) != NULL) {
         log("Can't add a unit where one already is");
         return -2;
      }

      // Otherwise, we should be good
      GRID_AT(unit_grid, cx, cy) = player;

      return 0;
   }
   return -1;
}

int addUnit( Unit *u )
{
   if (u) {
      int cx = u->x_grid, cy = u->y_grid;
      if (GRID_AT(unit_grid, cx, cy) != NULL) {
         log("Can't add a unit where one already is");
         return -2;
      }

      // Otherwise, we should be good
      GRID_AT(unit_grid, cx, cy) = u;
      unit_list.push_front( u );

      return 0;
   }
   return -1;
}

int moveUnit( Unit *u, int new_x, int new_y )
{
   if (u && between_turns && canMove( u->x_grid, u->y_grid, new_x, new_y)) {

      if (GRID_AT(unit_grid, u->x_grid, u->y_grid) == u)
         GRID_AT(unit_grid, u->x_grid, u->y_grid) = NULL;

      GRID_AT(unit_grid, new_x, new_y) = u;
      return 0;
   }
   return -1;
}

int removeEffect( Effect *p )
{
   if (p) {
      list<Effect*>::iterator it= find( effect_list.begin(), effect_list.end(), p );
      effect_list.erase( it );
      return 0;
   }
   return -1;
}

int addProjectile( Effect_Type t, int team, float x, float y, float speed, float range, Unit* target, float homing, float fastforward )
{
   Effect *p = genProjectile( t, team, x, y, speed, range, target, homing, fastforward );

   return addEffectManual( p );
}

int addEffect( Effect_Type t, float dur, float x, float y, float rot, float fade )
{
   Effect *e = genEffect( t, dur, x, y, rot, fade );

   return addEffectManual( e );
}

int addEffectManual( Effect *e )
{
   if (e) {
      effect_list.push_back(e);
      return 0;
   }
   return -1;
}

int addWormhole( int x, int y, int &number, bool open )
{
   if (number == -1) {
      if (num_wormholes <= 5) {
         number = num_wormholes + 1;
         num_wormholes++;
      } else {
         return -1;
      }
   }
   if (number > 6)
      return -1;

   unsigned int index = ((number - 1) * 2) + (open?0:1);
   if (index < c_wormhole_locations) {

      // if there's one already, clear it
      Vector2i &loc = wormhole_v[index];
      if (loc.x >= 0 && loc.x < level_dim_x && loc.y >= 0 && loc.y < level_dim_y)
         GRID_AT(terrain_mod_grid,loc.x,loc.y) = TM_NONE;

      // Change stuff
      GRID_AT(terrain_mod_grid,x,y) = (TerrainMod) (TM_WH_OPEN_1 + index);
      loc.x = x;
      loc.y = y;

      // Check surrounding ter_mods for incoming paths, and cap them

      // TODO


   
      return 0;
   }
   else return -1;
}

//////////////////////////////////////////////////////////////////////
// Vision ---

bool isVisible( int x, int y )
{
   return !vision_enabled || (GRID_AT(vision_grid,x,y) == VIS_VISIBLE);
}

bool blocksVision( int x, int y, int from_x, int from_y, Direction ew, Direction ns, int &flags )
{
   Terrain t = GRID_AT(terrain_grid,x,y);

   // Always always block vision
   if (t >= TER_EDGE_CLIFF_N && t <= TER_EDGE_CLIFF_CORNER_NE_270)
      return true;

   // If flying, and not forest, then doesn't block vision
   if ( (flags & FLAG_VIS_FLYING) && !(t >= TER_TREE_1 && t <= TER_TREE_LAST) )
      return false;

   // If treesight, and is forest, then pierces it
   if ( (flags & FLAG_VIS_TREESIGHT_MASK) && t >= TER_TREE_1 && t <= TER_TREE_LAST) {
      flags--;
      return false;
   }
   
   // Otherwise continue as normal

   // Things that block vision
   if (t >= TER_ROCK_1 && t <= TER_TREE_LAST)
      return true;

   // Cliffs
   if ((t == TER_CLIFF_S || t == TER_CLIFF_S_E_EDGE || t == TER_CLIFF_S_W_EDGE)
      && ns == NORTH && (y < from_y))
      return true;
   if ((t == TER_CLIFF_N || t == TER_CLIFF_N_E_EDGE || t == TER_CLIFF_N_W_EDGE)
      && ns == SOUTH && (y > from_y))
      return true;
   if ((t == TER_CLIFF_E || t == TER_CLIFF_E_N_EDGE || t == TER_CLIFF_E_S_EDGE)
      && ew == WEST && (x < from_x))
      return true;
   if ((t == TER_CLIFF_W || t == TER_CLIFF_W_S_EDGE || t == TER_CLIFF_W_N_EDGE)
      && ew == EAST && (x > from_x))
      return true;
   // Cliff corners
   if ((t == TER_CLIFF_CORNER_SE_90 || t == TER_CLIFF_CORNER_SE_270)
         && ns != SOUTH && ew != EAST)
      return true;
   if ((t == TER_CLIFF_CORNER_SW_90 || t == TER_CLIFF_CORNER_SW_270)
         && ns != SOUTH && ew != WEST)
      return true;
   if ((t == TER_CLIFF_CORNER_NW_90 || t == TER_CLIFF_CORNER_NW_270)
         && ns != NORTH && ew != WEST)
      return true;
   if ((t == TER_CLIFF_CORNER_NE_90 || t == TER_CLIFF_CORNER_NE_270)
         && ns != NORTH && ew != EAST)
      return true;

   return false;
}

int calculatePointVision( int start_x, int start_y, int end_x, int end_y, int flags, Vision *v_grid )
{
   int previous_x = start_x, previous_y = start_y;

   float dydx;
   if (start_x == end_x)
      dydx = 100000000;
   else
      dydx = ((float)(start_y - end_y)) / ((float)(start_x - end_x));

   int x, y, dx = 1, dy = 1;
   if (end_x < start_x) dx = -1;
   if (end_y < start_y) dy = -1;
   float calculator = 0;

   Direction dir1 = EAST, dir2 = SOUTH;
   if (dx == -1) dir1 = WEST;
   if (dy == -1) dir2 = NORTH;
   
   if (dydx >= 1 || dydx <= -1) { 
      // steep slope - go up/down 1 each move, calculate x
      float f_dx = ((float)dy)/dydx; 
      x = start_x;
      for (y = start_y; y != end_y; y += dy) {
         calculator += f_dx;
         if (calculator >= 1.05) {
            x += dx;
            calculator -= 1.0;
         }
         else if (calculator <= -1.05) {
            x += dx;
            calculator += 1.0;
         }

         GRID_AT(v_grid,x,y) = VIS_VISIBLE;
         // Check if this square obstructs vision
         if (y != start_y && blocksVision(x, y, previous_x, previous_y, dir1, dir2, flags))
            return -1; // Vision obstructed beyond here

         // Otherwise, it's visible, move on
         previous_x = x;
         previous_y = y;
      }
   }
   else
   {
      // shallow slope, go left/right 1 each move, calculate y
      float f_dy = dydx*((float)dx);
      y = start_y;
      for (x = start_x; x != end_x; x += dx) {
         calculator += f_dy;
         if (calculator >= 1.05) {
            y += dy;
            calculator -= 1.0;
         }
         else if (calculator <= -1.05) {
            y += dy;
            calculator += 1.0;
         }

         GRID_AT(v_grid,x,y) = VIS_VISIBLE;
         // Check if this square obstructs vision
         if (x != start_x && blocksVision(x, y, previous_x, previous_y, dir1, dir2, flags))
            return -1; // Vision obstructed beyond here

         // Otherwise, it's visible, move on
         previous_x = x;
         previous_y = y;
      }
   }
   GRID_AT(v_grid,end_x,end_y) = VIS_VISIBLE;
   return 0;
}

int calculateVerticalLineVision( int x, int start_y, int end_y, int flags, Vision *v_grid )
{
   int dy = 1;
   if (start_y > end_y) dy = -1;

   Direction dir = SOUTH; 
   if (dy == -1) dir = NORTH;

   for ( int y = start_y + dy; y != end_y + dy; y += dy) {
      GRID_AT(v_grid,x,y) = VIS_VISIBLE;
      // Check possible vision-obstructors
      if (blocksVision(x, y, x, y - dy, ALL_DIR, dir, flags))
         return -1; // Vision obstructed beyond here

      // Otherwise, it's visible, move on
   }
   return 0;
}

int calculateHorizintalLineVision( int start_x, int end_x, int y, int flags, Vision *v_grid )
{
   int dx = 1;
   if (start_x > end_x) dx = -1;

   Direction dir = EAST;
   if (dx == -1) dir = WEST;

   for ( int x = start_x + dx; x != end_x + dx; x += dx) {
      GRID_AT(v_grid,x,y) = VIS_VISIBLE;
      // Check possible vision-obstructors
      if (blocksVision(x, y, x - dx, y, dir, ALL_DIR, flags))
         return -1; // Vision obstructed beyond here

      // Otherwise, it's visible, move on
   }
   return 0;
}

int calculateLineVision( int start_x, int start_y, int end_x, int end_y, float range_squared, int flags, Vision *v_grid )
{
   // First things first, if it's out of range it's not visible
   int dx = (end_x - start_x), dy = (end_y - start_y);
   if (range_squared < ((dx*dx)+(dy*dy)))
      return -2;

   if (start_x == end_x)
      return calculateVerticalLineVision( start_x, start_y, end_y, flags, v_grid );
   if (start_y == end_y)
      return calculateHorizintalLineVision( start_x, end_x, start_y, flags, v_grid );
   
   int ret = calculatePointVision( start_x, start_y, end_x, end_y, flags, v_grid ); 
   if (ret == 0) // We must have seen it to get this result
      GRID_AT(v_grid,end_x,end_y) = VIS_VISIBLE;

   return ret;
}

int calculateUnitVision( Unit *unit, bool ai=false )
{
   int x, y;
   if (unit) {
      
      Vision *v_grid = vision_grid;
      if (unit->team != 0 || ai)
         v_grid = ai_vision_grid;

      int flags = 0x1; // Give units treesight 1
      if (unit->flying == true)
         flags |= FLAG_VIS_FLYING;

      float vision_range_squared = unit->vision_range * unit->vision_range;

      // Strategy: go around in boxes, skipping already visible squares,
      // and from each square make a line back to the middle,
      // and determine visibility along that line for all squares in the line
      // GOAL: be generous in what is visible, to avoid intuitive problems
      GRID_AT(v_grid,unit->x_grid,unit->y_grid) = VIS_VISIBLE;

      for (int radius = unit->vision_range; radius > 0; --radius)
      {
         if (unit->y_grid - radius >= 0)
            for (x = unit->x_grid - radius; x <= unit->x_grid + radius; ++x) {
               if (x < 0 || x >= level_dim_x) continue;
               if (GRID_AT(v_grid, x, unit->y_grid - radius) == VIS_VISIBLE) continue;
               calculateLineVision( unit->x_grid, unit->y_grid, x, (unit->y_grid - radius), vision_range_squared, flags, v_grid );
            }

         if (unit->y_grid + radius < level_dim_y)
            for (x = unit->x_grid - radius; x <= unit->x_grid + radius; ++x) {
               if (x < 0 || x >= level_dim_x) continue;
               if (GRID_AT(v_grid, x, unit->y_grid + radius) == VIS_VISIBLE) continue;
               calculateLineVision( unit->x_grid, unit->y_grid, x, (unit->y_grid + radius), vision_range_squared, flags, v_grid );
            }

         if (unit->x_grid - radius >= 0)
            for (y = unit->y_grid - radius; y <= unit->y_grid + radius; ++y) {
               if (y < 0 || y >= level_dim_y) continue;
               if (GRID_AT(v_grid, unit->x_grid - radius, y) == VIS_VISIBLE) continue;
               calculateLineVision( unit->x_grid, unit->y_grid, (unit->x_grid - radius), y, vision_range_squared, flags, v_grid );
            }

         if (unit->x_grid + radius < level_dim_x)
            for (y = unit->y_grid - radius; y <= unit->y_grid + radius; ++y) {
               if (y < 0 || y >= level_dim_y) continue;
               if (GRID_AT(v_grid, unit->x_grid + radius, y) == VIS_VISIBLE) continue;
               calculateLineVision( unit->x_grid, unit->y_grid, (unit->x_grid + radius), y, vision_range_squared, flags, v_grid );
            }

      }
   }

   return 0;
}

int calculateVision()
{
   int x, y;
   for (x = 0; x < level_dim_x; ++x) {
      for (y = 0; y < level_dim_y; ++y) {
         if (GRID_AT(vision_grid,x,y) >= VIS_SEEN_BEFORE)
            GRID_AT(vision_grid,x,y) = VIS_SEEN_BEFORE;
         else
            GRID_AT(vision_grid,x,y) = VIS_NEVER_SEEN;
      }
   }

   for (x = 0; x < level_dim_x; ++x) {
      for (y = 0; y < level_dim_y; ++y) {
         if (GRID_AT(terrain_grid,x,y) >= TER_ATELIER) {
            // Give vision in 1 radius
            for (int i = x - 1; i <= x + 1 && i <= level_dim_x; ++i) {
               if (i < 0) continue;
               for (int j = y - 1; j <= y + 1 && j <= level_dim_y; ++j) {
                  if (j < 0) continue;
                  GRID_AT(vision_grid,i,j) = VIS_VISIBLE;
               }
            }
         }
      }
   }
   
   if (player)
      calculateUnitVision( player );
 
   for (list<Unit*>::iterator it=unit_list.begin(); it != unit_list.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit->team == 0) // Allied unit
         calculateUnitVision( unit );
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////
// Targetting ---

int selectUnit( Vector2f coords )
{
   int cx = (int)coords.x,
       cy = (int)coords.y;

   std::stringstream ls;
   ls << "Selecting a unit at x=" << cx << ", y=" << cy;

   if (GRID_AT(vision_grid,cx,cy) != VIS_VISIBLE) {
      ls << " - coordinates not visible";
      log(ls.str());
      selected_unit = NULL;
      return -2;
   }

   Unit *u = GRID_AT(unit_grid, (int)coords.x, (int)coords.y);

   if (u) {
      ls << " - Selected: " << u->descriptor();
      log(ls.str());
      selected_unit = u;
      return 0;
   }

   ls << " - None found";
   log(ls.str());

   selected_unit = NULL;
   return -1;
}

Unit* enemySelector( int selector, Unit* u_cur, Unit* u_new, float cur_r_squared, float new_r_squared )
{
   // Compare based on selector
   if (NULL == u_cur) {
      return u_new;
   } else if (selector == SELECT_CLOSEST) {
      if (new_r_squared < cur_r_squared)
         return u_new;
      else
         return u_cur;
   } else if (selector == SELECT_FARTHEST) {
      if (new_r_squared > cur_r_squared)
         return u_new;
      else
         return u_cur;
   } else if (selector == SELECT_SMALLEST) {
      if (u_new->health < u_cur->health)
         return u_new;
      else
         return u_cur;
   } else if (selector == SELECT_BIGGEST) {
      if (u_new->health > u_cur->health)
         return u_new;
      else
         return u_cur;
   } else if (selector == SELECT_MOST_ARMORED) {
      if (u_new->armor > u_cur->armor)
         return u_new;
      else
         return u_cur;
   } else if (selector == SELECT_LEAST_ARMORED) {
      if (u_new->armor < u_cur->armor)
         return u_new;
      else
         return u_cur;
   }

   return NULL;
}

Unit* getEnemyBox( int x, int y, int min_x, int max_x, int min_y, int max_y, float range, Unit *source, int selector, bool ally )
{
   int team;
   if (source == NULL) team = -1;
   else {
      team = source->team;
      calculateUnitVision( source, true );
   }

   if (min_x < 0) min_x = 0;
   if (max_x >= level_dim_x) max_x = level_dim_x - 1;
   if (min_y < 0) min_y = 0;
   if (max_y >= level_dim_y) max_y = level_dim_y - 1;

   float range_squared = range * range;

   Unit *result = NULL;
   float result_r_squared = 0;
   for (int j = min_y; j <= max_y; ++j) {
      for (int i = min_x; i <= max_x; ++i) {
         Unit *u = GRID_AT(unit_grid,i,j);
         if (u && (u->alive == 1) && (u->invis == false || u->team == team) &&
               ((ally == false && (u->team != team)) || (ally == true && (u->team == team)))) {
            // Can I see it?
            if ((team == -1) || (GRID_AT(ai_vision_grid,i,j) == VIS_VISIBLE)) {
               float u_x = u->x_grid - x, u_y = u->y_grid - y;
               float u_squared = (u_x * u_x) + (u_y * u_y);
               // Is it really in range? And not invisible?
               if (u_squared <= range_squared) {
                  // Compare based on selector
                  if (result != enemySelector( selector, result, u, result_r_squared, u_squared )) {
                     result = u;
                     result_r_squared = u_squared;
                  }
               }
            }
         }
      }
   }

   return result;
}

Unit* getEnemy( int x, int y, float range, Direction dir, Unit *source, int selector, bool ally )
{
   // Set up search box
   int int_range = (int) range + 1;
   int min_x, max_x, min_y, max_y;
   min_x = x - int_range;
   max_x = x + int_range;
   min_y = y - int_range;
   max_y = y + int_range;
   switch (dir) {
      case NORTH:
         max_y = y;
         break;
      case SOUTH:
         min_y = y;
         break;
      case WEST:
         max_x = x;
         break;
      case EAST:
         min_x = x;
         break;
      default:
         break;
   }

   return getEnemyBox( x, y, min_x, max_x, min_y, max_y, range, source, selector, ally );
}

Unit* getEnemyLine( int x, int y, float range, Direction dir, Unit *source, int selector, bool ally )
{
   // Split it up
   if (dir == NORTH)
      return getEnemyBox( x, y, x, x, y - 1, y - (int)range, range, source, selector, ally );
   else if (dir == SOUTH)
      return getEnemyBox( x, y, x, x, y + 1, y + (int)range, range, source, selector, ally );
   else if (dir == WEST)
      return getEnemyBox( x, y, x - 1, x - (int)range, y, y, range, source, selector, ally );
   else if (dir == EAST)
      return getEnemyBox( x, y, x + 1, x + (int)range, y, y, range, source, selector, ally );

   return NULL;
}

Unit* getEnemyAdjacent( int x, int y, Unit *source, int selector, bool ally )
{
   int team;
   if (source == NULL) team = -1;
   else {
      team = source->team;
   }

   Unit *result = NULL, *u = NULL;
   int x0, y0;
   for (int d = (int)NORTH; d < (int)ALL_DIR; ++d) {
      x0 = x;
      y0 = y;
      if (addDirection( (Direction)d, x0, y0 ) == 0) {
         u = GRID_AT(unit_grid,x0,y0);
         if (u && (u->alive == 1) &&
               ((ally == false && (u->team != team)) || (ally == true && (u->team == team)))) {
            if (result != enemySelector( selector, result, u, 1, 1 )) {
               result = u;
            }
         }
      }
   }

   return result;
}

//////////////////////////////////////////////////////////////////////
// Movement interface ---

bool canMove( int x, int y, int from_x, int from_y )
{
   if (x < 0 || y < 0 || x >= level_dim_x || y >= level_dim_y)
      return false;

   Terrain t = GRID_AT(terrain_grid,x,y);
   if ( (t >= TER_CLIFF_S && t <= TER_CLIFF_CORNER_NW_270)
     || (t >= TER_ROCK_1 && t <= TER_ROCK_LAST)
     || (t >= TER_EDGE_CLIFF_S && t <= TER_EDGE_CLIFF_CORNER_NE_270))
      return false; // cliffs impassable

   return true;
}

set<Unit*> unit_collision_set;

Unit* unitIncoming( int to_x, int to_y, int from_x, int from_y )
{
   if (to_x < 0 || to_x >= level_dim_x || to_y < 0 || to_y >= level_dim_y
         || from_x < 0 || from_x >= level_dim_x || from_y < 0 || from_y >= level_dim_y)
      return NULL;

   Unit *u = GRID_AT(unit_grid,from_x,from_y);
   if (NULL != u && u->active == 1)
   {
      Order o = u->this_turn_order;
      if ((o.action == MOVE_FORWARD || o.action == MOVE_BACK || o.action == FOLLOW_PATH)
            && (u->x_next == to_x && u->y_next == to_y))
         return u;
   }
   return NULL;
}

bool canMoveUnit( int x, int y, int from_x, int from_y, Unit *u )
{
   // Ignores terrain/buildings - do that stuff in canMove
   //
   // This only figures out:
   //
   // 1) Is there a unit in the square you're going to?
   // 1.5) If so, will it move out in another direction?
   // 
   // 2) Are there any other units planning to move into this square?
   // 2.5) If so, are they bigger than you?

   // 1)
   Unit *u_next = GRID_AT(unit_grid,x,y);
   if (NULL != u_next) {
      // Check for loops
      set<Unit*>::iterator it = unit_collision_set.find( u_next );
      if (it != unit_collision_set.end()) { // found it
         // We have a collision loop. Nobody's moving today.
         log("Collision loop");
         return false;
      }
         
      // There's a unit. Let's see if it's moving?
      if (u_next->active != 1) // He aint moving
         return false;

      Order o = u_next->this_turn_order;
      if (o.action == MOVE_FORWARD || o.action == MOVE_BACK || o.action == FOLLOW_PATH) {
         // Is he moving to THIS square?
         if (u_next->x_next == x && u_next->y_next == y) {
            // Well fuck it, we're both gonna bump
            return false;
         }
         // Move bitch get out the WAAAAY
         
         // Oh wait what if he can't move b/c of other units?
         unit_collision_set.insert( u_next );
         bool r = testUnitCanMove( u_next );
         unit_collision_set.erase( u_next );

         if (r == false) // He aint moving
            return false;

      }
      else
      {
         // He aint moving
         return false;
      }
   }
   
   // Still here? Then this location is available to move into,
   // but you might not be the only one trying

   Unit *u_biggest = u;

   u_next = unitIncoming( x, y, x - 1, y );
   if (u_next && u_next != u) {
      if (u_biggest == NULL || u_next->max_health > u_biggest->max_health) {
         u_biggest = u_next;
      } else {
         u_next->this_turn_order = Order(BUMP);
         if (u_next->max_health == u_biggest->max_health)
            u_biggest->this_turn_order = Order(BUMP);
      }
   }

   u_next = unitIncoming( x, y, x + 1, y );
   if (u_next && u_next != u) {
      if (u_biggest == NULL || u_next->max_health > u_biggest->max_health) {
         u_biggest = u_next;
      } else {
         u_next->this_turn_order = Order(BUMP);
         if (u_next->max_health == u_biggest->max_health)
            u_biggest->this_turn_order = Order(BUMP);
      }
   }

   u_next = unitIncoming( x, y, x, y - 1 );
   if (u_next && u_next != u) {
      if (u_biggest == NULL || u_next->max_health > u_biggest->max_health) {
         u_biggest = u_next;
      } else {
         u_next->this_turn_order = Order(BUMP);
         if (u_next->max_health == u_biggest->max_health)
            u_biggest->this_turn_order = Order(BUMP);
      }
   }

   u_next = unitIncoming( x, y, x, y + 1 );
   if (u_next && u_next != u) {
      if (u_biggest == NULL || u_next->max_health > u_biggest->max_health) {
         u_biggest = u_next;
      } else {
         u_next->this_turn_order = Order(BUMP);
         if (u_next->max_health == u_biggest->max_health)
            u_biggest->this_turn_order = Order(BUMP);
      }
   }

   if (u_biggest != NULL && u_biggest != u)
      return false;

   return true;
}

//////////////////////////////////////////////////////////////////////
// Player interface ---

// Choosing player orders

int order_prep_count = 0;
Order_Conditional order_prep_cond = TRUE;

#define ORDER_PREP_LAST_COUNT 0x1
#define ORDER_PREP_LAST_COND 0x2
#define ORDER_PREP_LAST_ORDER 0x4
#define ORDER_PREP_MAX_COUNT 10000
int order_prep_last_input = 0;

int playerAddCount( int digit )
{
   if (order_prep_last_input == ORDER_PREP_LAST_COUNT) {
      // Add another digit to the count
      order_prep_count = ((order_prep_count * 10) + digit) % ORDER_PREP_MAX_COUNT;
   }
   else
      order_prep_count = digit;

   order_prep_last_input = ORDER_PREP_LAST_COUNT;
   return 0;
}
      
int playerSetCount( int c )
{
   order_prep_count = c;
   order_prep_last_input = 0;

   return 0;
}

int selectConditionButton( Order_Conditional c );

int playerAddConditional( Order_Conditional c )
{
   switch (c)
   {
      case TRUE:
         order_prep_cond = TRUE;
         break;
      case ENEMY_ADJACENT:
      case ENEMY_AHEAD:
      case ENEMY_IN_RANGE:
      case ALLY_ADJACENT:
      case ALLY_AHEAD:
      case ALLY_IN_RANGE:
      case HEALTH_UNDER_50:
      case HEALTH_UNDER_20:
      case BLOCKED_AHEAD:
         if (order_prep_cond == c) order_prep_cond = (Order_Conditional)((int)c + 1);
         else if (order_prep_cond == (Order_Conditional)((int)c + 1)) order_prep_cond = TRUE;
         else order_prep_cond = c;
         break;
      default:
         order_prep_cond = c;
   }

   selectConditionButton( order_prep_cond );
   order_prep_last_input = ORDER_PREP_LAST_COND;
   return 0;
}

int playerAddOrder( Order_Action a )
{
   Order o;
   o.initOrder( a, order_prep_cond, order_prep_count );

   player->addOrder( o );

   order_prep_count = 0;
   order_prep_cond = TRUE;
   order_prep_last_input = ORDER_PREP_LAST_ORDER;
   selectConditionButton( TRUE );
   return 0;
}

// Summons
SummonMarker* summonMarker = NULL;

int startSummon( Order o )
{
   log( "Starting a summon" );
   int x = o.count,
       y = o.iteration;

   if (GRID_AT(unit_grid,x,y) != NULL) // Summon fails, obvi
      return -2;
   if (!canMove( x, y, x, y ))
      return -2;
   if (!canMoveUnit( x, y, x, y, NULL ))
      return -2;

   summonMarker = SummonMarker::get( x, y );
   GRID_AT(unit_grid, x, y) = summonMarker;

   return 0;
}

int completeSummon( Order o )
{
   int x = o.count,
       y = o.iteration;

   GRID_AT(unit_grid, x, y) = 0;
   summonMarker = NULL;

   Unit *u;

   if (o.action == SUMMON_MONSTER)
      u = new Monster( x, y, player->facing );
   if (o.action == SUMMON_SOLDIER)
      u = new Soldier( x, y, player->facing );
   if (o.action == SUMMON_WORM)
      u = new Worm( x, y, player->facing );
   if (o.action == SUMMON_BIRD)
      u = new Bird( x, y, player->facing );
   if (o.action == SUMMON_BUG)
      u = new Bug( x, y, player->facing );

   if (NULL != u)
   {
      addUnit( u );
      addEffect( SE_SUMMON_CLOUD, 0.8, x + 0.5, y + 0.5, 0, 0.6 );
   }

   return 0;
}

int alert( Unit *u, list<Unit*> &listeners = listening_units )
{
   listeners.push_back(u);

   u->active = 0;
   u->clearOrders();

   addEffect( SE_ALERT_MARKER, 0.3, u->x_real, u->y_real, 0, 0.1 );

   return 0;
}

int unalert( Unit *u )
{
   // Yeah this does nothing

   return 0;
}

int alertUnits( Order o )
{
   if (NULL == player)
      return -1;

   // Un-alert current listeners
   for (list<Unit*>::iterator it=listening_units.begin(); it != listening_units.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit) {
         unalert(unit);
      }
   }
   listening_units.clear();

   // Check all areas that are in the Atelier
   for (int x = 0; x < level_dim_x; ++x) {
      for (int y = 0; y < level_dim_y; ++y) {
         if (GRID_AT(terrain_grid,x,y) >= TER_ATELIER) {
            Unit *u = GRID_AT(unit_grid,x,y);
            if (u == player) continue;

            if (u && u->team == 0) { // On my team

               if (o.count != 0 && u->group != o.count)
                  continue;

               if (o.action == PL_ALERT_ALL) {
                  alert(u);
               } else if (o.action == PL_ALERT_MONSTERS && u->type == MONSTER_T) {
                  alert(u);
               } else if (o.action == PL_ALERT_SOLDIERS && u->type == SOLDIER_T) {
                  alert(u);
               } else if (o.action == PL_ALERT_WORMS && u->type == WORM_T) {
                  alert(u);
               } else if (o.action == PL_ALERT_BIRDS && u->type == BIRD_T) {
                  alert(u);
               } else if (o.action == PL_ALERT_BUGS && u->type == BUG_T) {
                  alert(u);
               }

            }
         }
      }
   }

   return 0;
}
         
int activateUnits( Order o )
{
   for (int x = 0; x < level_dim_x; ++x) {
      for (int y = 0; y < level_dim_y; ++y) {
         if (GRID_AT(terrain_grid,x,y) >= TER_ATELIER) {
            Unit *u = GRID_AT(unit_grid,x,y);
            if (u == player) continue;

            if (u && u->team == 0) { // On my team

               if (o.count != 0 && u->group != o.count)
                  continue;

               if (o.action == PL_CMD_GO_ALL) {
                  u->activate();
               } else if (o.action == PL_CMD_GO_MONSTERS && u->type == MONSTER_T) {
                  u->activate();
               } else if (o.action == PL_CMD_GO_SOLDIERS && u->type == SOLDIER_T) {
                  u->activate();
               } else if (o.action == PL_CMD_GO_WORMS && u->type == WORM_T) {
                  u->activate();
               } else if (o.action == PL_CMD_GO_BIRDS && u->type == BIRD_T) {
                  u->activate();
               } else if (o.action == PL_CMD_GO_BUGS && u->type == BUG_T) {
                  u->activate();
               } else if (o.action == PL_CMD_GO && u->team == o.count) {
                  u->activate();
               }
            
            }
         }
      }
   }

   return 0;
}

int activateAlert( Order o, list<Unit*> &listeners = listening_units )
{
   log("Activating alert units");

   for (list<Unit*>::iterator it=listeners.begin(); it != listeners.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit) {
         if (o.count != 0 && unit->group != o.count)
            continue;

         unit->activate();
      }
   }
   
   return 0;
}

int setUnitsGroup( Order o, list<Unit*> &listeners = listening_units )
{
   log("Setting alert units' group");

   for (list<Unit*>::iterator it=listeners.begin(); it != listeners.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit) {
         unit->group = o.count;
      }
   }
   
   return 0;
}

int broadcastOrder( Order o, list<Unit*> &listeners )
{
   log("Broadcasting order");
   if (NULL == player)
      return -1;

   for (list<Unit*>::iterator it=listeners.begin(); it != listeners.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit) {
         unit->addOrder( o );
      }
   }

   return 0;
}

int startPlayerCommand( Order o )
{
   if (NULL == player)
      return -1;

   switch( o.action ) {
      case PL_ALERT_ALL:
      case PL_ALERT_MONSTERS:
      case PL_ALERT_SOLDIERS:
      case PL_ALERT_WORMS:
      case PL_ALERT_BIRDS:
      case PL_ALERT_BUGS:
         alertUnits( o );
         break;
      case PL_SET_GROUP:
         setUnitsGroup( o );
         break;
      case SUMMON_MONSTER:
      case SUMMON_SOLDIER:
      case SUMMON_WORM:
      case SUMMON_BIRD:
      case SUMMON_BUG:
         return startSummon( o );
      case PL_CAST_HEAL:
      case PL_CAST_LIGHTNING:
      case PL_CAST_TIMELOCK:
      case PL_CAST_SCRY:
      case PL_CAST_CONFUSION:
         log("startPlayerCommand: Spells not implemented");
         // TODO
         break;
      default:
         break;
   }

   return 0;
}

int completePlayerCommand( Order o )
{
   switch( o.action ) {
      case PL_CMD_GO_ALL:
      case PL_CMD_GO_MONSTERS:
      case PL_CMD_GO_SOLDIERS:
      case PL_CMD_GO_WORMS:
      case PL_CMD_GO_BIRDS:
      case PL_CMD_GO_BUGS:
         activateUnits( o );
         break;
      case PL_CMD_GO:
         activateAlert( o );
         break;
      case SUMMON_MONSTER:
      case SUMMON_SOLDIER:
      case SUMMON_WORM:
      case SUMMON_BIRD:
      case SUMMON_BUG:
         completeSummon( o );
         break;
      default:
         break;
   }
   return 0;
}

// Bird ones, based on range

int alertUnitsRange( Order o, int x_middle, int y_middle, float range, list<Unit*> &listener_list )
{
   // Un-alert current listeners
   for (list<Unit*>::iterator it=listener_list.begin(); it != listener_list.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit) {
         unalert(unit);
      }
   }
   listener_list.clear();

   int x_min = x_middle - (int)range - 1,
       y_min = y_middle - (int)range - 1,
       x_max = x_middle + (int)range + 1,
       y_max = y_middle + (int)range + 1;
   if (x_min < 0) x_min = 0;
   if (y_min < 0) y_min = 0;
   if (x_max >= level_dim_x) x_max = level_dim_x - 1;
   if (y_max >= level_dim_y) y_max = level_dim_y - 1;

   float r_squared = range * range;

   for (int x = x_min; x <= x_max; ++x) {
      for (int y = y_min; y <= y_max; ++y) {
         float u_r_squared = ((x_middle - x) * (x_middle - x)) + ((y_middle - y) * (y_middle - y));
         if (u_r_squared > r_squared)
            continue;
         Unit *u = GRID_AT(unit_grid,x,y);
         if (u == player) continue;

         if (u && u->team == 0) { // On my team

            if (o.count != 0 && u->group != o.count)
               continue;

            if (o.action == PL_ALERT_ALL) {
               alert(u, listener_list);
            } else if (o.action == PL_ALERT_MONSTERS && u->type == MONSTER_T) {
               alert(u, listener_list);
            } else if (o.action == PL_ALERT_SOLDIERS && u->type == SOLDIER_T) {
               alert(u, listener_list);
            } else if (o.action == PL_ALERT_WORMS && u->type == WORM_T) {
               alert(u, listener_list);
            } else if (o.action == PL_ALERT_BIRDS && u->type == BIRD_T) {
               alert(u, listener_list);
            } else if (o.action == PL_ALERT_BUGS && u->type == BUG_T) {
               alert(u, listener_list);
            }
         }
      }
   }
   
   return 0;
}

int activateUnitsRange( Order o, int x_middle, int y_middle, float range, list<Unit*> &listener_list )
{
   int x_min = x_middle - (int)range - 1,
       y_min = y_middle - (int)range - 1,
       x_max = x_middle + (int)range + 1,
       y_max = y_middle + (int)range + 1;
   if (x_min < 0) x_min = 0;
   if (y_min < 0) y_min = 0;
   if (x_max >= level_dim_x) x_max = level_dim_x - 1;
   if (y_max >= level_dim_y) y_max = level_dim_y - 1;

   float r_squared = range * range;

   for (int x = x_min; x <= x_max; ++x) {
      for (int y = y_min; y <= y_max; ++y) {
         float u_r_squared = ((x_middle - x) * (x_middle - x)) + ((y_middle - y) * (y_middle - y));
         if (u_r_squared > r_squared)
            continue;
         Unit *u = GRID_AT(unit_grid,x,y);
         if (u == player) continue;

         if (u && u->team == 0) { // On my team

            if (o.count != 0 && u->group != o.count)
               continue;

            if (o.action == PL_CMD_GO_ALL) {
               u->activate();
            } else if (o.action == PL_CMD_GO_MONSTERS && u->type == MONSTER_T) {
               u->activate();
            } else if (o.action == PL_CMD_GO_SOLDIERS && u->type == SOLDIER_T) {
               u->activate();
            } else if (o.action == PL_CMD_GO_WORMS && u->type == WORM_T) {
               u->activate();
            } else if (o.action == PL_CMD_GO_BIRDS && u->type == BIRD_T) {
               u->activate();
            } else if (o.action == PL_CMD_GO_BUGS && u->type == BUG_T) {
               u->activate();
            } else if (o.action == PL_CMD_GO && u->team == o.count) {
               u->activate();
            }
         }
      }
   }
   
   return 0;
}

int startPlayerCommandRange( Order o, int x_middle, int y_middle, float range, list<Unit*> &listener_list )
{
   if (NULL == player)
      return -1;

   // These are the only things a bird can do
   switch( o.action ) {
      case PL_ALERT_ALL:
      case PL_ALERT_MONSTERS:
      case PL_ALERT_SOLDIERS:
      case PL_ALERT_WORMS:
      case PL_ALERT_BIRDS:
      case PL_ALERT_BUGS:
         alertUnitsRange( o, x_middle, y_middle, range, listener_list );
         break;
      case PL_SET_GROUP:
         setUnitsGroup( o, listener_list );
         break;
      default:
         break;
   }

   return 0;
}

int completePlayerCommandRange( Order o, int x_middle, int y_middle, float range, list<Unit*> &listener_list )
{
   switch( o.action ) {
      case PL_CMD_GO_ALL:
      case PL_CMD_GO_MONSTERS:
      case PL_CMD_GO_SOLDIERS:
      case PL_CMD_GO_WORMS:
      case PL_CMD_GO_BIRDS:
      case PL_CMD_GO_BUGS:
         activateUnitsRange( o, x_middle, y_middle, range, listener_list );
         break;
      case PL_CMD_GO:
         activateAlert( o, listening_units );
         break;
      case SUMMON_MONSTER:
      case SUMMON_SOLDIER:
      case SUMMON_WORM:
      case SUMMON_BIRD:
      case SUMMON_BUG:
         completeSummon( o );
         break;
      default:
         break;
   }
   return 0;
}


//////////////////////////////////////////////////////////////////////
// Loading ---

void initTextures()
{
   int i;
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();

   // TERRAIN SPRITES --

   // Setup terrain
   terrain_sprites = new Sprite*[NUM_TERRAINS];
   for (i = 0; i < NUM_TERRAINS; ++i)
      terrain_sprites[i] = NULL;

   terrain_sprites[TER_NONE] = NULL;
   // Trees
   terrain_sprites[TER_TREE_1] = new Sprite( *(t_manager.getTexture( "BasicTree1.png" )));
   normalizeTo1x1( terrain_sprites[TER_TREE_1] );
   for (i = TER_TREE_2; i <= TER_TREE_LAST; ++i) { 
      terrain_sprites[i] = new Sprite( *(t_manager.getTexture( "BasicTree2.png" )));
      normalizeTo1x1( terrain_sprites[i] );
   }

   // Rocks
   for (i = TER_ROCK_1; i <= TER_ROCK_LAST; ++i) { 
      terrain_sprites[i] = new Sprite( *(t_manager.getTexture( "Rock1.png" )));
      normalizeTo1x1( terrain_sprites[i] );
   }

   // CLIFF & EDGE_CLIFF
   // straight
   Vector2u dim = t_manager.getTexture( "CliffStraight.png" )->getSize();
   terrain_sprites[TER_CLIFF_S] = new Sprite( *(t_manager.getTexture( "CliffStraight.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_S] );
   terrain_sprites[TER_CLIFF_W] = new Sprite( *(t_manager.getTexture( "CliffStraight.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_W] );
   terrain_sprites[TER_CLIFF_W]->setRotation( 90 );
   terrain_sprites[TER_CLIFF_W]->setOrigin( 0, dim.y );
   terrain_sprites[TER_CLIFF_N] = new Sprite( *(t_manager.getTexture( "CliffStraight.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_N] );
   terrain_sprites[TER_CLIFF_N]->setRotation( 180 );
   terrain_sprites[TER_CLIFF_N]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_CLIFF_E] = new Sprite( *(t_manager.getTexture( "CliffStraight.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_E] );
   terrain_sprites[TER_CLIFF_E]->setRotation( 270 );
   terrain_sprites[TER_CLIFF_E]->setOrigin( dim.x, 0 );

   // EDGE_CLIFF
   dim = t_manager.getTexture( "EdgeCliffStraight.png" )->getSize();
   terrain_sprites[TER_EDGE_CLIFF_S] = new Sprite( *(t_manager.getTexture( "EdgeCliffStraight.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_S] );
   terrain_sprites[TER_EDGE_CLIFF_W] = new Sprite( *(t_manager.getTexture( "EdgeCliffStraight.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_W] );
   terrain_sprites[TER_EDGE_CLIFF_W]->setRotation( 90 );
   terrain_sprites[TER_EDGE_CLIFF_W]->setOrigin( 0, dim.y );
   terrain_sprites[TER_EDGE_CLIFF_N] = new Sprite( *(t_manager.getTexture( "EdgeCliffStraight.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_N] );
   terrain_sprites[TER_EDGE_CLIFF_N]->setRotation( 180 );
   terrain_sprites[TER_EDGE_CLIFF_N]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_EDGE_CLIFF_E] = new Sprite( *(t_manager.getTexture( "EdgeCliffStraight.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_E] );
   terrain_sprites[TER_EDGE_CLIFF_E]->setRotation( 270 );
   terrain_sprites[TER_EDGE_CLIFF_E]->setOrigin( dim.x, 0 );


   // edge
   terrain_sprites[TER_CLIFF_S_W_EDGE] = new Sprite( *(t_manager.getTexture( "CliffEndLeft.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_S_W_EDGE] );
   terrain_sprites[TER_CLIFF_S_E_EDGE] = new Sprite( *(t_manager.getTexture( "CliffEndRight.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_S_E_EDGE] );

   terrain_sprites[TER_CLIFF_W_N_EDGE] = new Sprite( *(t_manager.getTexture( "CliffEndLeft.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_W_N_EDGE] );
   terrain_sprites[TER_CLIFF_W_S_EDGE] = new Sprite( *(t_manager.getTexture( "CliffEndRight.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_W_S_EDGE] );
   terrain_sprites[TER_CLIFF_W_N_EDGE]->setRotation( 90 );
   terrain_sprites[TER_CLIFF_W_N_EDGE]->setOrigin( 0, dim.y );
   terrain_sprites[TER_CLIFF_W_S_EDGE]->setRotation( 90 );
   terrain_sprites[TER_CLIFF_W_S_EDGE]->setOrigin( 0, dim.y );

   terrain_sprites[TER_CLIFF_N_E_EDGE] = new Sprite( *(t_manager.getTexture( "CliffEndLeft.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_N_E_EDGE] );
   terrain_sprites[TER_CLIFF_N_W_EDGE] = new Sprite( *(t_manager.getTexture( "CliffEndRight.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_N_W_EDGE] );
   terrain_sprites[TER_CLIFF_N_E_EDGE]->setRotation( 180 );
   terrain_sprites[TER_CLIFF_N_E_EDGE]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_CLIFF_N_W_EDGE]->setRotation( 180 );
   terrain_sprites[TER_CLIFF_N_W_EDGE]->setOrigin( dim.x, dim.y );

   terrain_sprites[TER_CLIFF_E_S_EDGE] = new Sprite( *(t_manager.getTexture( "CliffEndLeft.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_E_S_EDGE] );
   terrain_sprites[TER_CLIFF_E_N_EDGE] = new Sprite( *(t_manager.getTexture( "CliffEndRight.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_E_N_EDGE] );
   terrain_sprites[TER_CLIFF_E_S_EDGE]->setRotation( 270 );
   terrain_sprites[TER_CLIFF_E_S_EDGE]->setOrigin( dim.x, 0 );
   terrain_sprites[TER_CLIFF_E_N_EDGE]->setRotation( 270 );
   terrain_sprites[TER_CLIFF_E_N_EDGE]->setOrigin( dim.x, 0 );


   // corner
   terrain_sprites[TER_CLIFF_CORNER_SE_90] = new Sprite( *(t_manager.getTexture( "CliffCorner90.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_CORNER_SE_90] );
   terrain_sprites[TER_CLIFF_CORNER_SW_90] = new Sprite( *(t_manager.getTexture( "CliffCorner90.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_CORNER_SW_90] );
   terrain_sprites[TER_CLIFF_CORNER_SW_90]->setRotation( 90 );
   terrain_sprites[TER_CLIFF_CORNER_SW_90]->setOrigin( 0, dim.y );
   terrain_sprites[TER_CLIFF_CORNER_NW_90] = new Sprite( *(t_manager.getTexture( "CliffCorner90.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_CORNER_NW_90] );
   terrain_sprites[TER_CLIFF_CORNER_NW_90]->setRotation( 180 ); 
   terrain_sprites[TER_CLIFF_CORNER_NW_90]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_CLIFF_CORNER_NE_90] = new Sprite( *(t_manager.getTexture( "CliffCorner90.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_CORNER_NE_90] );
   terrain_sprites[TER_CLIFF_CORNER_NE_90]->setRotation( 270 );
   terrain_sprites[TER_CLIFF_CORNER_NE_90]->setOrigin( dim.x, 0 );

   terrain_sprites[TER_CLIFF_CORNER_SE_270] = new Sprite( *(t_manager.getTexture( "CliffCorner270.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_CORNER_SE_270] );
   terrain_sprites[TER_CLIFF_CORNER_SW_270] = new Sprite( *(t_manager.getTexture( "CliffCorner270.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_CORNER_SW_270] );
   terrain_sprites[TER_CLIFF_CORNER_SW_270]->setRotation( 90 );
   terrain_sprites[TER_CLIFF_CORNER_SW_270]->setOrigin( 0, dim.y );
   terrain_sprites[TER_CLIFF_CORNER_NW_270] = new Sprite( *(t_manager.getTexture( "CliffCorner270.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_CORNER_NW_270] );
   terrain_sprites[TER_CLIFF_CORNER_NW_270]->setRotation( 180 ); 
   terrain_sprites[TER_CLIFF_CORNER_NW_270]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_CLIFF_CORNER_NE_270] = new Sprite( *(t_manager.getTexture( "CliffCorner270.png" )));
   normalizeTo1x1( terrain_sprites[TER_CLIFF_CORNER_NE_270] );
   terrain_sprites[TER_CLIFF_CORNER_NE_270]->setRotation( 270 );
   terrain_sprites[TER_CLIFF_CORNER_NE_270]->setOrigin( dim.x, 0 );

   // EDGE_CLIFF
   terrain_sprites[TER_EDGE_CLIFF_CORNER_SE_90] = new Sprite( *(t_manager.getTexture( "EdgeCliffCorner90.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_CORNER_SE_90] );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_SW_90] = new Sprite( *(t_manager.getTexture( "EdgeCliffCorner90.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_CORNER_SW_90] );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_SW_90]->setRotation( 90 );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_SW_90]->setOrigin( 0, dim.y );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NW_90] = new Sprite( *(t_manager.getTexture( "EdgeCliffCorner90.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_CORNER_NW_90] );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NW_90]->setRotation( 180 ); 
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NW_90]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NE_90] = new Sprite( *(t_manager.getTexture( "EdgeCliffCorner90.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_CORNER_NE_90] );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NE_90]->setRotation( 270 );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NE_90]->setOrigin( dim.x, 0 );

   terrain_sprites[TER_EDGE_CLIFF_CORNER_SE_270] = new Sprite( *(t_manager.getTexture( "EdgeCliffCorner270.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_CORNER_SE_270] );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_SW_270] = new Sprite( *(t_manager.getTexture( "EdgeCliffCorner270.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_CORNER_SW_270] );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_SW_270]->setRotation( 90 );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_SW_270]->setOrigin( 0, dim.y );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NW_270] = new Sprite( *(t_manager.getTexture( "EdgeCliffCorner270.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_CORNER_NW_270] );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NW_270]->setRotation( 180 ); 
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NW_270]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NE_270] = new Sprite( *(t_manager.getTexture( "EdgeCliffCorner270.png" )));
   normalizeTo1x1( terrain_sprites[TER_EDGE_CLIFF_CORNER_NE_270] );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NE_270]->setRotation( 270 );
   terrain_sprites[TER_EDGE_CLIFF_CORNER_NE_270]->setOrigin( dim.x, 0 );

   // Path
   dim = t_manager.getTexture( "DirtPathEW.png" )->getSize();
   terrain_sprites[TER_PATH_EW] = new Sprite( *(t_manager.getTexture( "DirtPathEW.png" )));
   normalizeTo1x1( terrain_sprites[TER_PATH_EW] );
   terrain_sprites[TER_PATH_NS] = new Sprite( *(t_manager.getTexture( "DirtPathEW.png" )));
   normalizeTo1x1( terrain_sprites[TER_PATH_NS] );
   terrain_sprites[TER_PATH_NS]->setRotation( 90 );
   terrain_sprites[TER_PATH_NS]->setOrigin( 0, dim.y );

   terrain_sprites[TER_PATH_W_END] = new Sprite( *(t_manager.getTexture( "DirtPathEndW.png" )));
   normalizeTo1x1( terrain_sprites[TER_PATH_W_END] );
   terrain_sprites[TER_PATH_N_END] = new Sprite( *(t_manager.getTexture( "DirtPathEndW.png" )));
   normalizeTo1x1( terrain_sprites[TER_PATH_N_END] );
   terrain_sprites[TER_PATH_N_END]->setRotation( 90 );
   terrain_sprites[TER_PATH_N_END]->setOrigin( 0, dim.y );
   terrain_sprites[TER_PATH_E_END] = new Sprite( *(t_manager.getTexture( "DirtPathEndW.png" )));
   normalizeTo1x1( terrain_sprites[TER_PATH_E_END] );
   terrain_sprites[TER_PATH_E_END]->setRotation( 180 ); 
   terrain_sprites[TER_PATH_E_END]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_PATH_S_END] = new Sprite( *(t_manager.getTexture( "DirtPathEndW.png" )));
   normalizeTo1x1( terrain_sprites[TER_PATH_S_END] );
   terrain_sprites[TER_PATH_S_END]->setRotation( 270 );
   terrain_sprites[TER_PATH_S_END]->setOrigin( dim.x, 0 );

   terrain_sprites[TER_PATH_NW] = new Sprite( *(t_manager.getTexture( "DirtPathNW.png" )));
   normalizeTo1x1( terrain_sprites[TER_PATH_NW] );
   terrain_sprites[TER_PATH_NE] = new Sprite( *(t_manager.getTexture( "DirtPathNW.png" )));
   normalizeTo1x1( terrain_sprites[TER_PATH_NE] );
   terrain_sprites[TER_PATH_NE]->setRotation( 90 );
   terrain_sprites[TER_PATH_NE]->setOrigin( 0, dim.y );
   terrain_sprites[TER_PATH_SE] = new Sprite( *(t_manager.getTexture( "DirtPathNW.png" )));
   normalizeTo1x1( terrain_sprites[TER_PATH_SE] );
   terrain_sprites[TER_PATH_SE]->setRotation( 180 ); 
   terrain_sprites[TER_PATH_SE]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_PATH_SW] = new Sprite( *(t_manager.getTexture( "DirtPathNW.png" )));
   normalizeTo1x1( terrain_sprites[TER_PATH_SW] );
   terrain_sprites[TER_PATH_SW]->setRotation( 270 );
   terrain_sprites[TER_PATH_SW]->setOrigin( dim.x, 0 );

   // Atelier

   terrain_sprites[TER_ATELIER] = new Sprite( *(t_manager.getTexture( "AtelierCenter.png" )));
   normalizeTo1x1( terrain_sprites[TER_ATELIER] );

   // edge
   dim = t_manager.getTexture( "AtelierEdge.png" )->getSize();
   terrain_sprites[TER_ATELIER_S_EDGE] = new Sprite( *(t_manager.getTexture( "AtelierEdge.png" )));
   normalizeTo1x1( terrain_sprites[TER_ATELIER_S_EDGE] );
   terrain_sprites[TER_ATELIER_W_EDGE] = new Sprite( *(t_manager.getTexture( "AtelierEdge.png" )));
   normalizeTo1x1( terrain_sprites[TER_ATELIER_W_EDGE] );
   terrain_sprites[TER_ATELIER_W_EDGE]->setRotation( 90 );
   terrain_sprites[TER_ATELIER_W_EDGE]->setOrigin( 0, dim.y );
   terrain_sprites[TER_ATELIER_N_EDGE] = new Sprite( *(t_manager.getTexture( "AtelierEdge.png" )));
   normalizeTo1x1( terrain_sprites[TER_ATELIER_N_EDGE] );
   terrain_sprites[TER_ATELIER_N_EDGE]->setRotation( 180 );
   terrain_sprites[TER_ATELIER_N_EDGE]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_ATELIER_E_EDGE] = new Sprite( *(t_manager.getTexture( "AtelierEdge.png" )));
   normalizeTo1x1( terrain_sprites[TER_ATELIER_E_EDGE] );
   terrain_sprites[TER_ATELIER_E_EDGE]->setRotation( 270 );
   terrain_sprites[TER_ATELIER_E_EDGE]->setOrigin( dim.x, 0 );
   
   // corner
   terrain_sprites[TER_ATELIER_CORNER_SW] = new Sprite( *(t_manager.getTexture( "AtelierCorner.png" )));
   normalizeTo1x1( terrain_sprites[TER_ATELIER_CORNER_SW] );
   terrain_sprites[TER_ATELIER_CORNER_NW] = new Sprite( *(t_manager.getTexture( "AtelierCorner.png" )));
   normalizeTo1x1( terrain_sprites[TER_ATELIER_CORNER_NW] );
   terrain_sprites[TER_ATELIER_CORNER_NW]->setRotation( 90 );
   terrain_sprites[TER_ATELIER_CORNER_NW]->setOrigin( 0, dim.y );
   terrain_sprites[TER_ATELIER_CORNER_NE] = new Sprite( *(t_manager.getTexture( "AtelierCorner.png" )));
   normalizeTo1x1( terrain_sprites[TER_ATELIER_CORNER_NE] );
   terrain_sprites[TER_ATELIER_CORNER_NE]->setRotation( 180 ); 
   terrain_sprites[TER_ATELIER_CORNER_NE]->setOrigin( dim.x, dim.y );
   terrain_sprites[TER_ATELIER_CORNER_SE] = new Sprite( *(t_manager.getTexture( "AtelierCorner.png" )));
   normalizeTo1x1( terrain_sprites[TER_ATELIER_CORNER_SE] );
   terrain_sprites[TER_ATELIER_CORNER_SE]->setRotation( 270 );
   terrain_sprites[TER_ATELIER_CORNER_SE]->setOrigin( dim.x, 0 );

   
   // TERRAIN MOD SPRITES --

   // Setup array
   terrain_mod_sprites = new Sprite*[NUM_TERRAIN_MODS];
   for (i = 0; i < NUM_TERRAIN_MODS; ++i)
      terrain_mod_sprites[i] = NULL;

   // Path
   dim = t_manager.getTexture( "TrailEW.png" )->getSize();
   terrain_mod_sprites[TM_TRAIL_EW] = new Sprite( *(t_manager.getTexture( "TrailEW.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_TRAIL_EW] );
   terrain_mod_sprites[TM_TRAIL_NS] = new Sprite( *(t_manager.getTexture( "TrailEW.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_TRAIL_NS] );
   terrain_mod_sprites[TM_TRAIL_NS]->setRotation( 90 );
   terrain_mod_sprites[TM_TRAIL_NS]->setOrigin( 0, dim.y );

   terrain_mod_sprites[TM_TRAIL_W_END] = new Sprite( *(t_manager.getTexture( "TrailEndW.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_TRAIL_W_END] );
   terrain_mod_sprites[TM_TRAIL_N_END] = new Sprite( *(t_manager.getTexture( "TrailEndW.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_TRAIL_N_END] );
   terrain_mod_sprites[TM_TRAIL_N_END]->setRotation( 90 );
   terrain_mod_sprites[TM_TRAIL_N_END]->setOrigin( 0, dim.y );
   terrain_mod_sprites[TM_TRAIL_E_END] = new Sprite( *(t_manager.getTexture( "TrailEndW.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_TRAIL_E_END] );
   terrain_mod_sprites[TM_TRAIL_E_END]->setRotation( 180 ); 
   terrain_mod_sprites[TM_TRAIL_E_END]->setOrigin( dim.x, dim.y );
   terrain_mod_sprites[TM_TRAIL_S_END] = new Sprite( *(t_manager.getTexture( "TrailEndW.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_TRAIL_S_END] );
   terrain_mod_sprites[TM_TRAIL_S_END]->setRotation( 270 );
   terrain_mod_sprites[TM_TRAIL_S_END]->setOrigin( dim.x, 0 );

   terrain_mod_sprites[TM_TRAIL_NW] = new Sprite( *(t_manager.getTexture( "TrailNW.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_TRAIL_NW] );
   terrain_mod_sprites[TM_TRAIL_NE] = new Sprite( *(t_manager.getTexture( "TrailNW.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_TRAIL_NE] );
   terrain_mod_sprites[TM_TRAIL_NE]->setRotation( 90 );
   terrain_mod_sprites[TM_TRAIL_NE]->setOrigin( 0, dim.y );
   terrain_mod_sprites[TM_TRAIL_SE] = new Sprite( *(t_manager.getTexture( "TrailNW.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_TRAIL_SE] );
   terrain_mod_sprites[TM_TRAIL_SE]->setRotation( 180 ); 
   terrain_mod_sprites[TM_TRAIL_SE]->setOrigin( dim.x, dim.y );
   terrain_mod_sprites[TM_TRAIL_SW] = new Sprite( *(t_manager.getTexture( "TrailNW.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_TRAIL_SW] );
   terrain_mod_sprites[TM_TRAIL_SW]->setRotation( 270 );
   terrain_mod_sprites[TM_TRAIL_SW]->setOrigin( dim.x, 0 );

   // Wormholes
   terrain_mod_sprites[TM_WH_OPEN_1] = terrain_mod_sprites[TM_WH_CLOSE_1] = new Sprite( *(t_manager.getTexture( "WormholeRed.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_WH_OPEN_1] );
   terrain_mod_sprites[TM_WH_OPEN_2] = terrain_mod_sprites[TM_WH_CLOSE_2] = new Sprite( *(t_manager.getTexture( "WormholeOrange.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_WH_OPEN_2] );
   terrain_mod_sprites[TM_WH_OPEN_3] = terrain_mod_sprites[TM_WH_CLOSE_3] = new Sprite( *(t_manager.getTexture( "WormholeYellow.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_WH_OPEN_3] );
   terrain_mod_sprites[TM_WH_OPEN_4] = terrain_mod_sprites[TM_WH_CLOSE_4] = new Sprite( *(t_manager.getTexture( "WormholeGreen.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_WH_OPEN_4] );
   terrain_mod_sprites[TM_WH_OPEN_5] = terrain_mod_sprites[TM_WH_CLOSE_5] = new Sprite( *(t_manager.getTexture( "WormholeBlue.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_WH_OPEN_5] );
   terrain_mod_sprites[TM_WH_OPEN_6] = terrain_mod_sprites[TM_WH_CLOSE_6] = new Sprite( *(t_manager.getTexture( "WormholePurple.png" )));
   normalizeTo1x1( terrain_mod_sprites[TM_WH_OPEN_6] );

   // Base Terrain
   base_grass_sprite = new Sprite( *(t_manager.getTexture( "GreenGrass.png" )));
   base_mountain_sprite = new Sprite( *(t_manager.getTexture( "GrayRock.png" )));
   base_underground_sprite = new Sprite( *(t_manager.getTexture( "BrownRock.png" )));

}

void init()
{
   if (level_init) return;

   level_view = new View();
   view_rel_x_to_y = ((float)config::height()) / ((float)config::width());

   initTextures();

   wormhole_v = new Vector2i[c_wormhole_locations];

   level_init = true;
}

void clearGrids()
{
   if (terrain_grid) {
      delete[] terrain_grid;
      terrain_grid = NULL;
   }
   if (terrain_mod_grid) {
      delete[] terrain_mod_grid;
      terrain_mod_grid = NULL;
      num_wormholes = 0;
   }
   if (unit_grid) {
      delete[] unit_grid;
      unit_grid = NULL;
   }
   if (vision_grid) {
      delete[] vision_grid;
      vision_grid = NULL;
   }
   if (ai_vision_grid) {
      delete[] ai_vision_grid;
      ai_vision_grid = NULL;
   }
}

void clearWormholes()
{
   num_wormholes = 0;

   for (int i = 0; i < c_wormhole_locations; ++i)
      wormhole_v[i] = Vector2i( -1, -1 );
}

void clearUnits()
{
   list<Unit*>::iterator it;
   for (it = unit_list.begin(); it != unit_list.end(); ++it)
   {
      Unit *u = (*it);
      if (u)
         delete u;
   }

   unit_list.clear();
   listening_units.clear();
}

void clearEffects()
{
   list<Effect*>::iterator it;
   for (it = effect_list.begin(); it != effect_list.end(); ++it)
   {
      Effect *e = (*it);
      if (e)
         delete e;
   }

   effect_list.clear();
}

void clearAll()
{
   clearGrids();
   clearWormholes();

   clearUnits();
   clearEffects();
}

int initGrids(int x, int y)
{
   if (x < 1 || x > c_max_dimension || y < 1 || y > c_max_dimension)
      return -1;

   clearGrids();
   clearWormholes();

   level_dim_x = x;
   level_dim_y = y;
   level_fog_dim = 3;

   int dim = x * y;

   terrain_grid = new Terrain[dim];
   for (int i = 0; i < dim; ++i) terrain_grid[i] = TER_NONE;

   terrain_mod_grid = new TerrainMod[dim];
   for (int i = 0; i < dim; ++i) terrain_mod_grid[i] = TM_NONE;

   unit_grid = new Unit*[dim];
   for (int i = 0; i < dim; ++i) unit_grid[i] = NULL;

   vision_grid = new Vision[dim];
   for (int i = 0; i < dim; ++i) vision_grid[i] = VIS_NEVER_SEEN;

   ai_vision_grid = new Vision[dim];
   for (int i = 0; i < dim; ++i) ai_vision_grid[i] = VIS_NEVER_SEEN;

   log("initGrids succeeded");

   return 0;
}

int changeLevelDimensions( int new_x_dim, int new_y_dim )
{
   if (new_x_dim < 1 || new_x_dim > c_max_dimension 
         || new_y_dim < 1 || new_y_dim > c_max_dimension) 
      return -2;

   // Copy old stuff
   int old_x_dim = level_dim_x, old_y_dim = level_dim_y;

   int i;
   Terrain* old_terrain_grid = new Terrain[level_dim_x * level_dim_y];
   for (i = 0; i < level_dim_x * level_dim_y; ++i)
      old_terrain_grid[i] = terrain_grid[i];

   TerrainMod* old_terrain_mod_grid = new TerrainMod[level_dim_x * level_dim_y];
   for (i = 0; i < level_dim_x * level_dim_y; ++i)
      old_terrain_mod_grid[i] = terrain_mod_grid[i];

   Unit** old_unit_grid = new Unit*[level_dim_x * level_dim_y];
   for (i = 0; i < level_dim_x * level_dim_y; ++i)
      old_unit_grid[i] = unit_grid[i];

   initGrids( new_x_dim, new_y_dim );

   for (int x = 0; x < old_x_dim; ++x) {
      for (int y = 0; y < old_y_dim; ++y) {
         if (x >= level_dim_x || y >= level_dim_y) { // Outside new dimensions
            // Delete any Unit there
            Unit *u = old_unit_grid[ x + (y * old_y_dim)];
            if (u) {
               list<Unit*>::iterator it = find( unit_list.begin(), unit_list.end(), u );
               if (it != unit_list.end()) removeUnit( it );
            }
         } else {
            // Copy everything over
            GRID_AT(terrain_grid,x,y) = old_terrain_grid[x + (y * old_x_dim)];
            GRID_AT(terrain_mod_grid,x,y) = old_terrain_mod_grid[x + (y * old_x_dim)];
            GRID_AT(unit_grid,x,y) = old_unit_grid[x + (y * old_x_dim)];
         }
      }
   }

   delete[] old_terrain_grid;
   delete[] old_unit_grid;

   return 0;
}

//////////////////////////////////////////////////////////////////////
// Loading Level --- 

Terrain parseTerrain( char c )
{
   return (Terrain)(int)c;
}

// Mirrors parseTerrain
int terrainToCharData( Terrain t, char&c )
{
   c = (char)(int)t;
   return 0;
}

#define FACING_BITS 0x3
#define WIN_CONDITION_BIT 0x4

int writeUnitNodeAI( AIUnit *ai, pugi::xml_node &base )
{
   if (NULL == ai)
      return -1;

   pugi::xml_node node = base.append_child("ai");

   node.append_attribute("ai_move") = (int) ai->ai_move_style;

   node.append_attribute("ai_aggro") = (int) ai->ai_aggro;
   node.append_attribute("ai_chase") = ai->ai_chase_distance;
   node.append_attribute("ai_attack") = ai->ai_attack_distance;

   if (NULL == ai->ai_leader) {
      node.append_attribute("ai_leader_x") = -1;
      node.append_attribute("ai_leader_y") = -1;
   }
   else
   {
      node.append_attribute("ai_leader_x") = ai->ai_leader->x_grid;
      node.append_attribute("ai_leader_y") = ai->ai_leader->y_grid;
   }

   if (!ai->ai_waypoints.empty()) {
      pugi::xml_node waypoints = node.append_child("ai_wps");
      for (std::deque< std::pair<sf::Vector2i,Direction> >::iterator it=ai->ai_waypoints.begin(); it != ai->ai_waypoints.end(); ++it) {
         pugi::xml_node wp = waypoints.append_child("wp");
         wp.append_attribute("x") = (*it).first.x;
         wp.append_attribute("y") = (*it).first.y;
         wp.append_attribute("f") = (*it).second;
      }
   }

   return 0;
}

int writeUnitNodeXML( Unit *u, pugi::xml_node &node )
{
   if (NULL == u)
      return -1;

   node.append_attribute("t") = u->type;
   node.append_attribute("x") = u->x_grid;
   node.append_attribute("y") = u->y_grid;
   node.append_attribute("f") = (int)u->facing;
   node.append_attribute("win_c") = u->win_condition;

   if (u->type >= R_HUMAN_ARCHER_T && u->type < SUMMONMARKER_T) {
      AIUnit *ai = (AIUnit*)u;
      if (ai)
         writeUnitNodeAI( ai, node );
   }

   return 0;
}

int parseUnitNodeAI( AIUnit *u, pugi::xml_node &base )
{
   pugi::xml_node ai = base.first_child();

   if (!ai)
      return -1;

   AI_Movement ai_move;
   AI_Aggression ai_aggro;
   float chase_dis, attack_dis;

   ai_move = (AI_Movement) ai.attribute("ai_move").as_int();
   ai_aggro = (AI_Aggression) ai.attribute("ai_aggro").as_int();
   chase_dis = ai.attribute("ai_chase").as_float();
   attack_dis = ai.attribute("ai_attack").as_float();

   // Figure out leader
   int leader_x, leader_y;
   leader_x = ai.attribute("ai_leader_x").as_int();
   leader_y = ai.attribute("ai_leader_y").as_int();
   Unit *leader;
   if (leader_x == -1 || leader_y == -1)
      leader = NULL;
   else
      leader = GRID_AT(unit_grid,leader_x,leader_y);

   u->setAI( ai_move, ai_aggro, chase_dis, attack_dis, leader );

   // Waypoints
   u->clearWaypoints();
   pugi::xml_node waypoints = ai.first_child();
   for (pugi::xml_node wp_node = waypoints.first_child(); wp_node; wp_node = wp_node.next_sibling()) {
      int grid_x, grid_y;
      Direction face;
      grid_x = wp_node.attribute("x").as_int();
      grid_y = wp_node.attribute("y").as_int();
      face = (Direction) wp_node.attribute("f").as_int();

      u->addWaypoint( grid_x, grid_y, face );
   }

   return 0;
}

int parseAndCreateUnitXML( pugi::xml_node &node )
{
   int x, y;
   Direction face;
   UnitType type;
   bool win_c;

   x = node.attribute("x").as_int();
   y = node.attribute("y").as_int();
   face = (Direction) node.attribute("f").as_int();
   type = (UnitType) node.attribute("t").as_int();
   win_c = node.attribute("win_c").as_bool();

   if (type == PLAYER_T) {
      player = Player::initPlayer( x, y, face );
      addPlayer();
   }
   else 
   {
      Unit *u = genBaseUnit( type, x, y, face );
      addUnit( u );
      if (win_c)
         addUnitWinCondition( u );

      // AI
      if (type >= R_HUMAN_ARCHER_T && type < SUMMONMARKER_T)
         parseUnitNodeAI( (AIUnit*) u, node );
   }

   return 0;
}

int parseAndCreateUnit( char c1, char c2, char x, char y )
{
   Direction facing;
   switch ((c2 & FACING_BITS))
   {
      case 0x0:
         facing = EAST;
         break;
      case 0x1:
         facing = SOUTH;
         break;
      case 0x2:
         facing = WEST;
         break;
      case 0x3:
         facing = NORTH;
         break;
   }

   switch ((UnitType)(int)c1)
   {
      case PLAYER_T:
         if (NULL != player) return -1;
         player = Player::initPlayer( (int)x, (int)y, facing );
         addPlayer();
         break;
      default:
         Unit *u = genBaseUnit( (UnitType)(int)c1, (int)x, (int)y, facing );
         addUnit( u );
         if (c2 & WIN_CONDITION_BIT)
            addUnitWinCondition( u );
         break;
   }

   return 0;
}

// Mirrors parseAndCreateUnit
int unitToCharData( Unit *u, char &c1, char &c2, char &x, char &y )
{
   if (NULL == u)
      return -1;

   c2 = 0; // clear
   switch (u->facing) {
      case EAST:
         c2 |= 0x0;
         break;
      case SOUTH:
         c2 |= 0x1;
         break;
      case WEST:
         c2 |= 0x2;
         break;
      case NORTH:
         c2 |= 0x3;
         break;
      default:
         break;
   }

   if (u->win_condition == true)
      c2 &= WIN_CONDITION_BIT;

   c1 = (char)(int)u->type;

   x = u->x_grid;
   y = u->y_grid;

   return 0;
}

int createLevelFromFile( string filename )
{
   pugi::xml_document doc;

   try {
      pugi::xml_parse_result result = doc.load_file( filename.c_str() );
      if (result.status != pugi::status_ok) {
         log("EE - Level parsing failed");
         return -1;
      }
   } catch ( const std::exception& e ) {
      log( "EE - Exception caught when parsing level file" );
      return -1;
   }


   pugi::xml_node level_node = doc.first_child();

   // add parameters node
   pugi::xml_node param = level_node.first_child();

   // get attributes from param node
   int dim_x = param.attribute("x_dim").as_int(),
       dim_y = param.attribute("y_dim").as_int();
   initGrids(dim_x,dim_y);
   float view_size_x = param.attribute("v_s_x").as_float();
   float view_pos_x = param.attribute("v_c_x").as_float();
   float view_pos_y = param.attribute("v_c_y").as_float();
   setView( view_size_x, Vector2f( view_pos_x, view_pos_y ) );

   win_if_reached.x = param.attribute("win_loc_x").as_int();
   win_if_reached.y = param.attribute("win_loc_y").as_int();

   // terrain block
   pugi::xml_node terrain = param.first_child();
   stringstream ss( terrain.attribute("ter").as_string() );
   for (int x = 0; x < level_dim_x; ++x) {
      for (int y = 0; y < level_dim_y; ++y) {
         int i;
         ss >> i;
         GRID_AT(terrain_grid,x,y) = i;
      }
   }

   // Units
   pugi::xml_node units_node = param.next_sibling();

   for (pugi::xml_node unit_node = units_node.first_child(); unit_node; unit_node = unit_node.next_sibling())
   {
         parseAndCreateUnitXML( unit_node );
   }

   return 0;

   // Old version
   ifstream level_file( filename.c_str(), ios::in | ios::binary | ios::ate );
   ifstream::pos_type fileSize;
   char* fileContents;
   if(level_file.is_open())
   {
      level_file.seekg(0, ios::end);
      fileSize = level_file.tellg();
      fileContents = new char[fileSize];
      level_file.seekg(0, ios::beg);
      if(!level_file.read(fileContents, fileSize))
      {
         log("Failed to read level file");
         level_file.close();
         return -1;
      }
      else
      {
         int x, y, i;
         int counter = 0;

         int dim_x = (int)fileContents[counter++];
         int dim_y = (int)fileContents[counter++];
         initGrids(dim_x,dim_y);

         float view_size_x = (float)(int)fileContents[counter++];
         float view_pos_x = (float)(int)fileContents[counter++];
         float view_pos_y = (float)(int)fileContents[counter++];
         setView( view_size_x, Vector2f( view_pos_x, view_pos_y ) );

         x = fileContents[counter++];
         y = fileContents[counter++];
         addLocationWinCondition( x, y );

         for (x = 0; x < dim_x; ++x) {
            for (y = 0; y < dim_y; ++y) {
               GRID_AT(terrain_grid,x,y) = parseTerrain( fileContents[counter++] );
            }
         }

         int num_units = (int)fileContents[counter++];

         for (i = 0; i < num_units; ++i) {
            char c1 = fileContents[counter++],
                 c2 = fileContents[counter++],
                 ux = fileContents[counter++],
                 uy = fileContents[counter++];
            parseAndCreateUnit( c1, c2, ux, uy );
         }

      }
      level_file.close();
   }
   if (!player) {
      player = Player::initPlayer( 0, 0, SOUTH );
      addPlayer();
   }
   return 0;
}

// Mirrors createLevelFromFile
int writeLevelToFile( string filename )
{
   // The new XML version of
   // writeLevelToFile

   pugi::xml_document doc;
   // add level node
   pugi::xml_node level_node = doc.append_child("level");

   // add parameters node
   pugi::xml_node param = level_node.append_child("param");

   // add attributes to param node
   param.append_attribute("x_dim") = level_dim_x;
   param.append_attribute("y_dim") = level_dim_y;
   param.append_attribute("v_s_x") = level_view->getSize().x;
   param.append_attribute("v_c_x") = level_view->getCenter().x;
   param.append_attribute("v_c_y") = level_view->getCenter().y;
   param.append_attribute("win_loc_x") = win_if_reached.x;
   param.append_attribute("win_loc_y") = win_if_reached.y;
   // terrain block
   stringstream ss;
   for (int x = 0; x < level_dim_x; ++x)
      for (int y = 0; y < level_dim_y; ++y)
         ss << ((int)GRID_AT(terrain_grid,x,y)) << " ";
   
   pugi::xml_node terrain = param.append_child("terrain");
   terrain.append_attribute("ter") = ss.str().c_str();

   pugi::xml_node units_node = level_node.append_child("units");
   if (player)
   {
      pugi::xml_node player_node = units_node.append_child("player");
      writeUnitNodeXML( player, player_node );
   }
   for (list<Unit*>::iterator it=unit_list.begin(); it != unit_list.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit) {
         pugi::xml_node unit_node = units_node.append_child();
         writeUnitNodeXML( unit, unit_node );
      }
   }

   try {
      doc.save_file( filename.c_str() );
   } catch ( const std::exception& e ) {
      log( "EE - Exception caught when writing level to file" );
      return -1;
   }

   return 0;



   // The old version

   ofstream level_file( filename.c_str(), ios::out | ios::binary | ios::ate );
   int unitcount = unit_list.size() + 1; // 1 = player
   int writesize = 2 // dimensions
                 + 3 // view
                 // Could be some other metadata here
                 + (level_dim_x * level_dim_y) // terrain data
                 + 1 // count of units
                 + (4 * (unitcount+1)) // unit descriptions
                 // Should be something here for buildings
                 ;
   char *fileContents = new char[writesize];
   if(level_file.is_open())
   {
      int i = 0, x, y;
      // Dimensions
      fileContents[i++] = (char)level_dim_x;
      fileContents[i++] = (char)level_dim_y;

      // View
      fileContents[i++] = (char)(int)level_view->getSize().x;
      fileContents[i++] = (char)(int)level_view->getCenter().x;
      fileContents[i++] = (char)(int)level_view->getCenter().y;

      if (win_if_reached.x == -1) fileContents[i++] = (char)0xff;
      else fileContents[i++] = (char)win_if_reached.x;
      if (win_if_reached.y == -1) fileContents[i++] = (char)0xff;
      else fileContents[i++] = (char)win_if_reached.y;

      for (x = 0; x < level_dim_x; ++x) {
         for (y = 0; y < level_dim_y; ++y) {
            terrainToCharData( GRID_AT(terrain_grid,x,y), fileContents[i] );
            i++;
         }
      }

      // Units
      fileContents[i++] = unitcount + 1;

      if (player)
      {
         char c1, c2, ux, uy;
         unitToCharData( player, c1, c2, ux, uy );
         fileContents[i++] = c1;
         fileContents[i++] = c2;
         fileContents[i++] = ux;
         fileContents[i++] = uy;
      }

      for (list<Unit*>::iterator it=unit_list.begin(); it != unit_list.end(); ++it)
      {
         Unit* unit = (*it);
         if (unit) {
            char c1, c2, ux, uy;
            unitToCharData( unit, c1, c2, ux, uy );
            fileContents[i++] = c1;
            fileContents[i++] = c2;
            fileContents[i++] = ux;
            fileContents[i++] = uy;
         }
      }

      level_file.write(fileContents, writesize);

      log("Wrote current level to file");
      delete fileContents;
      return 0;
   }

   log("Write level to file error: file failed to open");
   delete fileContents;
   return -1;
   
}

int loadLevel( int level_id )
{
   if (!level_init)
      init();

   clearWinConditions();

   // Currently only loads test level
   if (level_id == -1)
   {
      base_terrain = BASE_TER_GRASS;
      if (createLevelFromFile( "res/new_level_-1.lvl" ) == -1)
         return -1;
   }
   else if (level_id == -2)
   {
      base_terrain = BASE_TER_GRASS;
      if (createLevelFromFile( "res/new_level_-2.lvl" ) == -1)
         return -1;
   }
   else if (level_id == 0 || true)
   {
      base_terrain = BASE_TER_GRASS;
      if (createLevelFromFile( "res/testlevel0.lvl" ) == -1)
         return -1;
   }


   menu_state = MENU_MAIN | MENU_PRI_INGAME;
   setLevelListener(true);

   vision_enabled = true;
   show_framerate = false;
   clearFramerate();
   calculateVision();

   turn = 0;
   turn_progress = 0;

   return 0;
}


//////////////////////////////////////////////////////////////////////
// Pause ---

void pause()
{
   pause_state = -1;
}

void unpause()
{
   pause_state = c_fully_paused - 1;
}

void togglePause()
{
   if (pause_state == 0)
      pause();
   else if (pause_state == c_fully_paused)
      unpause();
}

void drawPause()
{
   if (pause_state == 0) return;

   int transparency = (128 * abs(pause_state)) / c_fully_paused;
   Color c( 192, 192, 192, transparency );
   RectangleShape r( Vector2f(config::width(), config::height()) );
   r.setFillColor( c );
   r.setPosition( 0, 0 );
   SFML_GlobalRenderWindow::get()->draw( r );
}

int updatePause( int dt )
{
   if (pause_state == c_fully_paused)
      return 2;
   else if (pause_state < 0) {
      float factor = (float)(c_fully_paused + pause_state) / (float)c_fully_paused;
      int d_pause = (dt / 10) + 1;
      dt *= factor;
      pause_state -= d_pause;
      if (pause_state <= -c_fully_paused)
         pause_state = c_fully_paused;
   }
   else if (pause_state > 0) {
      float factor = (float)(c_fully_paused + pause_state) / (float)c_fully_paused;
      int d_pause = (dt / 10) + 1;
      dt *= factor;
      pause_state -= d_pause;
      if (pause_state <= 0)
         pause_state = 0;
   }
   return 0;
}

//////////////////////////////////////////////////////////////////////
// Update ---

int prepareTurnAll()
{
   if (NULL != player)
      player->prepareTurn();

   for (list<Unit*>::iterator it=unit_list.begin(); it != unit_list.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit) {
         unit->prepareTurn();
      }
   }

   return 0;
}

int startTurnAll( )
{
   if (NULL != player)
      player->startTurn();

   for (list<Unit*>::iterator it=unit_list.begin(); it != unit_list.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit) {
         unit->startTurn();
      }
   }

   return 0;
}

int updateAll( int dt )
{
   float dtf = (float)dt / (float)c_turn_length;
   int result;

   if (NULL != player)
      player->update( dtf );

   if (NULL != summonMarker)
      summonMarker->update( dtf );

   for (list<Unit*>::iterator it=unit_list.begin(); it != unit_list.end();)
   {
      Unit* unit = (*it);
      if (unit) {
         result = unit->update( dtf );
         if (result == 1) {
            // Kill the unit
            list<Unit*>::iterator to_delete = it;
            ++it;
            removeUnit( to_delete );
            continue;
         }
      }
      ++it;
   }
   for (list<Effect*>::iterator it=effect_list.begin(); it != effect_list.end(); ++it)
   {
      Effect* effect = (*it);
      if (effect) {
         result = effect->update( dtf );
         if (result == 1) {
            // Delete the effect
            delete effect;
            (*it) = NULL;
         }
      }
   }

   return 0;
}

int swapWormholes()
{
   for (int i = 0; i < 6; ++i) {
      Vector2i in = wormhole_v[2*i],
               out = wormhole_v[2*i + 1];

      if (in.x >= 0 && in.y >= 0 && in.x < level_dim_x && in.y < level_dim_y
       && out.x >= 0 && out.y >= 0 && out.x < level_dim_x && out.y < level_dim_y) {
         // Swap

         Unit *u1 = GRID_AT(unit_grid,in.x,in.y),
              *u2 = GRID_AT(unit_grid,out.x,out.y);

         GRID_AT(unit_grid,in.x,in.y) = u2;
         GRID_AT(unit_grid,out.x,out.y) = u1;

         if (u1) {
            u1->x_grid = in.x;
            u1->y_grid = in.y;
            u1->x_real = in.x + 0.5;
            u1->y_real = in.y + 0.5;
         }
         if (u2) {
            u2->x_grid = out.x;
            u2->y_grid = out.y;
            u2->x_real = out.x + 0.5;
            u2->y_real = out.y + 0.5;
         }

         // TODO: Effects
      }
   }

   return num_wormholes;
}

int completeTurnAll( )
{
   if (NULL != player)
      player->completeTurn();

   for (list<Unit*>::iterator it=unit_list.begin(); it != unit_list.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit) {
         unit->completeTurn();
      }
   }

   swapWormholes();

   return 0;
}

int updateLevel( int dt )
{
   calculateFramerate( dt );
   tickcount++;

   if (updatePause( dt ) == 2)
      return 2;

   if (victory == true)
      return 3;

   int til_end = c_turn_length - turn_progress;
   turn_progress += dt;

   if (turn_progress > c_turn_length) {
      turn_progress -= c_turn_length;

      updateAll( til_end );

      between_turns = true;
      completeTurnAll();

      if (checkWinConditions() == true) {
         victory = true;
         return 0;
      }

      turn++;
      tickfps = tickcount;
      tickcount = 0;
      calculateVision();

      prepareTurnAll();
      startTurnAll();
      between_turns = false;

      updateAll( turn_progress );
   } else {
      updateAll( dt );
   }
   return 0;
}

//////////////////////////////////////////////////////////////////////
// GUI ---

//////////////////////////////////////////////////////////////////////
// Right Click Menu ---

int cast_menu_x_grid, cast_menu_y_grid; // the affected grid square
int cast_menu_grid_left, cast_menu_grid_top, cast_menu_grid_size;

int cast_menu_box = 1; // 0 not expanded, 1 above, 2 right, 3 below, 4 left
Vector2u cast_menu_top_left, cast_menu_size;
int cast_menu_button_size;

// Deprecated
int cast_menu_top_x_left, cast_menu_top_y_bottom; // Top box
int cast_menu_bottom_x_left, cast_menu_bottom_y_top; // Bottom box
float cast_menu_button_size_x, cast_menu_button_size_y;

int cast_menu_selection = 0; // 1 == closest to source

bool cast_menu_open = false;
bool cast_menu_solid = false;
bool cast_menu_summons_allowed = true;

void initRightClickMenu()
{
   cast_menu_open = false;
   cast_menu_solid = false;
}

int castMenuCreate( Vector2i grid )
{
   if (grid.x < 0 || grid.x >= level_dim_x || grid.y < 0 || grid.y >= level_dim_y)
      return -1;
   
   cast_menu_x_grid = grid.x;
   cast_menu_y_grid = grid.y;

   cast_menu_top_left = coordsViewToWindow( grid.x, grid.y );
   Vector2u bottom_right = coordsViewToWindow( grid.x + 1, grid.y + 1 );
   cast_menu_grid_size = cast_menu_size.x = cast_menu_size.y = bottom_right.y - cast_menu_top_left.y;

   cast_menu_box = 0;
   cast_menu_grid_left = cast_menu_top_left.x;
   cast_menu_grid_top = cast_menu_top_left.y;

   cast_menu_selection = 0;

   // Calculate how big to make the buttons
   float x_dim = config::width(), y_dim = config::height();
   cast_menu_button_size_x = x_dim / 6;
   cast_menu_button_size_y = y_dim / 30;
   cast_menu_button_size = x_dim / 20;

   cast_menu_open = true;
   cast_menu_solid = false;

   if (GRID_AT(terrain_grid,grid.x,grid.y) >= TER_ATELIER)
      cast_menu_summons_allowed = true;
   else
      cast_menu_summons_allowed = false;

   log("Right click menu created");

   return 0;
}

int castMenuSelect( unsigned int x, unsigned int y )
{
   if (cast_menu_box == 0)
   {
      if (x < cast_menu_top_left.x)
         cast_menu_box = 4;
      else if (x > cast_menu_top_left.x + cast_menu_size.x)
         cast_menu_box = 2;
      else if (y < cast_menu_top_left.y && cast_menu_summons_allowed)
         cast_menu_box = 1;
      else if (y > cast_menu_top_left.y + cast_menu_size.y && cast_menu_summons_allowed)
         cast_menu_box = 3;
   }

   if (cast_menu_box == 1 || cast_menu_box == 3) // Vertical - summons
   {
      int dy;
      if (cast_menu_box == 1)
         dy = cast_menu_top_left.y - y;
      else
         dy = y - (cast_menu_top_left.y + cast_menu_size.y);

      if (dy > (5*cast_menu_button_size))
         cast_menu_selection = 0;
      else if (dy < 0) {
         cast_menu_selection = 0;
         cast_menu_box = 0;
      }
      else
         cast_menu_selection = (dy / cast_menu_button_size) + 1;
      /*
      if (x < cast_menu_top_x_left || 
          x > (cast_menu_top_x_left + cast_menu_button_size_x) ||
          !( (y <= cast_menu_top_y_bottom && 
              y >= cast_menu_top_y_bottom - (cast_menu_button_size_y * 5))
             ||
             (y >= cast_menu_bottom_y_top && 
              y <= cast_menu_bottom_y_top + (cast_menu_button_size_y * 5))
           )
         )
      {
         // Not in a selection area
         cast_menu_selection = 0;
      } 
      else if (y <= cast_menu_top_y_bottom && 
          y >= cast_menu_top_y_bottom - (cast_menu_button_size_y * 5))
      {
         // Top selection area
         cast_menu_selection = ((int) ((y - cast_menu_top_y_bottom) / cast_menu_button_size_y)) - 1;
      }
      else
      {
         // Bottom selection area
         cast_menu_selection = (int) ((y - cast_menu_bottom_y_top) / cast_menu_button_size_y);
      }
      */
   }

   if (cast_menu_box == 2 || cast_menu_box == 4) // Horizontal - spells
   {
      int dx;
      if (cast_menu_box == 4)
         dx = cast_menu_top_left.x - x;
      else
         dx = x - (cast_menu_top_left.x + cast_menu_size.x);

      if (dx > (5*cast_menu_button_size))
         cast_menu_selection = 0;
      else if (dx < 0) {
         cast_menu_selection = 0;
         cast_menu_box = 0;
      }
      else
         cast_menu_selection = (dx / cast_menu_button_size) + 1;
   }


   return cast_menu_selection;
}

void castMenuSolidify()
{ 
   cast_menu_solid = true;
}

void castMenuClose()
{
   cast_menu_open = false;
}

void castMenuChoose()
{
   Order o( SKIP, TRUE, cast_menu_x_grid );
   o.iteration = cast_menu_y_grid;

   if (cast_menu_box == 1 || cast_menu_box == 3)
   {
      switch (cast_menu_selection) {
         case 1:
            o.action = SUMMON_MONSTER;
            player->addOrder( o ); 
            break;
         case 2:
            o.action = SUMMON_SOLDIER;
            player->addOrder( o ); 
            break;
         case 3:
            o.action = SUMMON_WORM;
            player->addOrder( o ); 
            break;
         case 4:
            o.action = SUMMON_BIRD;
            player->addOrder( o ); 
            break;
         case 5:
            o.action = SUMMON_BUG;
            player->addOrder( o ); 
            break;
      }
   }
   else
   {
      switch (cast_menu_selection) {
         case 1:
            o.action = PL_CAST_HEAL;
            player->addOrder( o ); 
            break;
         case 2:
            o.action = PL_CAST_LIGHTNING;
            player->addOrder( o ); 
            break;
         case 3:
            o.action = PL_CAST_TIMELOCK;
            player->addOrder( o ); 
            break;
         case 4:
            o.action = PL_CAST_SCRY;
            player->addOrder( o ); 
            break;
         case 5:
            o.action = PL_CAST_CONFUSION;
            player->addOrder( o ); 
            break;
      }

   }

   castMenuClose();
}

void drawRightClickMenu()
{
   if (cast_menu_open == false) return;
   RenderWindow *gui_window = SFML_GlobalRenderWindow::get();

   if (cast_menu_box == 0)
   {
      // Draw 'summon' vertically and 'cast' horizontally
      Text text;
      text.setFont( *menu_font );
      text.setColor( Color::Black );
      text.setCharacterSize( 16 );

      if (cast_menu_summons_allowed) {
         text.setString( String("Summon") );
         text.setPosition( cast_menu_top_left.x, cast_menu_top_left.y );
         text.setRotation( 270 );
         gui_window->draw( text );

         text.setPosition( cast_menu_top_left.x + cast_menu_size.x, cast_menu_top_left.y + cast_menu_size.y );
         text.setRotation( 90 );
         gui_window->draw( text );
      }

      text.setString( String("Cast") );
      text.setPosition( cast_menu_top_left.x, cast_menu_top_left.y + cast_menu_size.y );
      text.setRotation( 180 );
      gui_window->draw( text );

      text.setPosition( cast_menu_top_left.x + cast_menu_size.x, cast_menu_top_left.y );
      text.setRotation( 0 );
      gui_window->draw( text );

   }
   else if (cast_menu_box == 1 || cast_menu_box == 3)
   {
      int x, y, dy, dim;
      dim = cast_menu_button_size;
      x = cast_menu_grid_left + (cast_menu_grid_size / 2) - (dim / 2);
      if (cast_menu_box == 1) {
         y = cast_menu_grid_top - dim;
         dy = -dim;
      } else {
         y = cast_menu_grid_top + cast_menu_grid_size;
         dy = dim;
      }

      drawOrder( Order( SUMMON_MONSTER ), x, y, dim );
      if (cast_menu_selection == 1) drawSquare( x, y, dim, Color::White );
      y += dy;
      drawOrder( Order( SUMMON_SOLDIER ), x, y, dim );
      if (cast_menu_selection == 2) drawSquare( x, y, dim, Color::White );
      y += dy;
      drawOrder( Order( SUMMON_WORM ), x, y, dim );
      if (cast_menu_selection == 3) drawSquare( x, y, dim, Color::White );
      y += dy;
      drawOrder( Order( SUMMON_BIRD ), x, y, dim );
      if (cast_menu_selection == 4) drawSquare( x, y, dim, Color::White );
      y += dy;
      drawOrder( Order( SUMMON_BUG ), x, y, dim );
      if (cast_menu_selection == 5) drawSquare( x, y, dim, Color::White );

   }
   else if (cast_menu_box == 2 || cast_menu_box == 4)
   {
      int x, y, dx, dim;
      dim = cast_menu_button_size;
      y = cast_menu_grid_top + (cast_menu_grid_size / 2) - (dim / 2);
      if (cast_menu_box == 4) {
         x = cast_menu_grid_left - dim;
         dx = -dim;
      } else {
         x = cast_menu_grid_left + cast_menu_grid_size;
         dx = dim;
      }

      drawOrder( Order( PL_CAST_HEAL ), x, y, dim );
      if (cast_menu_selection == 1) drawSquare( x, y, dim, Color::White );
      x += dx;
      drawOrder( Order( PL_CAST_LIGHTNING ), x, y, dim );
      if (cast_menu_selection == 2) drawSquare( x, y, dim, Color::White );
      x += dx;
      drawOrder( Order( PL_CAST_TIMELOCK ), x, y, dim );
      if (cast_menu_selection == 3) drawSquare( x, y, dim, Color::White );
      x += dx;
      drawOrder( Order( PL_CAST_SCRY ), x, y, dim );
      if (cast_menu_selection == 4) drawSquare( x, y, dim, Color::White );
      x += dx;
      drawOrder( Order( PL_CAST_CONFUSION ), x, y, dim );
      if (cast_menu_selection == 5) drawSquare( x, y, dim, Color::White );

   }

   drawSquare( cast_menu_top_left.x, cast_menu_top_left.y, cast_menu_size.x, Color::White );
}

//////////////////////////////////////////////////////////////////////
// Order Buttons ---

int selection_box_width = 240, selection_box_height = 50;

bool init_level_gui = false;
IMImageButton *b_con_enemy_adjacent,
              *b_con_enemy_ahead,
              *b_con_enemy_in_range,
              *b_con_half_health,
              *b_con_20_health,
              *b_con_blocked_ahead,
              *b_con_clear,
              *b_o_move_forward,
              *b_o_move_backward,
              *b_o_follow_path,
              *b_o_turn_north,
              *b_o_turn_east,
              *b_o_turn_south,
              *b_o_turn_west,
              *b_o_turn_nearest_enemy,
              *b_o_attack_smallest,
              *b_o_attack_biggest,
              *b_o_attack_closest,
              *b_o_attack_farthest,
              *b_o_attack_least_armor,
              *b_o_attack_most_armor,
              *b_o_start_block,
              *b_o_end_block,
              *b_o_repeat,
              *b_o_wait,
              *b_pl_alert_all,
              *b_pl_cmd_go,
              *b_pl_cmd_go_all,
              *b_pl_set_group,
              *b_pl_delay,
              *b_pl_alert_monster,
              *b_pl_cmd_go_monster,
              *b_o_monster_guard,
              *b_o_monster_burst,
              *b_pl_alert_soldier,
              *b_pl_cmd_go_soldier,
              *b_o_soldier_switch_axe,
              *b_o_soldier_switch_spear,
              *b_o_soldier_switch_bow,
              *b_pl_alert_worm,
              *b_pl_cmd_go_worm,
              *b_o_worm_hide,
              *b_o_worm_trail_on,
              *b_o_worm_trail_off,
              *b_pl_alert_bird,
              *b_pl_cmd_go_bird,
              *b_o_bird_fly,
              *b_cmd_bird_shout,
              *b_cmd_bird_quiet,
              *b_pl_alert_bug,
              *b_pl_cmd_go_bug,
              *b_o_bug_meditate,
              *b_o_bug_fireball,
              *b_o_bug_sunder,
              *b_o_bug_heal,
              *b_o_bug_open_wormhole,
              *b_o_bug_close_wormhole,
              *b_monster_image,
              *b_soldier_image,
              *b_worm_image,
              *b_bird_image,
              *b_bug_image,
              *b_count_infinite;
IMTextButton *b_count_1,
             *b_count_2,
             *b_count_3,
             *b_count_4,
             *b_count_5,
             *b_count_6,
             *b_count_7,
             *b_count_8,
             *b_count_9,
             *b_count_0,
             *b_count_reset;
Text *count_text;
string s_1 = "1", s_2 = "2", s_3 = "3", s_4 = "4", s_5 = "5", s_6 = "6", s_7 = "7", s_8 = "8", s_9 = "9", s_0 = "0", s_infinite = "inf", s_reset = "X";
// Gui Boxes
IMEdgeButton *b_numpad_area,
             *b_conditional_area,
             *b_movement_area,
             *b_attack_area,
             *b_control_area,
             *b_pl_cmd_area,
             *b_monster_area,
             *b_soldier_area,
             *b_worm_area,
             *b_bird_area,
             *b_bug_area;
Vector2f b_con_enemy_adjacent_pos,
         b_con_enemy_ahead_pos,
         b_con_enemy_in_range_pos,
         b_con_half_health_pos,
         b_con_20_health_pos,
         b_con_blocked_ahead_pos,
         b_con_clear_pos,
         b_o_move_forward_pos,
         b_o_move_backward_pos,
         b_o_follow_path_pos,
         b_o_turn_north_pos,
         b_o_turn_east_pos,
         b_o_turn_south_pos,
         b_o_turn_west_pos,
         b_o_turn_nearest_enemy_pos,
         b_o_attack_smallest_pos,
         b_o_attack_biggest_pos,
         b_o_attack_closest_pos,
         b_o_attack_farthest_pos,
         b_o_attack_least_armor_pos,
         b_o_attack_most_armor_pos,
         b_o_start_block_pos,
         b_o_end_block_pos,
         b_o_repeat_pos,
         b_o_wait_pos,
         b_pl_alert_all_pos,
         b_pl_cmd_go_pos,
         b_pl_cmd_go_all_pos,
         b_pl_set_group_pos,
         b_pl_delay_pos,
         b_pl_alert_monster_pos,
         b_pl_cmd_go_monster_pos,
         b_o_monster_guard_pos,
         b_o_monster_burst_pos,
         b_pl_alert_soldier_pos,
         b_pl_cmd_go_soldier_pos,
         b_o_soldier_switch_axe_pos,
         b_o_soldier_switch_spear_pos,
         b_o_soldier_switch_bow_pos,
         b_pl_alert_worm_pos,
         b_pl_cmd_go_worm_pos,
         b_o_worm_hide_pos,
         b_o_worm_trail_on_pos,
         b_o_worm_trail_off_pos,
         b_pl_alert_bird_pos,
         b_pl_cmd_go_bird_pos,
         b_o_bird_fly_pos,
         b_cmd_bird_shout_pos,
         b_cmd_bird_quiet_pos,
         b_pl_alert_bug_pos,
         b_pl_cmd_go_bug_pos,
         b_o_bug_meditate_pos,
         b_o_bug_fireball_pos,
         b_o_bug_sunder_pos,
         b_o_bug_heal_pos,
         b_o_bug_open_wormhole_pos,
         b_o_bug_close_wormhole_pos,
         b_count_infinite_pos,
         b_count_1_pos,
         b_count_2_pos,
         b_count_3_pos,
         b_count_4_pos,
         b_count_5_pos,
         b_count_6_pos,
         b_count_7_pos,
         b_count_8_pos,
         b_count_9_pos,
         b_count_0_pos,
         b_count_reset_pos;
// Victory gui --
IMEdgeTextButton *b_victory_back_to_map;
string s_back_to_map = "Back to Map";

const int border = 3;
const int spacer = 2;
int button_size = 0;

bool isMouseOverGui( int x, int y )
{
   int width = config::width(),
       height = config::height();

   if (y > (height - (border + (spacer * 4) + (button_size * 3))))
      return true; // All button areas are this tall

   if (y > (height - ((border * 2) + (spacer * 7) + (button_size * 5)))
         && x < (border + (spacer * 4) + (button_size * 3)))
      return true; // Player command area is taller

   if (selected_unit && (y < selection_box_height) && (x > width - selection_box_width))
      return true; // Over the selected unit area

   return false;
}

void fitGui_Level()
{
   int width = config::width(),
       height = config::height();

   float button_size_f = ((float)width - (32.0 * spacer) - (9.0 * border)) / 22.0;
   button_size = (int) button_size_f;
   float adjustment = 0, adjust_ratio = (button_size_f - button_size);

   int text_size = floor( button_size / 2) + 2;

   int sec_start_numpad = 0,
       sec_start_conditionals = sec_start_numpad + border + (spacer * 4) + (button_size * 3),
       sec_start_control = sec_start_conditionals + border + (spacer * 3) + (button_size * 2),
       sec_start_movement = sec_start_control + border + (spacer * 2) + (button_size * 1),
       sec_start_attack = sec_start_movement + border + (spacer * 4) + (button_size * 3),
       sec_start_monster = sec_start_attack + border + (spacer * 3) + (button_size * 2),
       sec_start_soldier = sec_start_monster + border + (spacer * 3) + (button_size * 2),
       sec_start_worm = sec_start_soldier + border + (spacer * 3) + (button_size * 2),
       sec_start_bird = sec_start_worm + border + (spacer * 3) + (button_size * 2),
       sec_start_bug = sec_start_bird + border + (spacer * 3) + (button_size * 2);

   adjustment += (3 * adjust_ratio);
   sec_start_conditionals += (int) adjustment;
   adjustment += (1 * adjust_ratio);
   sec_start_control += (int) adjustment;
   adjustment += (1 * adjust_ratio);
   sec_start_movement += (int) adjustment;
   adjustment += (3 * adjust_ratio);
   sec_start_attack += (int) adjustment;
   adjustment += (2 * adjust_ratio);
   sec_start_monster += (int) adjustment;
   adjustment += (2 * adjust_ratio);
   sec_start_soldier += (int) adjustment;
   adjustment += (2 * adjust_ratio);
   sec_start_worm += (int) adjustment;
   adjustment += (2 * adjust_ratio);
   sec_start_bird += (int) adjustment;
   adjustment += (2 * adjust_ratio);
   sec_start_bug += (int) adjustment;

   // Numpad

   b_numpad_area->setSize( (border * 3) + (spacer * 4) + (button_size * 3),
                           (border * 2) + (spacer * 5) + (button_size * 4) );
   b_numpad_area->setPosition( sec_start_numpad - (2 * border),
                               height - (border + (spacer * 5) + (button_size * 4)) );
   
   b_count_0->setSize( button_size, button_size );
   b_count_0_pos = Vector2f( sec_start_numpad + (spacer * 2) + button_size,
                           height - (spacer + button_size));
   b_count_0->setPosition( b_count_0_pos.x, b_count_0_pos.y );
   b_count_0->setTextSize( text_size );
   b_count_0->centerText();
   
   b_count_1->setSize( button_size, button_size );
   b_count_1_pos = Vector2f( sec_start_numpad + spacer,
                           height - ((spacer * 4) + (button_size * 4)) );
   b_count_1->setPosition( b_count_1_pos.x, b_count_1_pos.y );
   b_count_1->setTextSize( text_size );
   b_count_1->centerText();

   b_count_2->setSize( button_size, button_size );
   b_count_2_pos = Vector2f( sec_start_numpad + (spacer * 2) + button_size,
                           height - ((spacer * 4) + (button_size * 4)) );
   b_count_2->setPosition( b_count_2_pos.x, b_count_2_pos.y );
   b_count_2->setTextSize( text_size );
   b_count_2->centerText();

   b_count_3->setSize( button_size, button_size );
   b_count_3_pos = Vector2f( sec_start_numpad + (spacer * 3) + (button_size * 2),
                           height - ((spacer * 4) + (button_size * 4)) );
   b_count_3->setPosition( b_count_3_pos.x, b_count_3_pos.y );
   b_count_3->setTextSize( text_size );
   b_count_3->centerText();

   b_count_4->setSize( button_size, button_size );
   b_count_4_pos = Vector2f( sec_start_numpad + spacer,
                           height - ((spacer * 3) + (button_size * 3)) );
   b_count_4->setPosition( b_count_4_pos.x, b_count_4_pos.y );
   b_count_4->setTextSize( text_size );
   b_count_4->centerText();

   b_count_5->setSize( button_size, button_size );
   b_count_5_pos = Vector2f( sec_start_numpad + (spacer * 2) + button_size,
                           height - ((spacer * 3) + (button_size * 3)) );
   b_count_5->setPosition( b_count_5_pos.x, b_count_5_pos.y );
   b_count_5->setTextSize( text_size );
   b_count_5->centerText();

   b_count_6->setSize( button_size, button_size );
   b_count_6_pos = Vector2f( sec_start_numpad + (spacer * 3) + (button_size * 2),
                           height - ((spacer * 3) + (button_size * 3)) );
   b_count_6->setPosition( b_count_6_pos.x, b_count_6_pos.y );
   b_count_6->setTextSize( text_size );
   b_count_6->centerText();

   b_count_7->setSize( button_size, button_size );
   b_count_7_pos = Vector2f( sec_start_numpad + spacer,
                           height - ((spacer * 2) + (button_size * 2)) );
   b_count_7->setPosition( b_count_7_pos.x, b_count_7_pos.y );
   b_count_7->setTextSize( text_size );
   b_count_7->centerText();

   b_count_8->setSize( button_size, button_size );
   b_count_8_pos = Vector2f( sec_start_numpad + (spacer * 2) + button_size,
                           height - ((spacer * 2) + (button_size * 2)) );
   b_count_8->setPosition( b_count_8_pos.x, b_count_8_pos.y );
   b_count_8->setTextSize( text_size );
   b_count_8->centerText();

   b_count_9->setSize( button_size, button_size );
   b_count_9_pos = Vector2f( sec_start_numpad + (spacer * 3) + (button_size * 2),
                           height - ((spacer * 2) + (button_size * 2)) );
   b_count_9->setPosition( b_count_9_pos.x, b_count_9_pos.y );
   b_count_9->setTextSize( text_size );
   b_count_9->centerText();

   b_count_infinite->setSize( button_size, button_size );
   b_count_infinite_pos = Vector2f( sec_start_numpad + (spacer * 3) + (button_size * 2),
                                  height - (spacer + button_size) );
   b_count_infinite->setPosition( b_count_infinite_pos.x, b_count_infinite_pos.y );
   b_count_infinite->setImageSize( button_size, button_size );

   b_count_reset->setSize( button_size, button_size );
   b_count_reset_pos = Vector2f( sec_start_numpad + spacer,
                               height - (spacer + button_size) );
   b_count_reset->setPosition( b_count_reset_pos.x, b_count_reset_pos.y );
   b_count_reset->setTextSize( text_size );
   b_count_reset->centerText();

   count_text->setPosition( (spacer * 2),
                            height - ((border * 2) + (button_size * 5) + (spacer * 9) + text_size));
   count_text->setCharacterSize( text_size );

   // Player commands

   b_pl_cmd_area->setSize( (border * 5) + (spacer * 6) + (button_size * 5),
                           (border * 2) + (spacer * 3) + (button_size * 1));
   b_pl_cmd_area->setPosition( sec_start_numpad - (2 * border),
                               height - ((border * 2) + (spacer * 7) + (button_size * 5)));

   b_pl_alert_all->setSize( button_size, button_size );
   b_pl_alert_all_pos = Vector2f( sec_start_numpad + spacer,
                                height - ((border * 1) + (spacer * 6) + (button_size * 5)));
   b_pl_alert_all->setPosition( b_pl_alert_all_pos.x, b_pl_alert_all_pos.y );

   b_pl_cmd_go->setSize( button_size, button_size );
   b_pl_cmd_go_pos = Vector2f( sec_start_numpad + (spacer * 2) + button_size,
                             height - ((border * 1) + (spacer * 6) + (button_size * 5)));
   b_pl_cmd_go->setPosition( b_pl_cmd_go_pos.x, b_pl_cmd_go_pos.y );

   b_pl_cmd_go_all->setSize( button_size, button_size );
   b_pl_cmd_go_all_pos = Vector2f( sec_start_numpad + (spacer * 3) + (button_size * 2),
                                 height - ((border * 1) + (spacer * 6) + (button_size * 5)));
   b_pl_cmd_go_all->setPosition( b_pl_cmd_go_all_pos.x, b_pl_cmd_go_all_pos.y );

   b_pl_set_group->setSize( button_size, button_size );
   b_pl_set_group_pos = Vector2f( sec_start_numpad + (spacer * 4) + (button_size * 3),
                               height - ((border * 1) + (spacer * 6) + (button_size * 5)));
   b_pl_set_group->setPosition( b_pl_set_group_pos.x, b_pl_set_group_pos.y );

   b_pl_delay->setSize( button_size, button_size );
   b_pl_delay_pos = Vector2f( sec_start_numpad + (spacer * 5) + (button_size * 4),
                            height - ((border * 1) + (spacer * 6) + (button_size * 5)));
   b_pl_delay->setPosition( b_pl_delay_pos.x, b_pl_delay_pos.y );


   // Conditionals

   b_conditional_area->setSize( (border * 3) + (spacer * 3) + (button_size * 2),
                                (border * 2) + (spacer * 5) + (button_size * 4));
   b_conditional_area->setPosition( sec_start_conditionals - (2 * border),
                                    height - (border + (spacer * 5) + (button_size * 4)));

   b_con_enemy_adjacent->setSize( button_size, button_size );
   b_con_enemy_adjacent->setImageSize( button_size, button_size );
   b_con_enemy_adjacent_pos = Vector2f( sec_start_conditionals + spacer,
                                      height - ((spacer * 3) + (button_size * 3)));
   b_con_enemy_adjacent->setPosition( b_con_enemy_adjacent_pos.x, b_con_enemy_adjacent_pos.y );

   b_con_enemy_ahead->setSize( button_size, button_size );
   b_con_enemy_ahead->setImageSize( button_size, button_size );
   b_con_enemy_ahead_pos = Vector2f( sec_start_conditionals + spacer,
                                      height - ((spacer * 2) + (button_size * 2)));
   b_con_enemy_ahead->setPosition( b_con_enemy_ahead_pos.x, b_con_enemy_ahead_pos.y );

   b_con_enemy_in_range->setSize( button_size, button_size );
   b_con_enemy_in_range->setImageSize( button_size, button_size );
   b_con_enemy_in_range_pos = Vector2f( sec_start_conditionals + spacer,
                                      height - ((spacer * 1) + (button_size * 1)));
   b_con_enemy_in_range->setPosition( b_con_enemy_in_range_pos.x, b_con_enemy_in_range_pos.y );

   b_con_blocked_ahead->setSize( button_size, button_size );
   b_con_blocked_ahead->setImageSize( button_size, button_size );
   b_con_blocked_ahead_pos = Vector2f( sec_start_conditionals + (spacer * 2) + button_size,
                                      height - ((spacer * 3) + (button_size * 3)));
   b_con_blocked_ahead->setPosition( b_con_blocked_ahead_pos.x, b_con_blocked_ahead_pos.y );

   b_con_half_health->setSize( button_size, button_size );
   b_con_half_health->setImageSize( button_size, button_size );
   b_con_half_health_pos = Vector2f( sec_start_conditionals + (spacer * 2) + button_size,
                                      height - ((spacer * 2) + (button_size * 2)));
   b_con_half_health->setPosition( b_con_half_health_pos.x, b_con_half_health_pos.y );

   b_con_20_health->setSize( button_size, button_size );
   b_con_20_health->setImageSize( button_size, button_size );
   b_con_20_health_pos = Vector2f( sec_start_conditionals + (spacer * 2) + button_size,
                                      height - ((spacer * 1) + (button_size * 1)));
   b_con_20_health->setPosition( b_con_20_health_pos.x, b_con_20_health_pos.y );

   b_con_clear->setSize( button_size, button_size );
   b_con_clear->setImageSize( button_size, button_size );
   b_con_clear_pos = Vector2f( sec_start_conditionals + spacer,
                                      height - ((spacer * 4) + (button_size * 4)));
   b_con_clear->setPosition( b_con_clear_pos.x, b_con_clear_pos.y );

   // Control

   b_control_area->setSize( (border * 3) + (spacer * 2) + (button_size * 1),
                            (border * 2) + (spacer * 4) + (button_size * 3));
   b_control_area->setPosition( sec_start_control - (2 * border),
                                height - (border + (spacer * 4) + (button_size * 3)));

   b_o_start_block->setSize( button_size, button_size );
   b_o_start_block->setImageSize( button_size, button_size );
   b_o_start_block_pos = Vector2f( sec_start_control + spacer,
                                 height - ((spacer * 3) + (button_size * 3)));
   b_o_start_block->setPosition( b_o_start_block_pos.x, b_o_start_block_pos.y );

   b_o_end_block->setSize( button_size, button_size );
   b_o_end_block->setImageSize( button_size, button_size );
   b_o_end_block_pos = Vector2f( sec_start_control + spacer,
                               height - ((spacer * 2) + (button_size * 2)));
   b_o_end_block->setPosition( b_o_end_block_pos.x, b_o_end_block_pos.y );

   b_o_repeat->setSize( button_size, button_size );
   b_o_repeat->setImageSize( button_size, button_size );
   b_o_repeat_pos = Vector2f( sec_start_control + spacer,
                            height - ((spacer * 1) + (button_size * 1)));
   b_o_repeat->setPosition( b_o_repeat_pos.x, b_o_repeat_pos.y );

   // Movement

   b_movement_area->setSize( (border * 3) + (spacer * 4) + (button_size * 3),
                             (border * 2) + (spacer * 4) + (button_size * 3));
   b_movement_area->setPosition( sec_start_movement - (2 * border),
                                 height - (border + (spacer * 4) + (button_size * 3)));

   b_o_move_forward->setSize( button_size, button_size );
   b_o_move_forward->setImageSize( button_size, button_size );
   b_o_move_forward_pos = Vector2f( sec_start_movement + (spacer * 3) + (button_size * 2),
                                  height - ((spacer * 3) + (button_size * 3)));
   b_o_move_forward->setPosition( b_o_move_forward_pos.x, b_o_move_forward_pos.y );

   b_o_move_backward->setSize( button_size, button_size );
   b_o_move_backward->setImageSize( button_size, button_size );
   b_o_move_backward_pos = Vector2f( sec_start_movement + spacer,
                                   height - ((spacer * 3) + (button_size * 3)));
   b_o_move_backward->setPosition( b_o_move_backward_pos.x, b_o_move_backward_pos.y );

   b_o_wait->setSize( button_size, button_size );
   b_o_wait->setImageSize( button_size, button_size );
   b_o_wait_pos = Vector2f( sec_start_movement + (spacer * 2) + button_size,
                          height - ((spacer * 3) + (button_size * 3)));
   b_o_wait->setPosition( b_o_wait_pos.x, b_o_wait_pos.y );

   b_o_turn_north->setSize( button_size, button_size );
   b_o_turn_north->setImageSize( button_size, button_size );
   b_o_turn_north_pos = Vector2f( sec_start_movement + (spacer * 2) + button_size,
                                      height - ((spacer * 2) + (button_size * 2)));
   b_o_turn_north->setPosition( b_o_turn_north_pos.x, b_o_turn_north_pos.y );

   b_o_turn_east->setSize( button_size, button_size );
   b_o_turn_east->setImageSize( button_size, button_size );
   b_o_turn_east_pos = Vector2f( sec_start_movement + (spacer * 3) + (button_size * 2),
                                     height - (spacer + (button_size * 1)));
   b_o_turn_east->setPosition( b_o_turn_east_pos.x, b_o_turn_east_pos.y );

   b_o_turn_south->setSize( button_size, button_size );
   b_o_turn_south->setImageSize( button_size, button_size );
   b_o_turn_south_pos = Vector2f( sec_start_movement + (spacer * 2) + button_size,
                                      height - (spacer + (button_size * 1)));
   b_o_turn_south->setPosition( b_o_turn_south_pos.x, b_o_turn_south_pos. y );

   b_o_turn_west->setSize( button_size, button_size );
   b_o_turn_west->setImageSize( button_size, button_size );
   b_o_turn_west_pos = Vector2f( sec_start_movement + spacer,
                                     height - (spacer + (button_size * 1)));
   b_o_turn_west->setPosition( b_o_turn_west_pos.x, b_o_turn_west_pos.y );

   b_o_turn_nearest_enemy->setSize( button_size, button_size );
   b_o_turn_nearest_enemy->setImageSize( button_size, button_size );
   b_o_turn_nearest_enemy_pos = Vector2f( sec_start_movement + spacer,
                                     height - ((spacer * 2) + (button_size * 2)));
   b_o_turn_nearest_enemy->setPosition( b_o_turn_nearest_enemy_pos.x, b_o_turn_nearest_enemy_pos.y );

   b_o_follow_path->setSize( button_size, button_size );
   b_o_follow_path->setImageSize( button_size, button_size );
   b_o_follow_path_pos = Vector2f( sec_start_movement + (spacer * 3) + (button_size * 2),
                                 height - ((spacer * 2) + (button_size * 2)));
   b_o_follow_path->setPosition( b_o_follow_path_pos.x, b_o_follow_path_pos.y );


   // Attack

   b_attack_area->setSize( (border * 3) + (spacer * 3) + (button_size * 2),
                           (border * 2) + (spacer * 4) + (button_size * 3));
   b_attack_area->setPosition( sec_start_attack - (2 * border),
                               height - (border + (spacer * 4) + (button_size * 3)));

   b_o_attack_smallest->setSize( button_size, button_size );
   b_o_attack_smallest->setImageSize( button_size, button_size );
   b_o_attack_smallest_pos = Vector2f( sec_start_attack + spacer,
                                     height - ((spacer * 3) + (button_size * 3)));
   b_o_attack_smallest->setPosition( b_o_attack_smallest_pos.x, b_o_attack_smallest_pos.y );

   b_o_attack_biggest->setSize( button_size, button_size );
   b_o_attack_biggest->setImageSize( button_size, button_size );
   b_o_attack_biggest_pos = Vector2f( sec_start_attack + (spacer * 2) + button_size,
                                    height - ((spacer * 3) + (button_size * 3)));
   b_o_attack_biggest->setPosition( b_o_attack_biggest_pos.x, b_o_attack_biggest_pos.y );

   b_o_attack_closest->setSize( button_size, button_size );
   b_o_attack_closest->setImageSize( button_size, button_size );
   b_o_attack_closest_pos = Vector2f( sec_start_attack + spacer,
                                    height - ((spacer * 2) + (button_size * 2)));
   b_o_attack_closest->setPosition( b_o_attack_closest_pos.x, b_o_attack_closest_pos.y );

   b_o_attack_farthest->setSize( button_size, button_size );
   b_o_attack_farthest->setImageSize( button_size, button_size );
   b_o_attack_farthest_pos = Vector2f( sec_start_attack + (spacer * 2) + button_size,
                                     height - ((spacer * 2) + (button_size * 2)));
   b_o_attack_farthest->setPosition( b_o_attack_farthest_pos.x, b_o_attack_farthest_pos.y );

   b_o_attack_least_armor->setSize( button_size, button_size );
   b_o_attack_least_armor->setImageSize( button_size, button_size );
   b_o_attack_least_armor_pos = Vector2f( sec_start_attack + spacer,
                                        height - (spacer + (button_size * 1)));
   b_o_attack_least_armor->setPosition( b_o_attack_least_armor_pos.x, b_o_attack_least_armor_pos.y );

   b_o_attack_most_armor->setSize( button_size, button_size );
   b_o_attack_most_armor->setImageSize( button_size, button_size );
   b_o_attack_most_armor_pos = Vector2f( sec_start_attack + (spacer * 2) + button_size,
                                       height - (spacer + (button_size * 1)));
   b_o_attack_most_armor->setPosition( b_o_attack_most_armor_pos.x, b_o_attack_most_armor_pos.y );

   // Units

   // Monster
   b_monster_area->setSize( (border * 3) + (spacer * 3) + (button_size * 2),
                            (border * 2) + (spacer * 4) + (button_size * 3));
   b_monster_area->setPosition( sec_start_monster - (2 * border),
                                height - (border + (spacer * 4) + (button_size * 3)));

   b_pl_alert_monster->setSize( button_size, button_size );
   b_pl_alert_monster->setImageSize( button_size, button_size );
   b_pl_alert_monster_pos = Vector2f( sec_start_monster + spacer,
                                    height - ((spacer * 3) + (button_size * 3)) );
   b_pl_alert_monster->setPosition( b_pl_alert_monster_pos.x, b_pl_alert_monster_pos.y );

   b_pl_cmd_go_monster->setSize( button_size, button_size );
   b_pl_cmd_go_monster->setImageSize( button_size, button_size );
   b_pl_cmd_go_monster_pos = Vector2f( sec_start_monster + (spacer * 2) + button_size,
                                     height - ((spacer * 3) + (button_size * 3)) );
   b_pl_cmd_go_monster->setPosition( b_pl_cmd_go_monster_pos.x, b_pl_cmd_go_monster_pos.y );

   b_o_monster_guard->setSize( button_size, button_size );
   b_o_monster_guard->setImageSize( button_size, button_size );
   b_o_monster_guard_pos = Vector2f( sec_start_monster + spacer,
                                 height - ((spacer * 2) + (button_size * 2)) );
   b_o_monster_guard->setPosition( b_o_monster_guard_pos.x, b_o_monster_guard_pos.y );

   b_o_monster_burst->setSize( button_size, button_size );
   b_o_monster_burst->setImageSize( button_size, button_size );
   b_o_monster_burst_pos = Vector2f( sec_start_monster + spacer,
                                   height - (spacer + button_size) );
   b_o_monster_burst->setPosition( b_o_monster_burst_pos.x, b_o_monster_burst_pos.y );

   b_monster_image->setSize( button_size, button_size );
   b_monster_image->setImageSize( button_size, button_size );
   b_monster_image->setPosition( sec_start_monster + (spacer * 3) + button_size,
                                 height - (button_size) );

   // Soldier
   b_soldier_area->setSize( (border * 3) + (spacer * 3) + (button_size * 2),
                            (border * 2) + (spacer * 4) + (button_size * 3));
   b_soldier_area->setPosition( sec_start_soldier - (2 * border),
                                height - (border + (spacer * 4) + (button_size * 3)));

   b_pl_alert_soldier->setSize( button_size, button_size );
   b_pl_alert_soldier->setImageSize( button_size, button_size );
   b_pl_alert_soldier_pos = Vector2f( sec_start_soldier + spacer,
                                    height - ((spacer * 3) + (button_size * 3)) );
   b_pl_alert_soldier->setPosition( b_pl_alert_soldier_pos.x, b_pl_alert_soldier_pos.y );

   b_pl_cmd_go_soldier->setSize( button_size, button_size );
   b_pl_cmd_go_soldier->setImageSize( button_size, button_size );
   b_pl_cmd_go_soldier_pos = Vector2f( sec_start_soldier + (spacer * 2) + button_size,
                                     height - ((spacer * 3) + (button_size * 3)) );
   b_pl_cmd_go_soldier->setPosition( b_pl_cmd_go_soldier_pos.x, b_pl_cmd_go_soldier_pos.y );

   b_o_soldier_switch_axe->setSize( button_size, button_size );
   b_o_soldier_switch_axe->setImageSize( button_size, button_size );
   b_o_soldier_switch_axe_pos = Vector2f( sec_start_soldier + spacer,
                                      height - ((spacer * 2) + (button_size * 2)) );
   b_o_soldier_switch_axe->setPosition( b_o_soldier_switch_axe_pos.x, b_o_soldier_switch_axe_pos.y );

   b_o_soldier_switch_spear->setSize( button_size, button_size );
   b_o_soldier_switch_spear->setImageSize( button_size, button_size );
   b_o_soldier_switch_spear_pos = Vector2f( sec_start_soldier + (spacer * 2) + button_size,
                                        height - ((spacer * 2) + (button_size * 2)) );
   b_o_soldier_switch_spear->setPosition( b_o_soldier_switch_spear_pos.x, b_o_soldier_switch_spear_pos.y );
   
   b_o_soldier_switch_bow->setSize( button_size, button_size );
   b_o_soldier_switch_bow->setImageSize( button_size, button_size );
   b_o_soldier_switch_bow_pos = Vector2f( sec_start_soldier + spacer,
                                      height - (spacer + button_size ) );
   b_o_soldier_switch_bow->setPosition( b_o_soldier_switch_bow_pos.x, b_o_soldier_switch_bow_pos.y );

   b_soldier_image->setSize( button_size, button_size );
   b_soldier_image->setImageSize( button_size, button_size );
   b_soldier_image->setPosition( sec_start_soldier + (spacer * 3) + button_size,
                                 height - (button_size) );

   // Worm
   b_worm_area->setSize( (border * 3) + (spacer * 3) + (button_size * 2),
                         (border * 2) + (spacer * 4) + (button_size * 3));
   b_worm_area->setPosition( sec_start_worm - (2 * border),
                             height - (border + (spacer * 4) + (button_size * 3)));

   b_pl_alert_worm->setSize( button_size, button_size );
   b_pl_alert_worm->setImageSize( button_size, button_size );
   b_pl_alert_worm_pos = Vector2f( sec_start_worm + spacer,
                                 height - ((spacer * 3) + (button_size * 3)) );
   b_pl_alert_worm->setPosition( b_pl_alert_worm_pos.x, b_pl_alert_worm_pos.y );

   b_pl_cmd_go_worm->setSize( button_size, button_size );
   b_pl_cmd_go_worm->setImageSize( button_size, button_size );
   b_pl_cmd_go_worm_pos = Vector2f( sec_start_worm + (spacer * 2) + button_size,
                                  height - ((spacer * 3) + (button_size * 3)) );
   b_pl_cmd_go_worm->setPosition( b_pl_cmd_go_worm_pos.x, b_pl_cmd_go_worm_pos.y );

   b_o_worm_trail_on->setSize( button_size, button_size );
   b_o_worm_trail_on->setImageSize( button_size, button_size );
   b_o_worm_trail_on_pos = Vector2f( sec_start_worm + spacer,
                                    height - ((spacer * 2) + (button_size * 2)) );
   b_o_worm_trail_on->setPosition( b_o_worm_trail_on_pos.x, b_o_worm_trail_on_pos.y );

   b_o_worm_trail_off->setSize( button_size, button_size );
   b_o_worm_trail_off->setImageSize( button_size, button_size );
   b_o_worm_trail_off_pos = Vector2f( sec_start_worm + (spacer * 2) + button_size,
                                     height - ((spacer * 2) + (button_size * 2)) );
   b_o_worm_trail_off->setPosition( b_o_worm_trail_off_pos.x, b_o_worm_trail_off_pos.y );

   b_o_worm_hide->setSize( button_size, button_size );
   b_o_worm_hide->setImageSize( button_size, button_size );
   b_o_worm_hide_pos = Vector2f( sec_start_worm + spacer,
                                 height - (spacer + button_size ) );
   b_o_worm_hide->setPosition( b_o_worm_hide_pos.x, b_o_worm_hide_pos.y );

   b_worm_image->setSize( button_size, button_size );
   b_worm_image->setImageSize( button_size, button_size );
   b_worm_image->setPosition( sec_start_worm + (spacer * 3) + button_size,
                              height - (button_size) );

   // Bird
   b_bird_area->setSize( (border * 3) + (spacer * 3) + (button_size * 2),
                         (border * 2) + (spacer * 4) + (button_size * 3));
   b_bird_area->setPosition( sec_start_bird - (2 * border),
                             height - (border + (spacer * 4) + (button_size * 3)));

   b_pl_alert_bird->setSize( button_size, button_size );
   b_pl_alert_bird->setImageSize( button_size, button_size );
   b_pl_alert_bird_pos = Vector2f( sec_start_bird + spacer,
                                 height - ((spacer * 3) + (button_size * 3)) );
   b_pl_alert_bird->setPosition( b_pl_alert_bird_pos.x, b_pl_alert_bird_pos.y );

   b_pl_cmd_go_bird->setSize( button_size, button_size );
   b_pl_cmd_go_bird->setImageSize( button_size, button_size );
   b_pl_cmd_go_bird_pos = Vector2f( sec_start_bird + (spacer * 2) + button_size,
                                  height - ((spacer * 3) + (button_size * 3)) );
   b_pl_cmd_go_bird->setPosition( b_pl_cmd_go_bird_pos.x, b_pl_cmd_go_bird_pos.y );

   b_cmd_bird_shout->setSize( button_size, button_size );
   b_cmd_bird_shout->setImageSize( button_size, button_size );
   b_cmd_bird_shout_pos = Vector2f( sec_start_bird + spacer,
                                height - ((spacer * 2) + (button_size * 2)) );
   b_cmd_bird_shout->setPosition( b_cmd_bird_shout_pos.x, b_cmd_bird_shout_pos.y );

   b_cmd_bird_quiet->setSize( button_size, button_size );
   b_cmd_bird_quiet->setImageSize( button_size, button_size );
   b_cmd_bird_quiet_pos = Vector2f( sec_start_bird + (spacer * 2) + button_size,
                                height - ((spacer * 2) + (button_size * 2)) );
   b_cmd_bird_quiet->setPosition( b_cmd_bird_quiet_pos.x, b_cmd_bird_quiet_pos.y );

   b_o_bird_fly->setSize( button_size, button_size );
   b_o_bird_fly->setImageSize( button_size, button_size );
   b_o_bird_fly_pos = Vector2f( sec_start_bird + spacer,
                               height - (spacer + button_size) );
   b_o_bird_fly->setPosition( b_o_bird_fly_pos.x, b_o_bird_fly_pos.y );

   b_bird_image->setSize( button_size, button_size );
   b_bird_image->setImageSize( button_size, button_size );
   b_bird_image->setPosition( sec_start_bird + (spacer * 3) + button_size,
                              height - (button_size) );

   // Bug
   b_bug_area->setSize( (border * 3) + (spacer * 6) + (button_size * 3),
                        (border * 2) + (spacer * 4) + (button_size * 3));
   b_bug_area->setPosition( sec_start_bug - (2 * border),
                            height - (border + (spacer * 4) + (button_size * 3)));

   b_pl_alert_bug->setSize( button_size, button_size );
   b_pl_alert_bug->setImageSize( button_size, button_size );
   b_pl_alert_bug_pos = Vector2f( sec_start_bug + spacer,
                                height - ((spacer * 3) + (button_size * 3)) );
   b_pl_alert_bug->setPosition( b_pl_alert_bug_pos.x, b_pl_alert_bug_pos.y );

   b_pl_cmd_go_bug->setSize( button_size, button_size );
   b_pl_cmd_go_bug->setImageSize( button_size, button_size );
   b_pl_cmd_go_bug_pos = Vector2f( sec_start_bug + (spacer * 2) + button_size,
                                 height - ((spacer * 3) + (button_size * 3)) );
   b_pl_cmd_go_bug->setPosition( b_pl_cmd_go_bug_pos.x, b_pl_cmd_go_bug_pos.y );

   b_o_bug_fireball->setSize( button_size, button_size );
   b_o_bug_fireball->setImageSize( button_size, button_size );
   b_o_bug_fireball_pos = Vector2f( sec_start_bug + spacer,
                                  height - ((spacer * 2) + (button_size * 2)) );
   b_o_bug_fireball->setPosition( b_o_bug_fireball_pos.x, b_o_bug_fireball_pos.y );

   b_o_bug_sunder->setSize( button_size, button_size );
   b_o_bug_sunder->setImageSize( button_size, button_size );
   b_o_bug_sunder_pos = Vector2f( sec_start_bug + (spacer * 2) + button_size,
                                height - ((spacer * 2) + (button_size * 2)) );
   b_o_bug_sunder->setPosition( b_o_bug_sunder_pos.x, b_o_bug_sunder_pos.y );

   b_o_bug_heal->setSize( button_size, button_size );
   b_o_bug_heal->setImageSize( button_size, button_size );
   b_o_bug_heal_pos = Vector2f( sec_start_bug + (spacer * 3) + (button_size * 2),
                              height - ((spacer * 2) + (button_size * 2)) );
   b_o_bug_heal->setPosition( b_o_bug_heal_pos.x, b_o_bug_heal_pos.y );

   b_o_bug_open_wormhole->setSize( button_size, button_size );
   b_o_bug_open_wormhole->setImageSize( button_size, button_size );
   b_o_bug_open_wormhole_pos = Vector2f( sec_start_bug + spacer,
                                        height - (spacer + button_size ) );
   b_o_bug_open_wormhole->setPosition( b_o_bug_open_wormhole_pos.x, b_o_bug_open_wormhole_pos.y );

   b_o_bug_close_wormhole->setSize( button_size, button_size );
   b_o_bug_close_wormhole->setImageSize( button_size, button_size );
   b_o_bug_close_wormhole_pos = Vector2f( sec_start_bug + (spacer * 2) + button_size,
                                        height - (spacer + button_size ) );
   b_o_bug_close_wormhole->setPosition( b_o_bug_close_wormhole_pos.x, b_o_bug_close_wormhole_pos.y );

   b_o_bug_meditate->setSize( button_size, button_size );
   b_o_bug_meditate->setImageSize( button_size, button_size );
   b_o_bug_meditate_pos = Vector2f( sec_start_bug + (spacer * 3) + (button_size * 2),
                                    height - ((spacer * 3) + (button_size * 3) ) );
   b_o_bug_meditate->setPosition( b_o_bug_meditate_pos.x, b_o_bug_meditate_pos.y );

   b_bug_image->setSize( button_size, button_size );
   b_bug_image->setImageSize( button_size, button_size );
   b_bug_image->setPosition( width - button_size, height - button_size );

   // Victory Gui --
   b_victory_back_to_map->setSize( button_size_f * 5, button_size_f );
   b_victory_back_to_map->setPosition( (width / 2) - (button_size_f * 2.5), (height / 2) + (button_size_f * 1.0 ));
   b_victory_back_to_map->setTextSize( text_size );
   b_victory_back_to_map->centerText();

   view_rel_x_to_y = ((float)height) / ((float)width);
}

int initLevelGui()
{
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();
   IMGuiManager &gui_manager = IMGuiManager::getSingleton();

   b_con_enemy_adjacent = new IMImageButton();
   b_con_enemy_adjacent->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_enemy_adjacent->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_enemy_adjacent->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   b_con_enemy_adjacent->setImage( t_manager.getTexture( "ConditionalEnemyAdjacent.png" ) );
   b_con_enemy_adjacent->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Condition: enemy adjacent", b_con_enemy_adjacent);

   b_con_enemy_ahead = new IMImageButton();
   b_con_enemy_ahead->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_enemy_ahead->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_enemy_ahead->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   b_con_enemy_ahead->setImage( t_manager.getTexture( "ConditionalEnemyAhead.png" ) );
   b_con_enemy_ahead->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Condition: enemy ahead", b_con_enemy_ahead);

   b_con_enemy_in_range = new IMImageButton();
   b_con_enemy_in_range->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_enemy_in_range->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_enemy_in_range->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   b_con_enemy_in_range->setImage( t_manager.getTexture( "ConditionalEnemyInRange.png" ) );
   b_con_enemy_in_range->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Condition: enemy in range", b_con_enemy_in_range);
/*
   b_con_ally_adjacent = new IMImageButton();
   b_con_ally_adjacent->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_ally_adjacent->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_ally_adjacent->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   b_con_ally_adjacent->setImage( t_manager.getTexture( "ConditionalAllyAdjacent.png" ) );
   b_con_ally_adjacent->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Condition: ally adjacent", b_con_ally_adjacent);

   b_con_ally_ahead = new IMImageButton();
   b_con_ally_ahead->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_ally_ahead->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_ally_ahead->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   b_con_ally_ahead->setImage( t_manager.getTexture( "ConditionalAllyAhead.png" ) );
   b_con_ally_ahead->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Condition: ally ahead", b_con_ally_ahead);

   b_con_ally_in_range = new IMImageButton();
   b_con_ally_in_range->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_ally_in_range->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_ally_in_range->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   b_con_ally_in_range->setImage( t_manager.getTexture( "ConditionalAllyInRange.png" ) );
   b_con_ally_in_range->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Condition: ally in range", b_con_ally_in_range);
   */

   b_con_half_health = new IMImageButton();
   b_con_half_health->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_half_health->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_half_health->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   b_con_half_health->setImage( t_manager.getTexture( "ConditionalHalfHealth.png" ) );
   b_con_half_health->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Condition: half health", b_con_half_health);

   b_con_20_health = new IMImageButton();
   b_con_20_health->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_20_health->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_20_health->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   b_con_20_health->setImage( t_manager.getTexture( "Conditional20Health.png" ) );
   b_con_20_health->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Condition: 20 percent health", b_con_20_health);

   b_con_blocked_ahead = new IMImageButton();
   b_con_blocked_ahead->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_blocked_ahead->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_blocked_ahead->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   b_con_blocked_ahead->setImage( t_manager.getTexture( "ConditionalBlockedAhead.png" ) );
   b_con_blocked_ahead->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Condition: blocked ahead", b_con_blocked_ahead);

   b_con_clear = new IMImageButton();
   b_con_clear->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_clear->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_clear->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   b_con_clear->setImage( t_manager.getTexture( "ConditionalClear.png" ) );
   b_con_clear->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Clear conditionals", b_con_clear);

   b_o_start_block = new IMImageButton();
   b_o_start_block->setNormalTexture( t_manager.getTexture( "ControlButtonBase.png" ) );
   b_o_start_block->setPressedTexture( t_manager.getTexture( "ControlButtonPressed.png" ) );
   b_o_start_block->setHoverTexture( t_manager.getTexture( "ControlButtonHover.png" ) );
   b_o_start_block->setImage(  t_manager.getTexture( "ControlStartBlock.png" ) );
   b_o_start_block->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Control: start block", b_o_start_block);

   b_o_end_block = new IMImageButton();
   b_o_end_block->setNormalTexture( t_manager.getTexture( "ControlButtonBase.png" ) );
   b_o_end_block->setPressedTexture( t_manager.getTexture( "ControlButtonPressed.png" ) );
   b_o_end_block->setHoverTexture( t_manager.getTexture( "ControlButtonHover.png" ) );
   b_o_end_block->setImage(  t_manager.getTexture( "ControlEndBlock.png" ) );
   b_o_end_block->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Control: end block", b_o_end_block);

   b_o_repeat = new IMImageButton();
   b_o_repeat->setNormalTexture( t_manager.getTexture( "ControlButtonBase.png" ) );
   b_o_repeat->setPressedTexture( t_manager.getTexture( "ControlButtonPressed.png" ) );
   b_o_repeat->setHoverTexture( t_manager.getTexture( "ControlButtonHover.png" ) );
   b_o_repeat->setImage(  t_manager.getTexture( "ControlRepeat.png" ) );
   b_o_repeat->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Control: repeat", b_o_repeat);

   b_o_wait = new IMImageButton();
   b_o_wait->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_wait->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_wait->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_wait->setImage(  t_manager.getTexture( "OrderWait.png" ) );
   b_o_wait->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: wait", b_o_wait);

   b_o_move_forward = new IMImageButton();
   b_o_move_forward->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_move_forward->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_move_forward->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_move_forward->setImage( t_manager.getTexture( "OrderMoveForward.png" ) );
   b_o_move_forward->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: move forward", b_o_move_forward);

   b_o_move_backward = new IMImageButton();
   b_o_move_backward->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_move_backward->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_move_backward->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_move_backward->setImage( t_manager.getTexture( "OrderMoveBackward.png" )  );
   b_o_move_backward->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: move backward", b_o_move_backward);

   b_o_turn_north = new IMImageButton();
   b_o_turn_north->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_turn_north->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_turn_north->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_turn_north->setImage( t_manager.getTexture( "OrderTurnNorth.png" )  );
   b_o_turn_north->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: turn north", b_o_turn_north);

   b_o_turn_east = new IMImageButton();
   b_o_turn_east->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_turn_east->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_turn_east->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_turn_east->setImage(  t_manager.getTexture( "OrderTurnEast.png" ) );
   b_o_turn_east->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: turn east", b_o_turn_east);

   b_o_turn_south = new IMImageButton();
   b_o_turn_south->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_turn_south->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_turn_south->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_turn_south->setImage(  t_manager.getTexture( "OrderTurnSouth.png" ) );
   b_o_turn_south->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: turn south", b_o_turn_south);

   b_o_turn_west = new IMImageButton();
   b_o_turn_west->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_turn_west->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_turn_west->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_turn_west->setImage(  t_manager.getTexture( "OrderTurnWest.png" ) );
   b_o_turn_west->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: turn west", b_o_turn_west);

   b_o_turn_nearest_enemy = new IMImageButton();
   b_o_turn_nearest_enemy->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_turn_nearest_enemy->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_turn_nearest_enemy->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_turn_nearest_enemy->setImage(  t_manager.getTexture( "OrderTurnNearestEnemy.png" ) );
   b_o_turn_nearest_enemy->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: turn nearest enemy", b_o_turn_nearest_enemy);

   b_o_follow_path = new IMImageButton();
   b_o_follow_path->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_follow_path->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_follow_path->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_follow_path->setImage(  t_manager.getTexture( "OrderFollowPath.png" ) );
   b_o_follow_path->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: turn nearest enemy", b_o_follow_path);

   b_o_attack_smallest = new IMImageButton();
   b_o_attack_smallest->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_attack_smallest->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_attack_smallest->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_attack_smallest->setImage(  t_manager.getTexture( "OrderAttackSmallest.png" ) );
   b_o_attack_smallest->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: attack smallest", b_o_attack_smallest);

   b_o_attack_biggest = new IMImageButton();
   b_o_attack_biggest->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_attack_biggest->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_attack_biggest->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_attack_biggest->setImage(  t_manager.getTexture( "OrderAttackBiggest.png" ) );
   b_o_attack_biggest->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: attack biggest", b_o_attack_biggest);

   b_o_attack_closest = new IMImageButton();
   b_o_attack_closest->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_attack_closest->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_attack_closest->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_attack_closest->setImage(  t_manager.getTexture( "OrderAttackClosest.png" ) );
   b_o_attack_closest->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: attack closest", b_o_attack_closest);

   b_o_attack_farthest = new IMImageButton();
   b_o_attack_farthest->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_attack_farthest->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_attack_farthest->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_attack_farthest->setImage(  t_manager.getTexture( "OrderAttackFarthest.png" ) );
   b_o_attack_farthest->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: attack farthest", b_o_attack_farthest);

   b_o_attack_most_armor = new IMImageButton();
   b_o_attack_most_armor->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_attack_most_armor->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_attack_most_armor->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_attack_most_armor->setImage(  t_manager.getTexture( "OrderAttackMostArmored.png" ) );
   b_o_attack_most_armor->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: attack most armored", b_o_attack_most_armor);

   b_o_attack_least_armor = new IMImageButton();
   b_o_attack_least_armor->setNormalTexture( t_manager.getTexture( "OrderButtonBase.png" ) );
   b_o_attack_least_armor->setPressedTexture( t_manager.getTexture( "OrderButtonPressed.png" ) );
   b_o_attack_least_armor->setHoverTexture( t_manager.getTexture( "OrderButtonHover.png" ) );
   b_o_attack_least_armor->setImage(  t_manager.getTexture( "OrderAttackLeastArmored.png" ) );
   b_o_attack_least_armor->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Order: attack least armored", b_o_attack_least_armor);

   b_pl_alert_all = new IMImageButton();
   b_pl_alert_all->setAllTextures( t_manager.getTexture( "PlayerAlert.png" ) );
   b_pl_alert_all->setImage( NULL );
   b_pl_alert_all->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Player: alert all", b_pl_alert_all);

   b_pl_cmd_go = new IMImageButton();
   b_pl_cmd_go->setAllTextures( t_manager.getTexture( "PlayerGoButton.png" ) );
   b_pl_cmd_go->setImage( NULL );
   b_pl_cmd_go->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Player: command go", b_pl_cmd_go);

   b_pl_cmd_go_all = new IMImageButton();
   b_pl_cmd_go_all->setAllTextures( t_manager.getTexture( "PlayerGoAllButton.png" ) );
   b_pl_cmd_go_all->setImage( NULL );
   b_pl_cmd_go_all->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Player: command go all", b_pl_cmd_go_all);

   b_pl_set_group = new IMImageButton();
   b_pl_set_group->setAllTextures( t_manager.getTexture( "SetGroup.png" ) );
   b_pl_set_group->setImage( NULL );
   b_pl_set_group->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Player: set team", b_pl_set_group);

   b_pl_delay = new IMImageButton();
   b_pl_delay->setAllTextures( t_manager.getTexture( "OrderWait.png" ) );
   b_pl_delay->setImage( NULL );
   b_pl_delay->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Player: delay", b_pl_delay);

   b_pl_alert_monster = new IMImageButton();
   b_pl_alert_monster->setAllTextures( t_manager.getTexture( "MonsterAlertButton.png" ) );
   b_pl_alert_monster->setImage( NULL );
   b_pl_alert_monster->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Alert Monster", b_pl_alert_monster);

   b_pl_cmd_go_monster = new IMImageButton();
   b_pl_cmd_go_monster->setAllTextures( t_manager.getTexture( "MonsterGoButton.png" ) );
   b_pl_cmd_go_monster->setImage( NULL );
   b_pl_cmd_go_monster->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Go Monster", b_pl_cmd_go_monster);

   b_o_monster_guard = new IMImageButton();
   b_o_monster_guard->setNormalTexture( t_manager.getTexture( "MonsterOrderButtonBase.png" ) );
   b_o_monster_guard->setPressedTexture( t_manager.getTexture( "MonsterButtonPressed.png" ) );
   b_o_monster_guard->setHoverTexture( t_manager.getTexture( "MonsterButtonHover.png" ) );
   b_o_monster_guard->setImage( t_manager.getTexture( "MonsterOrderGuard.png" ) );
   b_o_monster_guard->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Monster: Guard", b_o_monster_guard);

   b_o_monster_burst = new IMImageButton();
   b_o_monster_burst->setNormalTexture( t_manager.getTexture( "MonsterOrderButtonBase.png" ) );
   b_o_monster_burst->setPressedTexture( t_manager.getTexture( "MonsterButtonPressed.png" ) );
   b_o_monster_burst->setHoverTexture( t_manager.getTexture( "MonsterButtonHover.png" ) );
   b_o_monster_burst->setImage( t_manager.getTexture( "MonsterOrderBurst.png" ) );
   b_o_monster_burst->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Monster: Burst", b_o_monster_burst);


   b_pl_alert_soldier = new IMImageButton();
   b_pl_alert_soldier->setAllTextures( t_manager.getTexture( "SoldierAlertButton.png" ) );
   b_pl_alert_soldier->setImage( NULL );
   b_pl_alert_soldier->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Alert Soldier", b_pl_alert_soldier);

   b_pl_cmd_go_soldier = new IMImageButton();
   b_pl_cmd_go_soldier->setAllTextures( t_manager.getTexture( "SoldierGoButton.png" ) );
   b_pl_cmd_go_soldier->setImage( NULL );
   b_pl_cmd_go_soldier->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Go Soldier", b_pl_cmd_go_soldier);

   b_o_soldier_switch_axe = new IMImageButton();
   b_o_soldier_switch_axe->setNormalTexture( t_manager.getTexture( "SoldierOrderButtonBase.png" ) );
   b_o_soldier_switch_axe->setPressedTexture( t_manager.getTexture( "SoldierButtonPressed.png" ) );
   b_o_soldier_switch_axe->setHoverTexture( t_manager.getTexture( "SoldierButtonHover.png" ) );
   b_o_soldier_switch_axe->setImage( t_manager.getTexture( "SoldierOrderSwitchAxe.png" ) );
   b_o_soldier_switch_axe->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Soldier: Switch Axe", b_o_soldier_switch_axe);

   b_o_soldier_switch_spear = new IMImageButton();
   b_o_soldier_switch_spear->setNormalTexture( t_manager.getTexture( "SoldierOrderButtonBase.png" ) );
   b_o_soldier_switch_spear->setPressedTexture( t_manager.getTexture( "SoldierButtonPressed.png" ) );
   b_o_soldier_switch_spear->setHoverTexture( t_manager.getTexture( "SoldierButtonHover.png" ) );
   b_o_soldier_switch_spear->setImage( t_manager.getTexture( "SoldierOrderSwitchSpear.png" ) );
   b_o_soldier_switch_spear->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Soldier: Switch Spear", b_o_soldier_switch_spear);

   b_o_soldier_switch_bow = new IMImageButton();
   b_o_soldier_switch_bow->setNormalTexture( t_manager.getTexture( "SoldierOrderButtonBase.png" ) );
   b_o_soldier_switch_bow->setPressedTexture( t_manager.getTexture( "SoldierButtonPressed.png" ) );
   b_o_soldier_switch_bow->setHoverTexture( t_manager.getTexture( "SoldierButtonHover.png" ) );
   b_o_soldier_switch_bow->setImage( t_manager.getTexture( "SoldierOrderSwitchBow.png" ) );
   b_o_soldier_switch_bow->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Soldier: Switch Bow", b_o_soldier_switch_bow);

   b_pl_alert_worm = new IMImageButton();
   b_pl_alert_worm->setAllTextures( t_manager.getTexture( "WormAlertButton.png" ) );
   b_pl_alert_worm->setImage( NULL );
   b_pl_alert_worm->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Alert Worm", b_pl_alert_worm);

   b_pl_cmd_go_worm = new IMImageButton();
   b_pl_cmd_go_worm->setAllTextures( t_manager.getTexture( "WormGoButton.png" ) );
   b_pl_cmd_go_worm->setImage( NULL );
   b_pl_cmd_go_worm->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Go Worm", b_pl_cmd_go_worm);

   b_o_worm_hide = new IMImageButton();
   b_o_worm_hide->setNormalTexture( t_manager.getTexture( "WormOrderButtonBase.png" ) );
   b_o_worm_hide->setPressedTexture( t_manager.getTexture( "WormButtonPressed.png" ) );
   b_o_worm_hide->setHoverTexture( t_manager.getTexture( "WormButtonHover.png" ) );
   b_o_worm_hide->setImage( t_manager.getTexture( "WormOrderHide.png" ) );
   b_o_worm_hide->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Worm: Hide", b_o_worm_hide);

   b_o_worm_trail_on = new IMImageButton();
   b_o_worm_trail_on->setNormalTexture( t_manager.getTexture( "WormOrderButtonBase.png" ) );
   b_o_worm_trail_on->setPressedTexture( t_manager.getTexture( "WormButtonPressed.png" ) );
   b_o_worm_trail_on->setHoverTexture( t_manager.getTexture( "WormButtonHover.png" ) );
   b_o_worm_trail_on->setImage( t_manager.getTexture( "WormOrderTrailOn.png" ) );
   b_o_worm_trail_on->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Worm: Trail On", b_o_worm_trail_on);

   b_o_worm_trail_off = new IMImageButton();
   b_o_worm_trail_off->setNormalTexture( t_manager.getTexture( "WormOrderButtonBase.png" ) );
   b_o_worm_trail_off->setPressedTexture( t_manager.getTexture( "WormButtonPressed.png" ) );
   b_o_worm_trail_off->setHoverTexture( t_manager.getTexture( "WormButtonHover.png" ) );
   b_o_worm_trail_off->setImage( t_manager.getTexture( "WormOrderTrailOff.png" ) );
   b_o_worm_trail_off->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Worm: Trail Off", b_o_worm_trail_off);

   b_pl_alert_bird = new IMImageButton();
   b_pl_alert_bird->setAllTextures( t_manager.getTexture( "BirdAlertButton.png" ) );
   b_pl_alert_bird->setImage( NULL );
   b_pl_alert_bird->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Alert Bird", b_pl_alert_bird);

   b_pl_cmd_go_bird = new IMImageButton();
   b_pl_cmd_go_bird->setAllTextures( t_manager.getTexture( "BirdGoButton.png" ) );
   b_pl_cmd_go_bird->setImage( NULL );
   b_pl_cmd_go_bird->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Go Bird", b_pl_cmd_go_bird);

   b_o_bird_fly = new IMImageButton();
   b_o_bird_fly->setNormalTexture( t_manager.getTexture( "BirdOrderButtonBase.png" ) );
   b_o_bird_fly->setPressedTexture( t_manager.getTexture( "BirdButtonPressed.png" ) );
   b_o_bird_fly->setHoverTexture( t_manager.getTexture( "BirdButtonHover.png" ) );
   b_o_bird_fly->setImage( t_manager.getTexture( "BirdOrderFly.png" ) );
   b_o_bird_fly->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bird: Fly", b_o_bird_fly);

   b_cmd_bird_shout = new IMImageButton();
   b_cmd_bird_shout->setNormalTexture( t_manager.getTexture( "BirdControlButtonBase.png" ) );
   b_cmd_bird_shout->setPressedTexture( t_manager.getTexture( "BirdControlButtonPressed.png" ) );
   b_cmd_bird_shout->setHoverTexture( t_manager.getTexture( "BirdControlButtonHover.png" ) );
   b_cmd_bird_shout->setImage( t_manager.getTexture( "BirdControlShout.png" ) );
   b_cmd_bird_shout->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bird: Shout", b_cmd_bird_shout);

   b_cmd_bird_quiet = new IMImageButton();
   b_cmd_bird_quiet->setNormalTexture( t_manager.getTexture( "BirdControlButtonBase.png" ) );
   b_cmd_bird_quiet->setPressedTexture( t_manager.getTexture( "BirdControlButtonPressed.png" ) );
   b_cmd_bird_quiet->setHoverTexture( t_manager.getTexture( "BirdControlButtonHover.png" ) );
   b_cmd_bird_quiet->setImage( t_manager.getTexture( "BirdControlQuiet.png" ) );
   b_cmd_bird_quiet->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bird: Quiet", b_cmd_bird_quiet);

   b_pl_alert_bug = new IMImageButton();
   b_pl_alert_bug->setAllTextures( t_manager.getTexture( "BugAlertButton.png" ) );
   b_pl_alert_bug->setImage( NULL );
   b_pl_alert_bug->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Alert Bug", b_pl_alert_bug);

   b_pl_cmd_go_bug = new IMImageButton();
   b_pl_cmd_go_bug->setAllTextures( t_manager.getTexture( "BugGoButton.png" ) );
   b_pl_cmd_go_bug->setImage( NULL );
   b_pl_cmd_go_bug->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Go Bug", b_pl_cmd_go_bug);

   b_o_bug_meditate = new IMImageButton();
   b_o_bug_meditate->setNormalTexture( t_manager.getTexture( "BugOrderButtonBase.png" ) );
   b_o_bug_meditate->setPressedTexture( t_manager.getTexture( "BugButtonPressed.png" ) );
   b_o_bug_meditate->setHoverTexture( t_manager.getTexture( "BugButtonHover.png" ) );
   b_o_bug_meditate->setImage( t_manager.getTexture( "BugOrderMeditate.png" ) );
   b_o_bug_meditate->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bug: Meditate", b_o_bug_meditate);

   b_o_bug_fireball = new IMImageButton();
   b_o_bug_fireball->setNormalTexture( t_manager.getTexture( "BugOrderButtonBase.png" ) );
   b_o_bug_fireball->setPressedTexture( t_manager.getTexture( "BugButtonPressed.png" ) );
   b_o_bug_fireball->setHoverTexture( t_manager.getTexture( "BugButtonHover.png" ) );
   b_o_bug_fireball->setImage( t_manager.getTexture( "BugOrderFireball.png" ) );
   b_o_bug_fireball->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bug: Fireball", b_o_bug_fireball);

   b_o_bug_sunder = new IMImageButton();
   b_o_bug_sunder->setNormalTexture( t_manager.getTexture( "BugOrderButtonBase.png" ) );
   b_o_bug_sunder->setPressedTexture( t_manager.getTexture( "BugButtonPressed.png" ) );
   b_o_bug_sunder->setHoverTexture( t_manager.getTexture( "BugButtonHover.png" ) );
   b_o_bug_sunder->setImage( t_manager.getTexture( "BugOrderSunder.png" ) );
   b_o_bug_sunder->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bug: Sunder", b_o_bug_sunder);

   b_o_bug_heal = new IMImageButton();
   b_o_bug_heal->setNormalTexture( t_manager.getTexture( "BugOrderButtonBase.png" ) );
   b_o_bug_heal->setPressedTexture( t_manager.getTexture( "BugButtonPressed.png" ) );
   b_o_bug_heal->setHoverTexture( t_manager.getTexture( "BugButtonHover.png" ) );
   b_o_bug_heal->setImage( t_manager.getTexture( "BugOrderHeal.png" ) );
   b_o_bug_heal->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bug: Heal", b_o_bug_heal);

   b_o_bug_open_wormhole = new IMImageButton();
   b_o_bug_open_wormhole->setNormalTexture( t_manager.getTexture( "BugOrderButtonBase.png" ) );
   b_o_bug_open_wormhole->setPressedTexture( t_manager.getTexture( "BugButtonPressed.png" ) );
   b_o_bug_open_wormhole->setHoverTexture( t_manager.getTexture( "BugButtonHover.png" ) );
   b_o_bug_open_wormhole->setImage( t_manager.getTexture( "BugOrderOpenWormhole.png" ) );
   b_o_bug_open_wormhole->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bug: Open Wormhole", b_o_bug_open_wormhole);

   b_o_bug_close_wormhole = new IMImageButton();
   b_o_bug_close_wormhole->setNormalTexture( t_manager.getTexture( "BugOrderButtonBase.png" ) );
   b_o_bug_close_wormhole->setPressedTexture( t_manager.getTexture( "BugButtonPressed.png" ) );
   b_o_bug_close_wormhole->setHoverTexture( t_manager.getTexture( "BugButtonHover.png" ) );
   b_o_bug_close_wormhole->setImage( t_manager.getTexture( "BugOrderCloseWormhole.png" ) );
   b_o_bug_close_wormhole->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bug: Close Wormhole", b_o_bug_close_wormhole);

   b_monster_image = new IMImageButton();
   b_monster_image->setAllTextures( t_manager.getTexture( "MonsterButtonImage.png" ) );
   b_monster_image->setImage( NULL );
   b_monster_image->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Monster Image", b_monster_image);

   b_soldier_image = new IMImageButton();
   b_soldier_image->setAllTextures( t_manager.getTexture( "SoldierButtonImage.png" ) );
   b_soldier_image->setImage( NULL );
   b_soldier_image->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Soldier Image", b_soldier_image);

   b_worm_image = new IMImageButton();
   b_worm_image->setAllTextures( t_manager.getTexture( "WormButtonImage.png" ) );
   b_worm_image->setImage( NULL );
   b_worm_image->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Worm Image", b_worm_image);

   b_bird_image = new IMImageButton();
   b_bird_image->setAllTextures( t_manager.getTexture( "BirdButtonImage.png" ) );
   b_bird_image->setImage( NULL );
   b_bird_image->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bird Image", b_bird_image);

   b_bug_image = new IMImageButton();
   b_bug_image->setAllTextures( t_manager.getTexture( "BugButtonImage.png" ) );
   b_bug_image->setImage( NULL );
   b_bug_image->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Bug Image", b_bug_image);

   b_count_1 = new IMTextButton();
   b_count_1->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_1->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_1->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_1->setText( &s_1 );
   b_count_1->setTextColor( Color::Black );
   b_count_1->setFont( menu_font );
   gui_manager.registerWidget( "Count: 1", b_count_1);

   b_count_2 = new IMTextButton();
   b_count_2->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_2->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_2->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_2->setText( &s_2 );
   b_count_2->setTextColor( Color::Black );
   b_count_2->setFont( menu_font );
   gui_manager.registerWidget( "Count: 2", b_count_2);

   b_count_3 = new IMTextButton();
   b_count_3->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_3->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_3->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_3->setText( &s_3 );
   b_count_3->setTextColor( Color::Black );
   b_count_3->setFont( menu_font );
   gui_manager.registerWidget( "Count: 3", b_count_3);

   b_count_4 = new IMTextButton();
   b_count_4->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_4->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_4->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_4->setText( &s_4 );
   b_count_4->setTextColor( Color::Black );
   b_count_4->setFont( menu_font );
   gui_manager.registerWidget( "Count: 4", b_count_4);

   b_count_5 = new IMTextButton();
   b_count_5->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_5->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_5->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_5->setText( &s_5 );
   b_count_5->setTextColor( Color::Black );
   b_count_5->setFont( menu_font );
   gui_manager.registerWidget( "Count: 5", b_count_5);

   b_count_6 = new IMTextButton();
   b_count_6->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_6->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_6->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_6->setText( &s_6 );
   b_count_6->setTextColor( Color::Black );
   b_count_6->setFont( menu_font );
   gui_manager.registerWidget( "Count: 6", b_count_6);

   b_count_7 = new IMTextButton();
   b_count_7->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_7->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_7->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_7->setText( &s_7 );
   b_count_7->setTextColor( Color::Black );
   b_count_7->setFont( menu_font );
   gui_manager.registerWidget( "Count: 7", b_count_7);

   b_count_8 = new IMTextButton();
   b_count_8->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_8->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_8->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_8->setText( &s_8 );
   b_count_8->setTextColor( Color::Black );
   b_count_8->setFont( menu_font );
   gui_manager.registerWidget( "Count: 8", b_count_8);

   b_count_9 = new IMTextButton();
   b_count_9->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_9->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_9->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_9->setText( &s_9 );
   b_count_9->setTextColor( Color::Black );
   b_count_9->setFont( menu_font );
   gui_manager.registerWidget( "Count: 9", b_count_9);

   b_count_0 = new IMTextButton();
   b_count_0->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_0->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_0->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_0->setText( &s_0 );
   b_count_0->setTextColor( Color::Black );
   b_count_0->setFont( menu_font );
   gui_manager.registerWidget( "Count: 0", b_count_0);

   b_count_infinite = new IMImageButton();
   b_count_infinite->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_infinite->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_infinite->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_infinite->setImage( t_manager.getTexture( "CountButtonInfinite.png" ) );
   b_count_infinite->setImageOffset( 0, 0 );
   gui_manager.registerWidget( "Count: infinite", b_count_infinite);

   b_count_reset = new IMTextButton();
   b_count_reset->setNormalTexture( t_manager.getTexture( "CountButtonBase.png" ) );
   b_count_reset->setPressedTexture( t_manager.getTexture( "CountButtonPressed.png" ) );
   b_count_reset->setHoverTexture( t_manager.getTexture( "CountButtonHover.png" ) );
   b_count_reset->setText( &s_reset );
   b_count_reset->setTextColor( Color::Black );
   b_count_reset->setFont( menu_font );
   gui_manager.registerWidget( "Count: reset", b_count_reset);

   count_text = new Text();
   count_text->setFont( *menu_font );
   count_text->setColor( Color::White );

   b_numpad_area = new IMEdgeButton();
   b_numpad_area->setAllTextures( t_manager.getTexture( "UICenterBrown.png" ) );
   b_numpad_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_numpad_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_numpad_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Numpad area", b_numpad_area);

   b_conditional_area = new IMEdgeButton();
   b_conditional_area->setAllTextures( t_manager.getTexture( "UICenterBrown.png" ) );
   b_conditional_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_conditional_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_conditional_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Conditional area", b_conditional_area);

   b_movement_area = new IMEdgeButton();
   b_movement_area->setAllTextures( t_manager.getTexture( "UICenterBrown.png" ) );
   b_movement_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_movement_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_movement_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Movement area", b_movement_area);

   b_attack_area = new IMEdgeButton();
   b_attack_area->setAllTextures( t_manager.getTexture( "UICenterBrown.png" ) );
   b_attack_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_attack_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_attack_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Attack area", b_attack_area);

   b_control_area = new IMEdgeButton();
   b_control_area->setAllTextures( t_manager.getTexture( "UICenterBrown.png" ) );
   b_control_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_control_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_control_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Control area", b_control_area);

   b_pl_cmd_area = new IMEdgeButton();
   b_pl_cmd_area->setAllTextures( t_manager.getTexture( "UICenterBrown.png" ) );
   b_pl_cmd_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_pl_cmd_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_pl_cmd_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Player command area", b_pl_cmd_area);

   b_monster_area = new IMEdgeButton();
   b_monster_area->setAllTextures( t_manager.getTexture( "MonsterColorSoft.png" ) );
   b_monster_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_monster_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_monster_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Monster area", b_monster_area);

   b_soldier_area = new IMEdgeButton();
   b_soldier_area->setAllTextures( t_manager.getTexture( "SoldierColorSoft.png" ) );
   b_soldier_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_soldier_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_soldier_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Soldier area", b_soldier_area);

   b_worm_area = new IMEdgeButton();
   b_worm_area->setAllTextures( t_manager.getTexture( "WormColorSoft.png" ) );
   b_worm_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_worm_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_worm_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Worm area", b_worm_area);

   b_bird_area = new IMEdgeButton();
   b_bird_area->setAllTextures( t_manager.getTexture( "BirdColorSoft.png" ) );
   b_bird_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_bird_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_bird_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Bird area", b_bird_area);

   b_bug_area = new IMEdgeButton();
   b_bug_area->setAllTextures( t_manager.getTexture( "BugColorSoft.png" ) );
   b_bug_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_bug_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_bug_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Bug area", b_bug_area);

   // Victory Gui --
   b_victory_back_to_map = new IMEdgeTextButton();
   b_victory_back_to_map->setAllTextures( t_manager.getTexture( "UICenterBrown.png" ) );
   b_victory_back_to_map->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_victory_back_to_map->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_victory_back_to_map->setEdgeWidth( 3 );
   b_victory_back_to_map->setText( &s_back_to_map );
   b_victory_back_to_map->setTextColor( Color::Black );
   b_victory_back_to_map->setFont( menu_font );
   gui_manager.registerWidget( "Victory - Back to Map", b_victory_back_to_map);


   init_level_gui = true;
   fitGui_Level();
   return 0;
}

int selectConditionButton( Order_Conditional c )
{
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();

   b_con_enemy_adjacent->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_enemy_adjacent->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_enemy_adjacent->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );

   b_con_enemy_ahead->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_enemy_ahead->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_enemy_ahead->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );

   b_con_enemy_in_range->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_enemy_in_range->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_enemy_in_range->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );

   /*
   b_con_ally_adjacent->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_ally_adjacent->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_ally_adjacent->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );

   b_con_ally_ahead->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_ally_ahead->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_ally_ahead->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );

   b_con_ally_in_range->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_ally_in_range->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_ally_in_range->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );
   */

   b_con_half_health->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_half_health->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_half_health->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );

   b_con_20_health->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_20_health->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_20_health->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );

   b_con_blocked_ahead->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_blocked_ahead->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_blocked_ahead->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );

   b_con_clear->setNormalTexture( t_manager.getTexture( "ConditionalButtonBase.png" ) );
   b_con_clear->setHoverTexture( t_manager.getTexture( "ConditionalButtonHover.png" ) );
   b_con_clear->setPressedTexture( t_manager.getTexture( "ConditionalButtonPressed.png" ) );

   switch (c)
   {
      case ENEMY_ADJACENT:
         b_con_enemy_adjacent->setAllTextures(t_manager.getTexture("ConditionalSelectedGreen.png"));
         break;
      case ENEMY_NOT_ADJACENT:
         b_con_enemy_adjacent->setAllTextures(t_manager.getTexture("ConditionalSelectedRed.png"));
         break;
      case ENEMY_AHEAD:
         b_con_enemy_ahead->setAllTextures(t_manager.getTexture("ConditionalSelectedGreen.png"));
         break;
      case ENEMY_NOT_AHEAD:
         b_con_enemy_ahead->setAllTextures(t_manager.getTexture("ConditionalSelectedRed.png"));
         break;
      case ENEMY_IN_RANGE:
         b_con_enemy_in_range->setAllTextures(t_manager.getTexture("ConditionalSelectedGreen.png"));
         break;
      case ENEMY_NOT_IN_RANGE:
         b_con_enemy_in_range->setAllTextures(t_manager.getTexture("ConditionalSelectedRed.png"));
         break;
         /*
      case ALLY_ADJACENT:
         b_con_ally_adjacent->setAllTextures(t_manager.getTexture("ConditionalSelectedGreen.png"));
         break;
      case ALLY_NOT_ADJACENT:
         b_con_ally_adjacent->setAllTextures(t_manager.getTexture("ConditionalSelectedRed.png"));
         break;
      case ALLY_AHEAD:
         b_con_ally_ahead->setAllTextures(t_manager.getTexture("ConditionalSelectedGreen.png"));
         break;
      case ALLY_NOT_AHEAD:
         b_con_ally_ahead->setAllTextures(t_manager.getTexture("ConditionalSelectedRed.png"));
         break;
      case ALLY_IN_RANGE:
         b_con_ally_in_range->setAllTextures(t_manager.getTexture("ConditionalSelectedGreen.png"));
         break;
      case ALLY_NOT_IN_RANGE:
         b_con_ally_in_range->setAllTextures(t_manager.getTexture("ConditionalSelectedRed.png"));
         break;
         */
      case HEALTH_UNDER_50:
         b_con_half_health->setAllTextures(t_manager.getTexture("ConditionalSelectedGreen.png"));
         break;
      case HEALTH_OVER_50:
         b_con_half_health->setAllTextures(t_manager.getTexture("ConditionalSelectedRed.png"));
         break;
      case HEALTH_UNDER_20:
         b_con_20_health->setAllTextures(t_manager.getTexture("ConditionalSelectedGreen.png"));
         break;
      case HEALTH_OVER_20:
         b_con_20_health->setAllTextures(t_manager.getTexture("ConditionalSelectedRed.png"));
         break;
      case BLOCKED_AHEAD:
         b_con_blocked_ahead->setAllTextures(t_manager.getTexture("ConditionalSelectedGreen.png"));
         break;
      case NOT_BLOCKED_AHEAD:
         b_con_blocked_ahead->setAllTextures(t_manager.getTexture("ConditionalSelectedRed.png"));
         break;
      default:
         break;
   }

   return true;
}

void drawGuiButtonCounts()
{
   // Count boxes
   int count_text_size = 12;
   if (button_size < 40) count_text_size = 10;
   drawCount( 0, b_o_turn_north_pos.x, b_o_turn_north_pos.y, button_size, false, count_text_size );
   drawCount( 0, b_o_turn_east_pos.x, b_o_turn_east_pos.y, button_size, false, count_text_size );
   drawCount( 0, b_o_turn_south_pos.x, b_o_turn_south_pos.y, button_size, false, count_text_size );
   drawCount( 0, b_o_turn_west_pos.x, b_o_turn_west_pos.y, button_size, false, count_text_size );
   drawCount( 0, b_o_turn_nearest_enemy_pos.x, b_o_turn_nearest_enemy_pos.y, button_size, false, count_text_size );
   drawCount( 2, b_o_monster_guard_pos.x, b_o_monster_guard_pos.y, button_size, true, count_text_size );
   drawCount( 0, b_o_soldier_switch_axe_pos.x, b_o_soldier_switch_axe_pos.y, button_size, false, count_text_size );
   drawCount( 0, b_o_soldier_switch_spear_pos.x, b_o_soldier_switch_spear_pos.y, button_size, false, count_text_size );
   drawCount( 0, b_o_soldier_switch_bow_pos.x, b_o_soldier_switch_bow_pos.y, button_size, false, count_text_size );
   drawCount( 0, b_o_worm_trail_on_pos.x, b_o_worm_trail_on_pos.y, button_size, false, count_text_size );
   drawCount( 0, b_o_worm_trail_off_pos.x, b_o_worm_trail_off_pos.y, button_size, false, count_text_size );
   drawCount( 2, b_o_bird_fly_pos.x, b_o_bird_fly_pos.y, button_size, true, count_text_size );
   drawCount( 10, b_o_bug_meditate_pos.x, b_o_bug_meditate_pos.y, button_size, false, count_text_size );
}

void drawGuiButtonKeybinds()
{
   int txt_size = 12;
   if (button_size < 40) txt_size = 10;
   drawKeybind( KB_BTN_COND_ENEMY_ADJACENT, b_con_enemy_adjacent_pos.x, b_con_enemy_adjacent_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COND_ENEMY_AHEAD, b_con_enemy_ahead_pos.x, b_con_enemy_ahead_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COND_ENEMY_IN_RANGE, b_con_enemy_in_range_pos.x, b_con_enemy_in_range_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COND_HEALTH_UNDER_50, b_con_half_health_pos.x, b_con_half_health_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COND_HEALTH_UNDER_20, b_con_20_health_pos.x, b_con_20_health_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COND_BLOCKED_AHEAD, b_con_blocked_ahead_pos.x, b_con_blocked_ahead_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COND_CLEAR, b_con_clear_pos.x, b_con_clear_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_MOVE_FORWARD, b_o_move_forward_pos.x, b_o_move_forward_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_MOVE_BACK, b_o_move_backward_pos.x, b_o_move_backward_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_FOLLOW_PATH, b_o_follow_path_pos.x, b_o_follow_path_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_TURN_NORTH, b_o_turn_north_pos.x, b_o_turn_north_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_TURN_EAST, b_o_turn_east_pos.x, b_o_turn_east_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_TURN_SOUTH, b_o_turn_south_pos.x, b_o_turn_south_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_TURN_WEST, b_o_turn_west_pos.x, b_o_turn_west_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_TURN_NEAREST_ENEMY, b_o_turn_nearest_enemy_pos.x, b_o_turn_nearest_enemy_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_ATTACK_SMALLEST, b_o_attack_smallest_pos.x, b_o_attack_smallest_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_ATTACK_BIGGEST, b_o_attack_biggest_pos.x, b_o_attack_biggest_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_ATTACK_CLOSEST, b_o_attack_closest_pos.x, b_o_attack_closest_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_ATTACK_FARTHEST, b_o_attack_farthest_pos.x, b_o_attack_farthest_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_ATTACK_MOST_ARMORED, b_o_attack_least_armor_pos.x, b_o_attack_least_armor_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_ATTACK_LEAST_ARMORED, b_o_attack_most_armor_pos.x, b_o_attack_most_armor_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_START_BLOCK, b_o_start_block_pos.x, b_o_start_block_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_END_BLOCK, b_o_end_block_pos.x, b_o_end_block_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_REPEAT, b_o_repeat_pos.x, b_o_repeat_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_WAIT, b_o_wait_pos.x, b_o_wait_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_ALERT_ALL, b_pl_alert_all_pos.x, b_pl_alert_all_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_GO, b_pl_cmd_go_pos.x, b_pl_cmd_go_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_GO_ALL, b_pl_cmd_go_all_pos.x, b_pl_cmd_go_all_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_SET_GROUP, b_pl_set_group_pos.x, b_pl_set_group_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_DELAY, b_pl_delay_pos.x, b_pl_delay_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_ALERT_MONSTERS, b_pl_alert_monster_pos.x, b_pl_alert_monster_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_GO_MONSTERS, b_pl_cmd_go_monster_pos.x, b_pl_cmd_go_monster_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_MONSTER_GUARD, b_o_monster_guard_pos.x, b_o_monster_guard_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_MONSTER_BURST, b_o_monster_burst_pos.x, b_o_monster_burst_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_ALERT_SOLDIERS, b_pl_alert_soldier_pos.x, b_pl_alert_soldier_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_GO_SOLDIERS, b_pl_cmd_go_soldier_pos.x, b_pl_cmd_go_soldier_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_SOLDIER_SWITCH_AXE, b_o_soldier_switch_axe_pos.x, b_o_soldier_switch_axe_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_SOLDIER_SWITCH_SPEAR, b_o_soldier_switch_spear_pos.x, b_o_soldier_switch_spear_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_SOLDIER_SWITCH_BOW, b_o_soldier_switch_bow_pos.x, b_o_soldier_switch_bow_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_ALERT_WORMS, b_pl_alert_worm_pos.x, b_pl_alert_worm_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_GO_WORMS, b_pl_cmd_go_worm_pos.x, b_pl_cmd_go_worm_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_WORM_HIDE, b_o_worm_hide_pos.x, b_o_worm_hide_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_WORM_TRAIL_START, b_o_worm_trail_on_pos.x, b_o_worm_trail_on_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_WORM_TRAIL_END, b_o_worm_trail_off_pos.x, b_o_worm_trail_off_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_ALERT_BIRDS, b_pl_alert_bird_pos.x, b_pl_alert_bird_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_GO_BIRDS, b_pl_cmd_go_bird_pos.x, b_pl_cmd_go_bird_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_BIRD_FLY, b_o_bird_fly_pos.x, b_o_bird_fly_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_BIRD_CMD_SHOUT, b_cmd_bird_shout_pos.x, b_cmd_bird_shout_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_BIRD_CMD_QUIET, b_cmd_bird_quiet_pos.x, b_cmd_bird_quiet_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_ALERT_BUGS, b_pl_alert_bug_pos.x, b_pl_alert_bug_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_PL_GO_BUGS, b_pl_cmd_go_bug_pos.x, b_pl_cmd_go_bug_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_BUG_MEDITATE, b_o_bug_meditate_pos.x, b_o_bug_meditate_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_BUG_CAST_FIREBALL, b_o_bug_fireball_pos.x, b_o_bug_fireball_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_BUG_CAST_SHOCK, b_o_bug_sunder_pos.x, b_o_bug_sunder_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_BUG_CAST_DUST, b_o_bug_heal_pos.x, b_o_bug_heal_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_BUG_OPEN_WORMHOLE, b_o_bug_open_wormhole_pos.x, b_o_bug_open_wormhole_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_BUG_CLOSE_WORMHOLE, b_o_bug_close_wormhole_pos.x, b_o_bug_close_wormhole_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_INFINITE, b_count_infinite_pos.x, b_count_infinite_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_1, b_count_1_pos.x, b_count_1_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_2, b_count_2_pos.x, b_count_2_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_3, b_count_3_pos.x, b_count_3_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_4, b_count_4_pos.x, b_count_4_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_5, b_count_5_pos.x, b_count_5_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_6, b_count_6_pos.x, b_count_6_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_7, b_count_7_pos.x, b_count_7_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_8, b_count_8_pos.x, b_count_8_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_9, b_count_9_pos.x, b_count_9_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_0, b_count_0_pos.x, b_count_0_pos.y, button_size, txt_size );
   drawKeybind( KB_BTN_COUNT_CLEAR, b_count_reset_pos.x, b_count_reset_pos.y, button_size, txt_size );
}

int drawOrderButtons()
{
   if (init_level_gui == false)
      initLevelGui();

   drawGuiButtonCounts();

   if (config::show_keybindings)
      drawGuiButtonKeybinds();

   if (b_con_enemy_adjacent->doWidget())
      playerAddConditional( ENEMY_ADJACENT );

   if (b_con_enemy_ahead->doWidget())
      playerAddConditional( ENEMY_AHEAD );

   if (b_con_enemy_in_range->doWidget())
      playerAddConditional( ENEMY_IN_RANGE );

   /*
   if (b_con_ally_adjacent->doWidget())
      playerAddConditional( ALLY_ADJACENT );

   if (b_con_ally_ahead->doWidget())
      playerAddConditional( ALLY_AHEAD );

   if (b_con_ally_in_range->doWidget())
      playerAddConditional( ALLY_IN_RANGE );
      */
   if (b_con_half_health->doWidget())
      playerAddConditional( HEALTH_UNDER_50 );

   if (b_con_20_health->doWidget())
      playerAddConditional( HEALTH_UNDER_20 );

   if (b_con_blocked_ahead->doWidget())
      playerAddConditional( BLOCKED_AHEAD );

   if (b_con_clear->doWidget())
      playerAddConditional( TRUE );

   if (b_o_move_forward->doWidget())
      playerAddOrder( MOVE_FORWARD );

   if (b_o_move_backward->doWidget())
      playerAddOrder( MOVE_BACK );
      
   if (b_o_turn_north->doWidget())
      playerAddOrder( TURN_NORTH );
      
   if (b_o_turn_east->doWidget())
      playerAddOrder( TURN_EAST );
      
   if (b_o_turn_south->doWidget())
      playerAddOrder( TURN_SOUTH );
      
   if (b_o_turn_west->doWidget())
      playerAddOrder( TURN_WEST );
      
   if (b_o_turn_nearest_enemy->doWidget())
      playerAddOrder( TURN_NEAREST_ENEMY );
      
   if (b_o_follow_path->doWidget())
      playerAddOrder( FOLLOW_PATH );
      
   if (b_o_attack_smallest->doWidget())
      playerAddOrder( ATTACK_SMALLEST );
      
   if (b_o_attack_biggest->doWidget())
      playerAddOrder( ATTACK_BIGGEST );
      
   if (b_o_attack_closest->doWidget())
      playerAddOrder( ATTACK_CLOSEST );
      
   if (b_o_attack_farthest->doWidget())
      playerAddOrder( ATTACK_FARTHEST );
      
   if (b_o_attack_most_armor->doWidget())
      playerAddOrder( ATTACK_MOST_ARMORED );
      
   if (b_o_attack_least_armor->doWidget())
      playerAddOrder( ATTACK_LEAST_ARMORED );
      
   if (b_o_start_block->doWidget())
      playerAddOrder( START_BLOCK );
      
   if (b_o_end_block->doWidget())
      playerAddOrder( END_BLOCK );
      
   if (b_o_repeat->doWidget())
      playerAddOrder( REPEAT );

   if (b_o_wait->doWidget())
      playerAddOrder( WAIT );

   if (b_pl_alert_all->doWidget())
      playerAddOrder( PL_ALERT_ALL );

   if (b_pl_cmd_go->doWidget())
      playerAddOrder( PL_CMD_GO );

   if (b_pl_cmd_go_all->doWidget())
      playerAddOrder( PL_CMD_GO_ALL );

   if (b_pl_set_group->doWidget())
      playerAddOrder( PL_SET_GROUP );

   if (b_pl_delay->doWidget())
      playerAddOrder( PL_DELAY );

   if (b_pl_alert_monster->doWidget())
      playerAddOrder( PL_ALERT_MONSTERS );

   if (b_pl_cmd_go_monster->doWidget())
      playerAddOrder( PL_CMD_GO_MONSTERS );

   if (b_o_monster_guard->doWidget())
      playerAddOrder( MONSTER_GUARD );

   if (b_o_monster_burst->doWidget())
      playerAddOrder( MONSTER_BURST );

   if (b_pl_alert_soldier->doWidget())
      playerAddOrder( PL_ALERT_SOLDIERS );

   if (b_pl_cmd_go_soldier->doWidget())
      playerAddOrder( PL_CMD_GO_SOLDIERS );

   if (b_o_soldier_switch_axe->doWidget())
      playerAddOrder( SOLDIER_SWITCH_AXE );

   if (b_o_soldier_switch_spear->doWidget())
      playerAddOrder( SOLDIER_SWITCH_SPEAR );

   if (b_o_soldier_switch_bow->doWidget())
      playerAddOrder( SOLDIER_SWITCH_BOW );

   if (b_pl_alert_worm->doWidget())
      playerAddOrder( PL_ALERT_WORMS );

   if (b_pl_cmd_go_worm->doWidget())
      playerAddOrder( PL_CMD_GO_WORMS );

   if (b_o_worm_hide->doWidget())
      playerAddOrder( WORM_HIDE );

   if (b_o_worm_trail_on->doWidget())
      playerAddOrder( WORM_TRAIL_START );

   if (b_o_worm_trail_off->doWidget())
      playerAddOrder( WORM_TRAIL_END );

   if (b_pl_alert_bird->doWidget())
      playerAddOrder( PL_ALERT_BIRDS );

   if (b_pl_cmd_go_bird->doWidget())
      playerAddOrder( PL_CMD_GO_BIRDS );

   if (b_o_bird_fly->doWidget())
      playerAddOrder( BIRD_FLY );

   if (b_cmd_bird_shout->doWidget())
      playerAddOrder( BIRD_CMD_SHOUT );

   if (b_cmd_bird_quiet->doWidget())
      playerAddOrder( BIRD_CMD_QUIET );

   if (b_pl_alert_bug->doWidget())
      playerAddOrder( PL_ALERT_BUGS );

   if (b_pl_cmd_go_bug->doWidget())
      playerAddOrder( PL_CMD_GO_BUGS );

   if (b_o_bug_meditate->doWidget())
      playerAddOrder( BUG_MEDITATE );

   if (b_o_bug_fireball->doWidget())
      playerAddOrder( BUG_CAST_FIREBALL );

   if (b_o_bug_sunder->doWidget())
      playerAddOrder( BUG_CAST_SHOCK );

   if (b_o_bug_heal->doWidget())
      playerAddOrder( BUG_CAST_DUST );

   if (b_o_bug_open_wormhole->doWidget())
      playerAddOrder( BUG_OPEN_WORMHOLE );

   if (b_o_bug_close_wormhole->doWidget())
      playerAddOrder( BUG_CLOSE_WORMHOLE );


   if (b_count_0->doWidget())
      playerAddCount( 0 );

   if (b_count_1->doWidget())
      playerAddCount( 1 );

   if (b_count_2->doWidget())
      playerAddCount( 2 );

   if (b_count_3->doWidget())
      playerAddCount( 3 );

   if (b_count_4->doWidget())
      playerAddCount( 4 );

   if (b_count_5->doWidget())
      playerAddCount( 5 );

   if (b_count_6->doWidget())
      playerAddCount( 6 );

   if (b_count_7->doWidget())
      playerAddCount( 7 );

   if (b_count_8->doWidget())
      playerAddCount( 8 );

   if (b_count_9->doWidget())
      playerAddCount( 9 );

   if (b_count_infinite->doWidget())
      playerSetCount( -1 );

   if (b_count_reset->doWidget())
      playerSetCount( 0 );

   b_monster_image->doWidget();
   b_soldier_image->doWidget();
   b_worm_image->doWidget();
   b_bird_image->doWidget();
   b_bug_image->doWidget();

   b_numpad_area->doWidget();
   b_conditional_area->doWidget();
   b_pl_cmd_area->doWidget();
   b_control_area->doWidget();
   b_movement_area->doWidget();
   b_attack_area->doWidget();
   b_monster_area->doWidget();
   b_soldier_area->doWidget();
   b_worm_area->doWidget();
   b_bird_area->doWidget();
   b_bug_area->doWidget();

   // Draw current count
   if (order_prep_count != 0) {
      stringstream ss;
      if (order_prep_count == -1)
         ss << "indefinitely";
      else
         ss << order_prep_count;
      count_text->setString( String(ss.str()) );

      SFML_GlobalRenderWindow::get()->draw( *count_text );
   }

   return 0;
}

KeybindTarget drawKeybindButtons()
{
   KeybindTarget kb = KB_NOTHING;

   if (init_level_gui == false)
      initLevelGui();

   drawGuiButtonCounts();

   drawGuiButtonKeybinds();

   if (b_con_enemy_adjacent->doWidget())
      kb = KB_BTN_COND_ENEMY_ADJACENT;

   if (b_con_enemy_ahead->doWidget())
      kb = KB_BTN_COND_ENEMY_AHEAD;

   if (b_con_enemy_in_range->doWidget())
      kb = KB_BTN_COND_ENEMY_IN_RANGE;

   /*
   if (b_con_ally_adjacent->doWidget())
      playerAddConditional( ALLY_ADJACENT );

   if (b_con_ally_ahead->doWidget())
      playerAddConditional( ALLY_AHEAD );

   if (b_con_ally_in_range->doWidget())
      playerAddConditional( ALLY_IN_RANGE );
      */
   if (b_con_half_health->doWidget())
      kb = KB_BTN_COND_HEALTH_UNDER_50;

   if (b_con_20_health->doWidget())
      kb = KB_BTN_COND_HEALTH_UNDER_20;

   if (b_con_blocked_ahead->doWidget())
      kb = KB_BTN_COND_BLOCKED_AHEAD;

   if (b_con_clear->doWidget())
      kb = KB_BTN_COND_CLEAR;

   if (b_o_move_forward->doWidget())
      kb = KB_BTN_MOVE_FORWARD;

   if (b_o_move_backward->doWidget())
      kb = KB_BTN_MOVE_BACK;
      
   if (b_o_turn_north->doWidget())
      kb = KB_BTN_TURN_NORTH;
      
   if (b_o_turn_east->doWidget())
      kb = KB_BTN_TURN_EAST;
      
   if (b_o_turn_south->doWidget())
      kb = KB_BTN_TURN_SOUTH;
      
   if (b_o_turn_west->doWidget())
      kb = KB_BTN_TURN_WEST;
      
   if (b_o_turn_nearest_enemy->doWidget())
      kb = KB_BTN_TURN_NEAREST_ENEMY;
      
   if (b_o_follow_path->doWidget())
      kb = KB_BTN_FOLLOW_PATH;
      
   if (b_o_attack_smallest->doWidget())
      kb = KB_BTN_ATTACK_SMALLEST;
      
   if (b_o_attack_biggest->doWidget())
      kb = KB_BTN_ATTACK_BIGGEST;
      
   if (b_o_attack_closest->doWidget())
      kb = KB_BTN_ATTACK_CLOSEST;
      
   if (b_o_attack_farthest->doWidget())
      kb = KB_BTN_ATTACK_FARTHEST;
      
   if (b_o_attack_most_armor->doWidget())
      kb = KB_BTN_ATTACK_MOST_ARMORED;
      
   if (b_o_attack_least_armor->doWidget())
      kb = KB_BTN_ATTACK_LEAST_ARMORED;
      
   if (b_o_start_block->doWidget())
      kb = KB_BTN_START_BLOCK;
      
   if (b_o_end_block->doWidget())
      kb = KB_BTN_END_BLOCK;
      
   if (b_o_repeat->doWidget())
      kb = KB_BTN_REPEAT;

   if (b_o_wait->doWidget())
      kb = KB_BTN_WAIT;

   if (b_pl_alert_all->doWidget())
      kb = KB_BTN_PL_ALERT_ALL;

   if (b_pl_cmd_go->doWidget())
      kb = KB_BTN_PL_GO;

   if (b_pl_cmd_go_all->doWidget())
      kb = KB_BTN_PL_GO_ALL;

   if (b_pl_set_group->doWidget())
      kb = KB_BTN_PL_SET_GROUP;

   if (b_pl_delay->doWidget())
      kb = KB_BTN_PL_DELAY;

   if (b_pl_alert_monster->doWidget())
      kb = KB_BTN_PL_ALERT_MONSTERS;

   if (b_pl_cmd_go_monster->doWidget())
      kb = KB_BTN_PL_GO_MONSTERS;

   if (b_o_monster_guard->doWidget())
      kb = KB_BTN_MONSTER_GUARD;

   if (b_o_monster_burst->doWidget())
      kb = KB_BTN_MONSTER_BURST;

   if (b_pl_alert_soldier->doWidget())
      kb = KB_BTN_PL_ALERT_SOLDIERS;

   if (b_pl_cmd_go_soldier->doWidget())
      kb = KB_BTN_PL_GO_SOLDIERS;

   if (b_o_soldier_switch_axe->doWidget())
      kb = KB_BTN_SOLDIER_SWITCH_AXE;

   if (b_o_soldier_switch_spear->doWidget())
      kb = KB_BTN_SOLDIER_SWITCH_SPEAR;

   if (b_o_soldier_switch_bow->doWidget())
      kb = KB_BTN_SOLDIER_SWITCH_BOW;

   if (b_pl_alert_worm->doWidget())
      kb = KB_BTN_PL_ALERT_WORMS;

   if (b_pl_cmd_go_worm->doWidget())
      kb = KB_BTN_PL_GO_WORMS;

   if (b_o_worm_hide->doWidget())
      kb = KB_BTN_WORM_HIDE;

   if (b_o_worm_trail_on->doWidget())
      kb = KB_BTN_WORM_TRAIL_START;

   if (b_o_worm_trail_off->doWidget())
      kb = KB_BTN_WORM_TRAIL_END;

   if (b_pl_alert_bird->doWidget())
      kb = KB_BTN_PL_ALERT_BIRDS;

   if (b_pl_cmd_go_bird->doWidget())
      kb = KB_BTN_PL_GO_BIRDS;

   if (b_o_bird_fly->doWidget())
      kb = KB_BTN_BIRD_FLY;

   if (b_cmd_bird_shout->doWidget())
      kb = KB_BTN_BIRD_CMD_SHOUT;

   if (b_cmd_bird_quiet->doWidget())
      kb = KB_BTN_BIRD_CMD_QUIET;

   if (b_pl_alert_bug->doWidget())
      kb = KB_BTN_PL_ALERT_BUGS;

   if (b_pl_cmd_go_bug->doWidget())
      kb = KB_BTN_PL_GO_BUGS;

   if (b_o_bug_meditate->doWidget())
      kb = KB_BTN_BUG_MEDITATE;

   if (b_o_bug_fireball->doWidget())
      kb = KB_BTN_BUG_CAST_FIREBALL;

   if (b_o_bug_sunder->doWidget())
      kb = KB_BTN_BUG_CAST_SHOCK;

   if (b_o_bug_heal->doWidget())
      kb = KB_BTN_BUG_CAST_DUST;

   if (b_o_bug_open_wormhole->doWidget())
      kb = KB_BTN_BUG_OPEN_WORMHOLE;

   if (b_o_bug_close_wormhole->doWidget())
      kb = KB_BTN_BUG_CLOSE_WORMHOLE;


   if (b_count_0->doWidget())
      kb = KB_BTN_COUNT_0;

   if (b_count_1->doWidget())
      kb = KB_BTN_COUNT_1;

   if (b_count_2->doWidget())
      kb = KB_BTN_COUNT_2;

   if (b_count_3->doWidget())
      kb = KB_BTN_COUNT_3;

   if (b_count_4->doWidget())
      kb = KB_BTN_COUNT_4;

   if (b_count_5->doWidget())
      kb = KB_BTN_COUNT_5;

   if (b_count_6->doWidget())
      kb = KB_BTN_COUNT_6;

   if (b_count_7->doWidget())
      kb = KB_BTN_COUNT_7;

   if (b_count_8->doWidget())
      kb = KB_BTN_COUNT_8;

   if (b_count_9->doWidget())
      kb = KB_BTN_COUNT_9;

   if (b_count_infinite->doWidget())
      kb = KB_BTN_COUNT_INFINITE;

   if (b_count_reset->doWidget())
      kb = KB_BTN_COUNT_CLEAR;

   b_monster_image->doWidget();
   b_soldier_image->doWidget();
   b_worm_image->doWidget();
   b_bird_image->doWidget();
   b_bug_image->doWidget();

   b_numpad_area->doWidget();
   b_conditional_area->doWidget();
   b_pl_cmd_area->doWidget();
   b_control_area->doWidget();
   b_movement_area->doWidget();
   b_attack_area->doWidget();
   b_monster_area->doWidget();
   b_soldier_area->doWidget();
   b_worm_area->doWidget();
   b_bird_area->doWidget();
   b_bug_area->doWidget();

   return kb;
}

//////////////////////////////////////////////////////////////////////
// Rest of the Gui ---

Sprite *s_clock_face,
       *s_clock_half_red,
       *s_clock_half_white;
bool init_tick_clock = false;

void initTickClock()
{
   s_clock_face = new Sprite( *SFML_TextureManager::getSingleton().getTexture( "ClockFace.png" ) );
   normalizeTo1x1( s_clock_face );
   s_clock_face->scale( 32, 32 );
   s_clock_face->setPosition( 2, 2 );

   s_clock_half_red = new Sprite( *SFML_TextureManager::getSingleton().getTexture( "ClockHalfRed.png" ) );
   normalizeTo1x1( s_clock_half_red );
   s_clock_half_red->scale( 32, 32 );
   s_clock_half_red->setOrigin( 64, 64 );
   s_clock_half_red->setPosition( 18, 18 );

   s_clock_half_white = new Sprite( *SFML_TextureManager::getSingleton().getTexture( "ClockHalfWhite.png" ) );
   normalizeTo1x1( s_clock_half_white );
   s_clock_half_white->setOrigin( 64, 64 );
   s_clock_half_white->scale( 32, 32 );
   s_clock_half_white->setPosition( 18, 18 );

   init_tick_clock = true;
}

int drawClock()
{
   RenderWindow *gui_window = SFML_GlobalRenderWindow::get();

   if (!init_tick_clock)
      initTickClock();

   Sprite *s_1, *s_2;
   if (turn % 2 == 0) {
      s_1 = s_clock_half_red;
      s_2 = s_clock_half_white;
   } else {
      s_1 = s_clock_half_white;
      s_2 = s_clock_half_red;
   }

   s_1->setRotation( 0 );
   gui_window->draw( *s_1 );
   s_2->setRotation( 180 );
   gui_window->draw( *s_2 );
   if (turn_progress < (c_turn_length / 2)) {
      s_2->setRotation( 360 * ( (float)turn_progress / (float)c_turn_length ) );
      gui_window->draw( *s_2 );
   } else {
      s_1->setRotation( 360 * ( ((float)turn_progress / (float)c_turn_length ) - 0.5) );
      gui_window->draw( *s_1 );
   }

   gui_window->draw( *s_clock_face );

   return 0;
}

int drawOrderQueue()
{
   if (player) {
      int draw_x = 38, draw_y = 2, x_edge = config::width();
      if (selected_unit != NULL) x_edge -= selection_box_width;

      for (int i = player->current_order; i != player->final_order; ++i) {
         if (i == player->max_orders) i = 0;

         Order &o = player->order_queue[i];
         if ( o.action != SKIP ) {
            drawOrder( player->order_queue[i], draw_x, draw_y, 32 );
            draw_x += 36;

            if (draw_x + 32 >= x_edge) {
               if (pause_state == c_fully_paused) {
                  draw_x = 2;
                  draw_y += 36;
               } else {
                  break;
               }
            }
         }
      }
   }
   return 0;
}

int drawSelectedUnit()
{
   RenderWindow *gui_window = SFML_GlobalRenderWindow::get();

   if (selected_unit != NULL) {
      if (selected_unit->alive != 1) {
         selected_unit = NULL;
         return -1;
      }

      Texture *t = selected_unit->getTexture();
      if (t) { 
         float window_edge = config::width();
         RectangleShape select_rect, health_bound_rect, health_rect;

         select_rect.setSize( Vector2f( selection_box_width, selection_box_height ) );
         select_rect.setPosition( window_edge - selection_box_width, 0 );
         select_rect.setFillColor( Color::White );
         select_rect.setOutlineColor( Color::Black );
         select_rect.setOutlineThickness( 2.0 );
         gui_window->draw( select_rect );

         // The image
         int picture_size = selection_box_height - 10;
         Sprite unit_image( *t);
         normalizeTo1x1( &unit_image );
         unit_image.scale( picture_size, picture_size );
         unit_image.setPosition( window_edge - selection_box_width + 5, 5 );

         gui_window->draw( unit_image );

         if (selected_unit->team == 0 && selected_unit->group != 1) {
            // Display group number
            drawCount( selected_unit->group, window_edge - selection_box_width + 5, 5, picture_size, false, 12 );
         }

         // Health box
         int health_box_start_x = window_edge - selection_box_width + selection_box_height + 5,
             health_box_width = window_edge - health_box_start_x - 5,
             health_box_start_y = selection_box_height - 18,
             health_box_height = 14;
         health_bound_rect.setSize( Vector2f( health_box_width, health_box_height ) );
         health_bound_rect.setPosition( health_box_start_x, health_box_start_y );
         health_bound_rect.setFillColor( Color::White );
         health_bound_rect.setOutlineColor( Color::Black );
         health_bound_rect.setOutlineThickness( 2.0 );
         gui_window->draw( health_bound_rect );

         health_rect.setSize( Vector2f( (health_box_width - 4) * (selected_unit->health / selected_unit->max_health), 10 ) );
         health_rect.setPosition( health_box_start_x + 2, health_box_start_y + 2 );
         health_rect.setFillColor( Color::Red );
         gui_window->draw( health_rect );

         // Descriptive text
         Text selected_text;
         selected_text.setString(String(selected_unit->descriptor()));
         selected_text.setFont( *menu_font );
         selected_text.setColor( (selected_unit->team == 0)?Color::Black:Color::Red);
         selected_text.setCharacterSize( 24 );
         FloatRect text_size = selected_text.getGlobalBounds();
         float text_x = ((health_box_width - text_size.width) / 2) + health_box_start_x;
         selected_text.setPosition( text_x, 0 );
         gui_window->draw( selected_text );

         if (selected_unit != player && selected_unit->team == 0)
         {
            // Draw selected unit's order queue
            int draw_x = window_edge - selection_box_width + 1,
                draw_y = selection_box_height + 2,
                dx = selection_box_width / 6;
            //for (int i = 0; i < selected_unit->order_count; ++i) {
               //if (i == selected_unit->max_orders) i = 0;
            for (int i = 0; i < selected_unit->max_orders; ++i) {

               if (i >= selected_unit->order_count) {
                  Order o( NUM_ACTIONS );
                  drawOrder( o, draw_x, draw_y, (dx - 2) );
               } else {
                  if (i == selected_unit->current_order) {
                     drawSquare( draw_x - 1, draw_y - 1 , dx, Color::White );
                  }

                  Order &o = selected_unit->order_queue[i];
                  if ( o.action != SKIP ) {
                     drawOrder( o, draw_x, draw_y, (dx - 2) );
                  }
               }

               draw_x += dx;
               if (draw_x >= window_edge) {
                  draw_x = window_edge - selection_box_width + 1;
                  draw_y += dx;
               }
            }
         }

      }
   }

   return 0;
}

void drawDebugInfo()
{
   if (show_framerate) {
      int x = 2,
          y = b_pl_cmd_area->_y_position - 24;
      Text t;
      t.setPosition( x, y );
      t.setFont( *menu_font );
      t.setColor( Color::White );
      t.setCharacterSize( 16 );
      stringstream ss;
      ss << "FPS fast: " << getFramerate();
      t.setString( String(ss.str()) );

      SFML_GlobalRenderWindow::get()->draw( t );

      Text t2;
      t2.setPosition( x, y - 20 );
      t2.setFont( *menu_font );
      t2.setColor( Color::White );
      t2.setCharacterSize( 16 );
      stringstream ss2;
      ss2 << "FPS slow: " << tickfps;
      t2.setString( String(ss2.str()) );

      SFML_GlobalRenderWindow::get()->draw( t2 );

   }
}

int drawGui()
{
   RenderWindow *gui_window = SFML_GlobalRenderWindow::get();
   gui_window->setView(gui_window->getDefaultView());

   drawPause();

   drawClock();
   drawOrderQueue();
   drawSelectedUnit();
   drawOrderButtons();

   drawRightClickMenu();

   drawDebugInfo();

   return 0;
}

//////////////////////////////////////////////////////////////////////
// Level Editor ---

// Gui
bool init_level_editor_gui = false;
IMEdgeButton *b_editor_gui_area;
IMButton *b_ed_dec_x_dim,
         *b_ed_inc_x_dim,
         *b_ed_dec_y_dim,
         *b_ed_inc_y_dim,
         *b_ed_move_player_north,
         *b_ed_move_player_south,
         *b_ed_move_player_east,
         *b_ed_move_player_west,
         *b_ed_mark_location_wc,
         *b_ed_mark_unit_wc,
         *b_ed_turn_unit_north,
         *b_ed_turn_unit_south,
         *b_ed_turn_unit_east,
         *b_ed_turn_unit_west,
         *b_ed_dec_unit_team,
         *b_ed_inc_unit_team;
IMEdgeTextButton *b_ed_write_level,
                 *b_ed_clear_level;
string s_ed_write_level = "Write to File",
       s_ed_clear_level = "Clear Level";
Text *txt_x_dim,
     *txt_y_dim,
     *txt_x_dim_value,
     *txt_y_dim_value,
     *txt_move_player,
     *txt_mark_unit_wc,
     *txt_mark_location_wc,
     *txt_unit_turn,
     *txt_unit_team,
     *txt_unit_team_value;
Sprite *sp_ed_unit;

void fitGui_LevelEditor()
{
   b_editor_gui_area->setSize( 220, config::height() );
   b_editor_gui_area->setPosition( config::width() - 220, 0 );

   txt_x_dim->setPosition( config::width() - 210, 10 );

   // Set dimensions
   b_ed_dec_x_dim->setSize( 30, 30 );
   b_ed_dec_x_dim->setPosition( config::width() - 130, 10 );

   txt_x_dim_value->setPosition( config::width() - 90, 10 );

   b_ed_inc_x_dim->setSize( 30, 30 );
   b_ed_inc_x_dim->setPosition( config::width() - 40, 10 );

   txt_y_dim->setPosition( config::width() - 210, 50 );

   b_ed_dec_y_dim->setSize( 30, 30 );
   b_ed_dec_y_dim->setPosition( config::width() - 130, 50 );

   txt_y_dim_value->setPosition( config::width() - 90, 50 );

   b_ed_inc_y_dim->setSize( 30, 30 );
   b_ed_inc_y_dim->setPosition( config::width() - 40, 50 );

   // Move player
   txt_move_player->setPosition( config::width() - 210, 110 );

   b_ed_move_player_north->setSize( 30, 30 );
   b_ed_move_player_north->setPosition( config::width() - 90, 110 );

   b_ed_move_player_south->setSize( 30, 30 );
   b_ed_move_player_south->setPosition( config::width() - 90, 150 );

   b_ed_move_player_east->setSize( 30, 30 );
   b_ed_move_player_east->setPosition( config::width() - 50, 130 );

   b_ed_move_player_west->setSize( 30, 30 );
   b_ed_move_player_west->setPosition( config::width() - 130, 130 );

   // Win conditions

   txt_mark_location_wc->setPosition( config::width() - 210, 210 );

   b_ed_mark_location_wc->setSize( 30, 30 );
   b_ed_mark_location_wc->setPosition( config::width() - 50, 210 );

   txt_mark_unit_wc->setPosition( config::width() - 210, 250 );

   b_ed_mark_unit_wc->setSize( 30, 30 );
   b_ed_mark_unit_wc->setPosition( config::width() - 50, 250 );

   // Unit info
   txt_unit_turn->setPosition( config::width() - 210, config::height() - 250 );

   b_ed_turn_unit_north->setSize( 30, 30 );
   b_ed_turn_unit_north->setPosition( config::width() - 90, config::height() - 270 );

   b_ed_turn_unit_south->setSize( 30, 30 );
   b_ed_turn_unit_south->setPosition( config::width() - 90, config::height() - 230 );

   b_ed_turn_unit_east->setSize( 30, 30 );
   b_ed_turn_unit_east->setPosition( config::width() - 50, config::height() - 250 );

   b_ed_turn_unit_west->setSize( 30, 30 );
   b_ed_turn_unit_west->setPosition( config::width() - 130, config::height() - 250 );

   txt_unit_team->setPosition( config::width() - 210, config::height() - 190 );

   b_ed_dec_unit_team->setSize( 30, 30 );
   b_ed_dec_unit_team->setPosition( config::width() - 130, config::height() - 190 );

   txt_unit_team_value->setPosition( config::width() - 90, config::height() - 190 );

   b_ed_inc_unit_team->setSize( 30, 30 );
   b_ed_inc_unit_team->setPosition( config::width() - 40, config::height() - 190 );

   b_ed_write_level->setPosition( config::width() - 210, config::height() - 120 );
   b_ed_write_level->setSize( 200, 50 );
   b_ed_write_level->centerText();

   b_ed_clear_level->setPosition( config::width() - 210, config::height() - 60 );
   b_ed_clear_level->setSize( 200, 50 );
   b_ed_clear_level->centerText();
}

bool isMouseOverLevelGui( int x, int y )
{
   if (x >= config::width() - 220)
      return true;

   return false;
}

int initLevelEditorGui()
{
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();
   IMGuiManager &gui_manager = IMGuiManager::getSingleton();

   b_editor_gui_area = new IMEdgeButton();
   b_editor_gui_area->setAllTextures( t_manager.getTexture( "UICenterBrown.png" ) );
   b_editor_gui_area->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_editor_gui_area->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_editor_gui_area->setEdgeWidth( 3 );
   gui_manager.registerWidget( "Level Editor Area", b_editor_gui_area);

   b_ed_dec_x_dim = new IMButton();
   b_ed_dec_x_dim->setAllTextures( t_manager.getTexture( "OrderTurnWest.png" ) );
   gui_manager.registerWidget( "L.E. Dec X Dim", b_ed_dec_x_dim);

   b_ed_inc_x_dim = new IMButton();
   b_ed_inc_x_dim->setAllTextures( t_manager.getTexture( "OrderTurnEast.png" ) );
   gui_manager.registerWidget( "L.E. Inc X Dim", b_ed_inc_x_dim);

   b_ed_dec_y_dim = new IMButton();
   b_ed_dec_y_dim->setAllTextures( t_manager.getTexture( "OrderTurnWest.png" ) );
   gui_manager.registerWidget( "L.E. Dec Y Dim", b_ed_dec_y_dim);

   b_ed_inc_y_dim = new IMButton();
   b_ed_inc_y_dim->setAllTextures( t_manager.getTexture( "OrderTurnEast.png" ) );
   gui_manager.registerWidget( "L.E. Inc Y Dim", b_ed_inc_y_dim);

   b_ed_move_player_north = new IMButton();
   b_ed_move_player_north->setAllTextures( t_manager.getTexture( "OrderTurnNorth.png" ) );
   gui_manager.registerWidget( "L.E. Move Player North", b_ed_move_player_north);

   b_ed_move_player_south = new IMButton();
   b_ed_move_player_south->setAllTextures( t_manager.getTexture( "OrderTurnSouth.png" ) );
   gui_manager.registerWidget( "L.E. Move Player South", b_ed_move_player_south);

   b_ed_move_player_east = new IMButton();
   b_ed_move_player_east->setAllTextures( t_manager.getTexture( "OrderTurnEast.png" ) );
   gui_manager.registerWidget( "L.E. Move Player East", b_ed_move_player_east);

   b_ed_move_player_west = new IMButton();
   b_ed_move_player_west->setAllTextures( t_manager.getTexture( "OrderTurnWest.png" ) );
   gui_manager.registerWidget( "L.E. Move Player West", b_ed_move_player_west);

   b_ed_mark_location_wc = new IMButton();
   b_ed_mark_location_wc->setAllTextures( t_manager.getTexture( "CountButtonBase.png" ) );
   gui_manager.registerWidget( "L.E. Mark Location Win Condition", b_ed_mark_location_wc );

   b_ed_mark_unit_wc = new IMButton();
   b_ed_mark_unit_wc->setAllTextures( t_manager.getTexture( "CountButtonBase.png" ) );
   gui_manager.registerWidget( "L.E. Mark Unit Win Condition", b_ed_mark_unit_wc );

   b_ed_turn_unit_north = new IMButton();
   b_ed_turn_unit_north->setAllTextures( t_manager.getTexture( "OrderTurnNorth.png" ) );
   gui_manager.registerWidget( "L.E. Turn Unit North", b_ed_turn_unit_north);

   b_ed_turn_unit_south = new IMButton();
   b_ed_turn_unit_south->setAllTextures( t_manager.getTexture( "OrderTurnSouth.png" ) );
   gui_manager.registerWidget( "L.E. Turn Unit South", b_ed_turn_unit_south);

   b_ed_turn_unit_east = new IMButton();
   b_ed_turn_unit_east->setAllTextures( t_manager.getTexture( "OrderTurnEast.png" ) );
   gui_manager.registerWidget( "L.E. Turn Unit East", b_ed_turn_unit_east);

   b_ed_turn_unit_west = new IMButton();
   b_ed_turn_unit_west->setAllTextures( t_manager.getTexture( "OrderTurnWest.png" ) );
   gui_manager.registerWidget( "L.E. Turn Unit West", b_ed_turn_unit_west);

   b_ed_dec_unit_team = new IMButton();
   b_ed_dec_unit_team->setAllTextures( t_manager.getTexture( "OrderTurnWest.png" ) );
   gui_manager.registerWidget( "L.E. Inc Unit Team", b_ed_dec_unit_team);

   b_ed_inc_unit_team = new IMButton();
   b_ed_inc_unit_team->setAllTextures( t_manager.getTexture( "OrderTurnEast.png" ) );
   gui_manager.registerWidget( "L.E. Inc Unit Team", b_ed_inc_unit_team);

   txt_x_dim = new Text();
   txt_x_dim->setFont( *menu_font );
   txt_x_dim->setColor( Color::Black );
   txt_x_dim->setCharacterSize( 24 );
   txt_x_dim->setString( String( "X Dim:" ) );

   txt_x_dim_value = new Text();
   txt_x_dim_value->setFont( *menu_font );
   txt_x_dim_value->setColor( Color::Black );
   txt_x_dim_value->setCharacterSize( 24 );

   txt_y_dim = new Text();
   txt_y_dim->setFont( *menu_font );
   txt_y_dim->setColor( Color::Black );
   txt_y_dim->setCharacterSize( 24 );
   txt_y_dim->setString( String( "Y Dim:" ) );

   txt_y_dim_value = new Text();
   txt_y_dim_value->setFont( *menu_font );
   txt_y_dim_value->setColor( Color::Black );
   txt_y_dim_value->setCharacterSize( 24 );

   txt_move_player = new Text();
   txt_move_player->setFont( *menu_font );
   txt_move_player->setColor( Color::Black );
   txt_move_player->setCharacterSize( 16 );
   txt_move_player->setString( String( "Move Player" ) );

   txt_mark_location_wc = new Text();
   txt_mark_location_wc->setFont( *menu_font );
   txt_mark_location_wc->setColor( Color::Black );
   txt_mark_location_wc->setCharacterSize( 16 );
   txt_mark_location_wc->setString( String( "Mark Loc. WC" ) );

   txt_mark_unit_wc = new Text();
   txt_mark_unit_wc->setFont( *menu_font );
   txt_mark_unit_wc->setColor( Color::Black );
   txt_mark_unit_wc->setCharacterSize( 16 );
   txt_mark_unit_wc->setString( String( "Mark Unit WC" ) );

   txt_unit_turn = new Text();
   txt_unit_turn->setFont( *menu_font );
   txt_unit_turn->setColor( Color::Black );
   txt_unit_turn->setCharacterSize( 24 );
   txt_unit_turn->setString( String( "Turn" ) );

   txt_unit_team = new Text();
   txt_unit_team->setFont( *menu_font );
   txt_unit_team->setColor( Color::Black );
   txt_unit_team->setCharacterSize( 24 );
   txt_unit_team->setString( String( "Team:" ) );

   txt_unit_team_value = new Text();
   txt_unit_team_value->setFont( *menu_font );
   txt_unit_team_value->setColor( Color::Black );
   txt_unit_team_value->setCharacterSize( 24 );

   b_ed_write_level = new IMEdgeTextButton();
   b_ed_write_level->setAllTextures( t_manager.getTexture( "UICenterBrown.png" ) );
   b_ed_write_level->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_ed_write_level->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_ed_write_level->setEdgeWidth( 3 );
   b_ed_write_level->setText( &s_ed_write_level );
   b_ed_write_level->setFont( menu_font );
   b_ed_write_level->setTextSize( 24 );
   b_ed_write_level->setTextColor( sf::Color::Black );
   gui_manager.registerWidget( "Editor Write Level to File", b_ed_write_level);

   b_ed_clear_level = new IMEdgeTextButton();
   b_ed_clear_level->setAllTextures( t_manager.getTexture( "UICenterBrown.png" ) );
   b_ed_clear_level->setCornerAllTextures( t_manager.getTexture( "UICornerBrown3px.png" ) );
   b_ed_clear_level->setEdgeAllTextures( t_manager.getTexture( "UIEdgeBrown3px.png" ) );
   b_ed_clear_level->setEdgeWidth( 3 );
   b_ed_clear_level->setText( &s_ed_clear_level );
   b_ed_clear_level->setFont( menu_font );
   b_ed_clear_level->setTextSize( 24 );
   b_ed_clear_level->setTextColor( sf::Color::Black );
   gui_manager.registerWidget( "Editor Clear Level", b_ed_clear_level);

   sp_ed_unit = new Sprite();

   fitGui_LevelEditor();

   init_level_editor_gui = true;

   return 0;
}

Vector2i l_e_selection;
bool l_e_selected;

bool l_e_selected_loc_wc = false;
bool l_e_selected_unit_wc = false;

void levelEditorUpdateOnSelection()
{
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();
   // Update location WC button
   if (win_if_reached.x == l_e_selection.x && win_if_reached.y == l_e_selection.y) {
      // We have selected the current win_if_reached location
      b_ed_mark_location_wc->setAllTextures( t_manager.getTexture( "GuiExitX.png" ) );
      l_e_selected_loc_wc = true;
   } else {
      b_ed_mark_location_wc->setAllTextures( t_manager.getTexture( "CountButtonBase.png" ) );
      l_e_selected_loc_wc = false;
   }

   // Update unit WC button
   Unit *u = GRID_AT(unit_grid,l_e_selection.x,l_e_selection.y);
   if (find(win_if_killed.begin(), win_if_killed.end(), u) != win_if_killed.end()) {
      // This unit is already a target
      b_ed_mark_unit_wc->setAllTextures( t_manager.getTexture( "GuiExitX.png" ) );
      l_e_selected_unit_wc = true;
   } else {
      b_ed_mark_unit_wc->setAllTextures( t_manager.getTexture( "CountButtonBase.png" ) );
      l_e_selected_unit_wc = false;
   }
}

int levelEditorSelectGrid( Vector2f coords )
{
   int cx = (int)coords.x,
       cy = (int)coords.y;

   if (cx < 0 || cx >= level_dim_x || cy < 0 || cy >= level_dim_y) {
      l_e_selected = false;
      return -1;
   }

   l_e_selection.x = cx;
   l_e_selection.y = cy;
   l_e_selected = true;

   levelEditorUpdateOnSelection();

   return 0;
}

int levelEditorChangeTerrain( int change )
{
   if (!l_e_selected) {
      return -1;
   }

   int cur_t = GRID_AT(terrain_grid,l_e_selection.x,l_e_selection.y);
   int new_t = cur_t + change;

   if (new_t < 0)
      new_t = 0;
   if (new_t >= NUM_TERRAINS)
      new_t = NUM_TERRAINS - 1;

   GRID_AT(terrain_grid,l_e_selection.x,l_e_selection.y) = (Terrain)new_t;

   return 0;
}

Terrain terrain_copy = TER_NONE;

int levelEditorCopyTerrain()
{
   if (l_e_selected) {
      terrain_copy = GRID_AT(terrain_grid,l_e_selection.x,l_e_selection.y);
      return 0;
   }
   return -1;
}

int levelEditorPasteTerrain()
{
   if (l_e_selected) {
      GRID_AT(terrain_grid,l_e_selection.x,l_e_selection.y) = terrain_copy;
      return 0;
   }
   return -1;
}

// Unit modification
int levelEditorNextUnit()
{
   if (l_e_selected == false) return -1;

   Unit *cur_unit = GRID_AT(unit_grid,l_e_selection.x,l_e_selection.y);
   UnitType new_unit_type;

   if (cur_unit == NULL)
      new_unit_type = MONSTER_T;
   else
      new_unit_type = UnitType((int)cur_unit->type + 1);

   list<Unit*>::iterator it = find( unit_list.begin(), unit_list.end(), cur_unit );
   if (it != unit_list.end()) removeUnit( it );
   addUnit( genBaseUnit( new_unit_type, l_e_selection.x, l_e_selection.y, SOUTH ) );
   return 0;
}

int levelEditorPreviousUnit()
{
   if (l_e_selected == false) return -1;

   Unit *cur_unit = GRID_AT(unit_grid,l_e_selection.x,l_e_selection.y);
   UnitType new_unit_type;

   if (cur_unit == NULL)
      new_unit_type = M_DWARF_SPEAR_T;
   else
      new_unit_type = UnitType((int)cur_unit->type - 1);

   list<Unit*>::iterator it = find( unit_list.begin(), unit_list.end(), cur_unit );
   if (it != unit_list.end()) removeUnit( it );
   addUnit( genBaseUnit( new_unit_type, l_e_selection.x, l_e_selection.y, SOUTH ) );
   return 0;

   return 0;
}

int levelEditorDeleteUnit()
{
   if (l_e_selected == false) return -1;

   Unit *cur_unit = GRID_AT(unit_grid,l_e_selection.x,l_e_selection.y);

   list<Unit*>::iterator it = find( unit_list.begin(), unit_list.end(), cur_unit );
   if (it != unit_list.end()) removeUnit( it );

   return 0;
}

int loadLevelEditor( int level )
{
   loadLevel( level );

   menu_state = MENU_MAIN | MENU_PRI_LEVEL_EDITOR;
   setLevelListener(false);
   setLevelEditorListener(true);

   vision_enabled = false;
   calculateVision();
   
   return 0;
}

int levelEditorWriteToFile()
{
   string level_name_out = "res/level_editor_output.lvl";

   return writeLevelToFile( level_name_out );
}

void drawLevelEditorGui()
{
   RenderWindow *gui_window = SFML_GlobalRenderWindow::get();

   if (l_e_selected) {
      RectangleShape rect;
      rect.setSize( Vector2f( 1, 1 ) );
      rect.setPosition( l_e_selection.x, l_e_selection.y );
      rect.setFillColor( Color::Transparent );
      rect.setOutlineColor( Color::Yellow );
      rect.setOutlineThickness( 0.05 );
      SFML_GlobalRenderWindow::get()->draw( rect );
   }

   gui_window->setView(gui_window->getDefaultView());

   if (init_level_editor_gui == false)
      initLevelEditorGui();

   stringstream ss;
   ss << level_dim_x;
   txt_x_dim_value->setString( String( ss.str() ) );
   stringstream ss2;
   ss2 << level_dim_y;
   txt_y_dim_value->setString( String( ss2.str() ) );

   IMGuiManager::getSingleton().pushSprite( txt_x_dim );
   IMGuiManager::getSingleton().pushSprite( txt_x_dim_value );
   IMGuiManager::getSingleton().pushSprite( txt_y_dim );
   IMGuiManager::getSingleton().pushSprite( txt_y_dim_value );
   IMGuiManager::getSingleton().pushSprite( txt_move_player );

   if (b_ed_dec_x_dim->doWidget())
      changeLevelDimensions( level_dim_x - 1, level_dim_y );
   if (b_ed_inc_x_dim->doWidget())
      changeLevelDimensions( level_dim_x + 1, level_dim_y );
   if (b_ed_dec_y_dim->doWidget())
      changeLevelDimensions( level_dim_x, level_dim_y - 1 );
   if (b_ed_inc_y_dim->doWidget())
      changeLevelDimensions( level_dim_x, level_dim_y + 1 );

   bool move_player = false;
   int to_x = player->x_grid, to_y = player->y_grid;
   if (b_ed_move_player_north->doWidget()) {
      move_player = true;
      to_y--;
   }
   if (b_ed_move_player_south->doWidget()) {
      move_player = true;
      to_y++;
   }
   if (b_ed_move_player_west->doWidget()) {
      move_player = true;
      to_x--;
   }
   if (b_ed_move_player_east->doWidget()) {
      move_player = true;
      to_x++;
   }

   if (move_player && to_x >= 0 && to_x < level_dim_x && to_y >= 0 && to_y < level_dim_y) {
      if (GRID_AT(unit_grid,to_x,to_y) == NULL) {
         GRID_AT(unit_grid,to_x,to_y) = player;
         GRID_AT(unit_grid,player->x_grid,player->y_grid) = NULL;
         player->x_grid = to_x;
         player->y_grid = to_y;
         player->x_real = to_x + 0.5;
         player->y_real = to_y + 0.5;
      }
   }

   if (l_e_selected) {
      IMGuiManager::getSingleton().pushSprite( txt_mark_location_wc );
      if (b_ed_mark_location_wc->doWidget()) {
         if (l_e_selected_loc_wc)
            addLocationWinCondition( -1, -1 );
         else
            addLocationWinCondition( l_e_selection.x, l_e_selection.y );

         levelEditorUpdateOnSelection();
      }

      Unit *u = GRID_AT(unit_grid,l_e_selection.x,l_e_selection.y);
      if (u != NULL) {
         if (b_ed_mark_unit_wc->doWidget()) {
            if (l_e_selected_unit_wc)
               removeUnitWinCondition( u );
            else
               addUnitWinCondition( u );

            levelEditorUpdateOnSelection();
         }

         sp_ed_unit->setTexture( *u->getTexture() );
         normalizeTo1x1( sp_ed_unit );
         sp_ed_unit->scale( 128, 128 );
         sp_ed_unit->setPosition( config::width() - 169, config::height() - 400 );

         if (b_ed_turn_unit_north->doWidget())
            u->turnTo(NORTH);
         if (b_ed_turn_unit_south->doWidget())
            u->turnTo(SOUTH);
         if (b_ed_turn_unit_east->doWidget())
            u->turnTo(EAST);
         if (b_ed_turn_unit_west->doWidget())
            u->turnTo(WEST);

         stringstream ss3;
         ss3 << u->team;
         txt_unit_team_value->setString( String( ss3.str() ) );

         IMGuiManager::getSingleton().pushSprite( sp_ed_unit );
         IMGuiManager::getSingleton().pushSprite( txt_mark_unit_wc );
         IMGuiManager::getSingleton().pushSprite( txt_unit_turn );
         IMGuiManager::getSingleton().pushSprite( txt_unit_team );
         IMGuiManager::getSingleton().pushSprite( txt_unit_team_value );

         if (b_ed_dec_unit_team->doWidget())
            u->team--;
         if (b_ed_inc_unit_team->doWidget())
            u->team++;
      }
   }

   if (b_ed_write_level->doWidget())
      levelEditorWriteToFile();
   if (b_ed_clear_level->doWidget()) {
      initGrids( level_dim_x, level_dim_y );
      unit_list.clear();
   }

   b_editor_gui_area->doWidget();
}

//////////////////////////////////////////////////////////////////////
// Draw ---

void drawBaseTerrain()
{
   Sprite *s_ter;
   if (base_terrain == BASE_TER_GRASS)
         s_ter = base_grass_sprite;
   if (base_terrain == BASE_TER_MOUNTAIN)
         s_ter = base_mountain_sprite;
   if (base_terrain == BASE_TER_UNDERGROUND)
         s_ter = base_underground_sprite;
   
   s_ter->setPosition( 0, 0 );
   s_ter->setScale( level_dim_x, level_dim_y ); 
   SFML_GlobalRenderWindow::get()->draw( *s_ter );
}

void drawTerrain()
{
   RenderWindow *r_window = SFML_GlobalRenderWindow::get();
   int x, y;
   for (x = 0; x < level_dim_x; ++x) {
      for (y = 0; y < level_dim_y; ++y) {

         Sprite *s_ter = terrain_sprites[GRID_AT(terrain_grid,x,y)];
      
         if (NULL != s_ter) {
            s_ter->setPosition( x, y );
            r_window->draw( *s_ter );
         }

         Sprite *s_ter_mod = terrain_mod_sprites[GRID_AT(terrain_mod_grid,x,y)];
      
         if (NULL != s_ter_mod) {
            s_ter_mod->setPosition( x, y );
            r_window->draw( *s_ter_mod );
         }

      }
   }
}

void drawUnits()
{
   if (NULL != player)
      player->draw();

   if (NULL != summonMarker)
      summonMarker->draw();

   for (list<Unit*>::iterator it=unit_list.begin(); it != unit_list.end(); ++it)
   {
      Unit* unit = (*it);
      if (unit && (!vision_enabled || // Don't draw units I can't see
                     (!(unit->invis && unit->team != 0) && 
                     GRID_AT(vision_grid,unit->x_grid,unit->y_grid) == VIS_VISIBLE))) {
         unit->draw();
      }
   }
}

void drawEffects()
{
   for (list<Effect*>::iterator it=effect_list.begin(); it != effect_list.end(); ++it)
   {
      Effect* effect = (*it);
      if (effect) {
         effect->draw();
      }
   }
}

int drawFog()
{
   if (false == vision_enabled) return -1;

   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();
   RenderWindow *r_window = SFML_GlobalRenderWindow::get();

   Sprite s_unseen( *(t_manager.getTexture( "FogCloudSolid.png" )) ),
          s_dark( *(t_manager.getTexture( "FogCloudDark.png" )) ),
          s_fog_base( *(t_manager.getTexture( "Fog.png" )) ),
          s_fog_left( *(t_manager.getTexture( "FogCloudTransparentLeft.png" )) ),
          s_fog_right( *(t_manager.getTexture( "FogCloudTransparentRight.png" )) ),
          s_fog_top( *(t_manager.getTexture( "FogCloudTransparentTop.png" )) ),
          s_fog_bottom( *(t_manager.getTexture( "FogCloudTransparentBottom.png" )) );
   normalizeTo1x1( &s_unseen );
   s_unseen.scale( 1.3, 1.3 );
   normalizeTo1x1( &s_dark );
   s_dark.scale( 1.3, 1.3 );
   normalizeTo1x1( &s_fog_base );
   normalizeTo1x1( &s_fog_left );
   normalizeTo1x1( &s_fog_right );
   normalizeTo1x1( &s_fog_top );
   normalizeTo1x1( &s_fog_bottom );
   int x, y;
   for (x = 0; x < level_dim_x; ++x) {
      for (y = 0; y < level_dim_y; ++y) {
         Vision v = GRID_AT(vision_grid,x,y);
      
         if (v == VIS_NONE || v == VIS_OFFMAP || v == VIS_NEVER_SEEN) {
            s_unseen.setPosition( x - 0.17, y - 0.17 );
            r_window->draw( s_unseen );
         }
         else if (v == VIS_SEEN_BEFORE) {
            s_fog_base.setPosition( x, y );
            r_window->draw( s_fog_base );
         }
         else
         {
            // Draw fog edges for adjacent semi-fogged areas
            if (x != 0 && GRID_AT(vision_grid,(x-1),y) == VIS_SEEN_BEFORE) {
               s_fog_right.setPosition( x, y );
               r_window->draw( s_fog_right );
            }
            if (y != 0 && GRID_AT(vision_grid,x,(y-1)) == VIS_SEEN_BEFORE) {
               s_fog_bottom.setPosition( x, y );
               r_window->draw( s_fog_bottom );
            }
            if (x != level_dim_x - 1 && GRID_AT(vision_grid,(x+1),y) == VIS_SEEN_BEFORE) {
               s_fog_left.setPosition( x, y );
               r_window->draw( s_fog_left );
            }
            if (y != level_dim_y - 1 && GRID_AT(vision_grid,x,(y+1)) == VIS_SEEN_BEFORE) {
               s_fog_top.setPosition( x, y );
               r_window->draw( s_fog_top );
            }
         }
      }
   }

   for (x = -1; x <= level_dim_x; ++x) {
      s_dark.setPosition( x - 0.17, -1 );
      r_window->draw( s_dark );
      s_dark.setPosition( x - 0.17, level_dim_y - 0.17 );
      r_window->draw( s_dark );
   }
   for (y = 0; y < level_dim_y; ++y) {
      s_dark.setPosition( -1.17, y - 0.17 );
      r_window->draw( s_dark );
      s_dark.setPosition( level_dim_x - 0.17, y - 0.17 );
      r_window->draw( s_dark );
   }

   return 0;
}

int drawVictory()
{
   Text victory_text;
   victory_text.setFont( *menu_font );
   victory_text.setCharacterSize( 120 );
   victory_text.setString( String("Victory") );

   // Shadow
   victory_text.setPosition( config::width() / 2 - 190, config::height() / 2 - 102 );
   victory_text.setColor( Color( 75, 75, 75, 127 ) );
   SFML_GlobalRenderWindow::get()->draw( victory_text );

   // Real
   victory_text.setPosition( config::width() / 2 - 186, config::height() / 2 - 100 );
   victory_text.setColor( Color( 215, 0, 0, 255 ) );
   SFML_GlobalRenderWindow::get()->draw( victory_text );

   if (b_victory_back_to_map->doWidget()) {
      clearAll();
      menu_state = MENU_MAIN | MENU_PRI_MAP;
      setLevelListener( false );
      return -3;
   }
   return 0;
}

int drawLevel()
{
   SFML_GlobalRenderWindow::get()->setView(*level_view);
   SFML_GlobalRenderWindow::get()->clear( Color( 64, 64, 64 ) );
   // Level
   drawBaseTerrain();
   drawTerrain();
   drawUnits();
   drawEffects();
   drawFog();

   // Gui

   if (menu_state & MENU_PRI_LEVEL_EDITOR)
      drawLevelEditorGui();
   else {
      drawGui();
      if (victory)
         return drawVictory();
   }

   return 0;
} 

//////////////////////////////////////////////////////////////////////
// Event listener ---

struct LevelEventHandler : public My_SFML_MouseListener, public My_SFML_KeyListener
{
   virtual bool keyPressed( const Event::KeyEvent &key_press )
   {
      if (key_press.code == Keyboard::LShift || key_press.code == Keyboard::RShift)
         key_shift_down = 1;

      KeybindTarget kb = config::getBoundTarget( key_press.code );

      if (kb == KB_FORCE_QUIT)
         shutdown(1,1);

      // View movement
      else if (kb == KB_MOVE_CAMERA_RIGHT)
         shiftView( 2, 0 );
      else if (kb == KB_MOVE_CAMERA_LEFT)
         shiftView( -2, 0 );
      else if (kb == KB_MOVE_CAMERA_DOWN)
         shiftView( 0, 2 );
      else if (kb == KB_MOVE_CAMERA_UP)
         shiftView( 0, -2 );
      else if (kb == KB_ZOOM_IN_CAMERA)
         zoomView( 1 , level_view->getCenter());
      else if (kb == KB_ZOOM_OUT_CAMERA)
         zoomView( -1 , level_view->getCenter());

      // Pause
      else if (kb == KB_PAUSE)
         togglePause();

      // Counts
      else if (kb == KB_BTN_COUNT_0)
         playerAddCount( 0 );
      else if (kb == KB_BTN_COUNT_1)
         playerAddCount( 1 );
      else if (kb == KB_BTN_COUNT_2)
         playerAddCount( 2 );
      else if (kb == KB_BTN_COUNT_3)
         playerAddCount( 3 );
      else if (kb == KB_BTN_COUNT_4)
         playerAddCount( 4 );
      else if (kb == KB_BTN_COUNT_5)
         playerAddCount( 5 );
      else if (kb == KB_BTN_COUNT_6)
         playerAddCount( 6 );
      else if (kb == KB_BTN_COUNT_7)
         playerAddCount( 7 );
      else if (kb == KB_BTN_COUNT_8)
         playerAddCount( 8 );
      else if (kb == KB_BTN_COUNT_9)
         playerAddCount( 9 );
      else if (kb == KB_BTN_COUNT_CLEAR)
         playerSetCount( 0 );
      else if (kb == KB_BTN_COUNT_INFINITE)
         playerSetCount( -1 );

      // Conditions
      else if (kb == KB_BTN_COND_ENEMY_ADJACENT)
         playerAddConditional( ENEMY_ADJACENT );
      else if (kb == KB_BTN_COND_ENEMY_AHEAD)
         playerAddConditional( ENEMY_AHEAD ); 
      else if (kb == KB_BTN_COND_ENEMY_IN_RANGE)
         playerAddConditional( ENEMY_IN_RANGE );
      else if (kb == KB_BTN_COND_HEALTH_UNDER_50)
         playerAddConditional( HEALTH_UNDER_50 );
      else if (kb == KB_BTN_COND_HEALTH_UNDER_20)
         playerAddConditional( HEALTH_UNDER_20 );
      else if (kb == KB_BTN_COND_BLOCKED_AHEAD)
         playerAddConditional( BLOCKED_AHEAD );
      else if (kb == KB_BTN_COND_CLEAR)
         playerAddConditional( TRUE );

      // Control
      else if (kb == KB_BTN_START_BLOCK)
         playerAddOrder( START_BLOCK );
      else if (kb == KB_BTN_END_BLOCK)
         playerAddOrder( END_BLOCK );
      else if (kb == KB_BTN_REPEAT)
         playerAddOrder( REPEAT );

      // Movement
      else if (kb == KB_BTN_MOVE_FORWARD)
         playerAddOrder( MOVE_FORWARD );
      else if (kb == KB_BTN_MOVE_BACK)
         playerAddOrder( MOVE_BACK );
      else if (kb == KB_BTN_TURN_NORTH)
         playerAddOrder( TURN_NORTH );
      else if (kb == KB_BTN_TURN_EAST)
         playerAddOrder( TURN_EAST );
      else if (kb == KB_BTN_TURN_SOUTH)
         playerAddOrder( TURN_SOUTH );
      else if (kb == KB_BTN_TURN_WEST)
         playerAddOrder( TURN_WEST );
      else if (kb == KB_BTN_TURN_NEAREST_ENEMY)
         playerAddOrder( TURN_NEAREST_ENEMY );
      else if (kb == KB_BTN_FOLLOW_PATH)
         playerAddOrder( FOLLOW_PATH );
      else if (kb == KB_BTN_WAIT)
         playerAddOrder( WAIT );

      // Attack
      else if (kb == KB_BTN_ATTACK_SMALLEST)
         playerAddOrder( ATTACK_SMALLEST );
      else if (kb == KB_BTN_ATTACK_BIGGEST)
         playerAddOrder( ATTACK_BIGGEST );
      else if (kb == KB_BTN_ATTACK_CLOSEST)
         playerAddOrder( ATTACK_CLOSEST );
      else if (kb == KB_BTN_ATTACK_FARTHEST)
         playerAddOrder( ATTACK_FARTHEST );
      else if (kb == KB_BTN_ATTACK_MOST_ARMORED)
         playerAddOrder( ATTACK_MOST_ARMORED );
      else if (kb == KB_BTN_ATTACK_LEAST_ARMORED)
         playerAddOrder( ATTACK_LEAST_ARMORED );

      // Player cmd box
      else if (kb == KB_BTN_PL_ALERT_ALL)
         playerAddOrder( PL_ALERT_ALL );
      else if (kb == KB_BTN_PL_GO)
         playerAddOrder( PL_CMD_GO );
      else if (kb == KB_BTN_PL_GO_ALL)
         playerAddOrder( PL_CMD_GO_ALL );
      else if (kb == KB_BTN_PL_SET_GROUP)
         playerAddOrder( PL_SET_GROUP );
      else if (kb == KB_BTN_PL_DELAY)
         playerAddOrder( PL_DELAY );

      // Monster
      else if (kb == KB_BTN_PL_ALERT_MONSTERS)
         playerAddOrder( PL_ALERT_MONSTERS );
      else if (kb == KB_BTN_PL_GO_MONSTERS)
         playerAddOrder( PL_CMD_GO_MONSTERS );
      else if (kb == KB_BTN_MONSTER_GUARD)
         playerAddOrder( MONSTER_GUARD );
      else if (kb == KB_BTN_MONSTER_BURST)
         playerAddOrder( MONSTER_BURST );

      // Soldier
      else if (kb == KB_BTN_PL_ALERT_SOLDIERS)
         playerAddOrder( PL_ALERT_SOLDIERS );
      else if (kb == KB_BTN_PL_GO_SOLDIERS)
         playerAddOrder( PL_CMD_GO_SOLDIERS );
      else if (kb == KB_BTN_SOLDIER_SWITCH_AXE)
         playerAddOrder( SOLDIER_SWITCH_AXE );
      else if (kb == KB_BTN_SOLDIER_SWITCH_SPEAR)
         playerAddOrder( SOLDIER_SWITCH_SPEAR );
      else if (kb == KB_BTN_SOLDIER_SWITCH_BOW)
         playerAddOrder( SOLDIER_SWITCH_BOW );

      // Worm
      else if (kb == KB_BTN_PL_ALERT_WORMS)
         playerAddOrder( PL_ALERT_WORMS );
      else if (kb == KB_BTN_PL_GO_WORMS)
         playerAddOrder( PL_CMD_GO_WORMS );
      else if (kb == KB_BTN_WORM_HIDE)
         playerAddOrder( WORM_HIDE );
      else if (kb == KB_BTN_WORM_TRAIL_START)
         playerAddOrder( WORM_TRAIL_START );
      else if (kb == KB_BTN_WORM_TRAIL_END)
         playerAddOrder( WORM_TRAIL_END );

      // Bird
      else if (kb == KB_BTN_PL_ALERT_BIRDS)
         playerAddOrder( PL_ALERT_BIRDS );
      else if (kb == KB_BTN_PL_GO_BIRDS)
         playerAddOrder( PL_CMD_GO_BIRDS );
      else if (kb == KB_BTN_BIRD_FLY)
         playerAddOrder( BIRD_FLY );
      else if (kb == KB_BTN_BIRD_CMD_SHOUT)
         playerAddOrder( BIRD_CMD_SHOUT );
      else if (kb == KB_BTN_BIRD_CMD_QUIET)
         playerAddOrder( BIRD_CMD_QUIET );

      // Bug
      else if (kb == KB_BTN_PL_ALERT_BUGS)
         playerAddOrder( PL_ALERT_BUGS );
      else if (kb == KB_BTN_PL_GO_BUGS)
         playerAddOrder( PL_CMD_GO_BUGS );
      else if (kb == KB_BTN_BUG_MEDITATE)
         playerAddOrder( BUG_MEDITATE );
      else if (kb == KB_BTN_BUG_CAST_FIREBALL)
         playerAddOrder( BUG_CAST_FIREBALL );
      else if (kb == KB_BTN_BUG_CAST_SHOCK)
         playerAddOrder( BUG_CAST_SHOCK );
      else if (kb == KB_BTN_BUG_CAST_DUST)
         playerAddOrder( BUG_CAST_DUST );
      else if (kb == KB_BTN_BUG_OPEN_WORMHOLE)
         playerAddOrder( BUG_OPEN_WORMHOLE );
      else if (kb == KB_BTN_BUG_CLOSE_WORMHOLE)
         playerAddOrder( BUG_CLOSE_WORMHOLE );

      else if (kb == KB_SHOW_KEYBINDINGS)
         config::show_keybindings = true;

      // Debugging
      else if (kb == KB_DEBUG_TOGGLE_FOG)
         vision_enabled = !vision_enabled;
      else if (kb == KB_DEBUG_TOGGLE_FRAMERATE)
         show_framerate = !show_framerate;

      return true;
   }
    
   virtual bool keyReleased( const Event::KeyEvent &key_release )
   {
      if (key_release.code == Keyboard::LShift || key_release.code == Keyboard::RShift)
         key_shift_down = 0;

      KeybindTarget kb = config::getBoundTarget( key_release.code );

      if (kb == KB_SHOW_KEYBINDINGS)
         config::show_keybindings = false;

      return true;
   }

   virtual bool mouseMoved( const Event::MouseMoveEvent &mouse_move )
   {
      static int old_mouse_x, old_mouse_y;

      if (left_mouse_down) {
         Vector2f old_spot = coordsWindowToView( old_mouse_x, old_mouse_y );
         Vector2f new_spot = coordsWindowToView( mouse_move.x, mouse_move.y );
         shiftView( old_spot.x - new_spot.x, old_spot.y - new_spot.y );
      }

      if ((right_mouse_down && cast_menu_open) || cast_menu_solid) {
         castMenuSelect( mouse_move.x, mouse_move.y );
      }

      old_mouse_x = mouse_move.x;
      old_mouse_y = mouse_move.y;

      return true;
   }

   virtual bool mouseButtonPressed( const Event::MouseButtonEvent &mbp )
   {
      if (isMouseOverGui( mbp.x, mbp.y ))
         return false;

      if (mbp.button == Mouse::Left && key_shift_down == 0) {
         left_mouse_down = 1;
         left_mouse_down_time = game_clock->getElapsedTime().asMilliseconds();

         if (cast_menu_solid) {
            castMenuSelect( mbp.x, mbp.y );
            castMenuChoose();
         }
      }

      if (mbp.button == Mouse::Right ||
         (mbp.button == Mouse::Left && key_shift_down == 1)) {
         right_mouse_down = 1;
         right_mouse_down_time = game_clock->getElapsedTime().asMilliseconds();
         castMenuCreate( coordsWindowToLevel( mbp.x, mbp.y ) );
      }


      return true;
   }

   virtual bool mouseButtonReleased( const Event::MouseButtonEvent &mbr )
   {
      if (left_mouse_down == 1) {
         left_mouse_down = 0;
         int left_mouse_up_time = game_clock->getElapsedTime().asMilliseconds();

         if (left_mouse_up_time - left_mouse_down_time < c_mouse_down_select_time
               && !isMouseOverGui( mbr.x, mbr.y ))
            selectUnit( coordsWindowToView( mbr.x, mbr.y ) );

      }
      if (right_mouse_down == 1) {
         right_mouse_down = 0;
         int right_mouse_up_time = game_clock->getElapsedTime().asMilliseconds();

         if (right_mouse_up_time - right_mouse_down_time < c_mouse_down_select_time)
            castMenuSolidify();
         else {
            castMenuSelect( mbr.x, mbr.y );
            castMenuChoose();
         }
      }

      return true;
   }

   virtual bool mouseWheelMoved( const Event::MouseWheelEvent &mwm )
   {
      zoomView( mwm.delta, coordsWindowToView( mwm.x, mwm.y ) );
      return true;
   }
};

struct LevelEditorEventHandler : public My_SFML_MouseListener, public My_SFML_KeyListener
{
   virtual bool keyPressed( const Event::KeyEvent &key_press )
   {
      if (key_press.code == Keyboard::Q)
         shutdown(1,1);

      if (key_press.code == Keyboard::LShift || key_press.code == Keyboard::RShift)
         key_shift_down = 1;

      // View movement
      if (key_press.code == Keyboard::Right)
         levelEditorChangeTerrain( 10 );
      if (key_press.code == Keyboard::Left)
         levelEditorChangeTerrain( -10 );
      if (key_press.code == Keyboard::Down)
         levelEditorChangeTerrain( 1 );
      if (key_press.code == Keyboard::Up)
         levelEditorChangeTerrain( -1 );
      if (key_press.code == Keyboard::BackSpace)
         levelEditorChangeTerrain( -300 );
      if (key_press.code == Keyboard::Add)
         zoomView( 1 , level_view->getCenter());
      if (key_press.code == Keyboard::Subtract)
         zoomView( -1 , level_view->getCenter());

      if (key_press.code == Keyboard::W && (menu_state & MENU_PRI_LEVEL_EDITOR))
      {
         log("Writing new level data from level editor");
         levelEditorWriteToFile();
      }

      if (key_press.code == Keyboard::N)
         levelEditorNextUnit();
      if (key_press.code == Keyboard::P)
         levelEditorPreviousUnit();
      if (key_press.code == Keyboard::D)
         levelEditorDeleteUnit();

      if (key_press.code == Keyboard::C)
         levelEditorCopyTerrain();
      if (key_press.code == Keyboard::V)
         levelEditorPasteTerrain();

      return true;
   }
    
   virtual bool keyReleased( const Event::KeyEvent &key_release )
   {
      if (key_release.code == Keyboard::LShift || key_release.code == Keyboard::RShift)
         key_shift_down = 0;

      return true;
   }

   virtual bool mouseMoved( const Event::MouseMoveEvent &mouse_move )
   {
      static int old_mouse_x, old_mouse_y;

      if (left_mouse_down) {
         Vector2f old_spot = coordsWindowToView( old_mouse_x, old_mouse_y );
         Vector2f new_spot = coordsWindowToView( mouse_move.x, mouse_move.y );
         shiftView( old_spot.x - new_spot.x, old_spot.y - new_spot.y );
      }

      if (right_mouse_down && cast_menu_open) {
         //castMenuSelect( mouse_move.x, mouse_move.y );
      }

      old_mouse_x = mouse_move.x;
      old_mouse_y = mouse_move.y;

      return true;
   }

   virtual bool mouseButtonPressed( const Event::MouseButtonEvent &mbp )
   {
      if (mbp.button == Mouse::Left && key_shift_down == 0) {
         left_mouse_down = 1;
         left_mouse_down_time = game_clock->getElapsedTime().asMilliseconds();
      }

      if (mbp.button == Mouse::Right ||
         (mbp.button == Mouse::Left && key_shift_down == 1)) {
         right_mouse_down = 1;
         right_mouse_down_time = game_clock->getElapsedTime().asMilliseconds();
      }


      return true;
   }

   virtual bool mouseButtonReleased( const Event::MouseButtonEvent &mbr )
   {
      if (mbr.button == Mouse::Left && key_shift_down == 0) {
         left_mouse_down = 0;
         int left_mouse_up_time = game_clock->getElapsedTime().asMilliseconds();

         if (left_mouse_up_time - left_mouse_down_time < c_mouse_down_select_time
               && !isMouseOverLevelGui( mbr.x, mbr.y ))
            levelEditorSelectGrid( coordsWindowToView( mbr.x, mbr.y ) );

      }
      if (mbr.button == Mouse::Right ||
         (mbr.button == Mouse::Left && key_shift_down == 1)) {
         right_mouse_down = 0;
      }

      return true;
   }

   virtual bool mouseWheelMoved( const Event::MouseWheelEvent &mwm )
   {
      zoomView( mwm.delta, coordsWindowToView( mwm.x, mwm.y ) );
      return true;
   }
};

void setLevelListener( bool set )
{
   static LevelEventHandler level_listener;
   SFML_WindowEventManager& event_manager = SFML_WindowEventManager::getSingleton();
   if (set) {
      event_manager.addMouseListener( &level_listener, "level mouse" );
      event_manager.addKeyListener( &level_listener, "level key" );
   } else {
      event_manager.removeMouseListener( &level_listener );
      event_manager.removeKeyListener( &level_listener );
   }
}

void setLevelEditorListener( bool set )
{
   static LevelEditorEventHandler level_editor_listener;
   SFML_WindowEventManager& event_manager = SFML_WindowEventManager::getSingleton();
   if (set) {
      event_manager.addMouseListener( &level_editor_listener, "level editor mouse" );
      event_manager.addKeyListener( &level_editor_listener, "level editor key" );
   } else {
      event_manager.removeMouseListener( &level_editor_listener );
      event_manager.removeKeyListener( &level_editor_listener );
   }
}


};
