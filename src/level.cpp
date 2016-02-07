#include "level.h"

#include "shutdown.h"
#include "types.h"
#include "util.h"
#include "log.h"
#include "menustate.h"
#include "config.h"
#include "clock.h"

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include "SFML_GlobalRenderWindow.hpp"
#include "SFML_WindowEventManager.hpp"
#include "SFML_TextureManager.hpp"
#include "IMGuiManager.hpp"
#include "IMCursorManager.hpp"

using namespace sf;
using namespace std;

namespace robot {

#define GRID_AT(GRID,X,Y) (GRID[((X) + ((Y) * level_dim_x))])

//////////////////////////////////////////////////////////////////////
// Constants ---

const int c_fully_paused = 6;

//////////////////////////////////////////////////////////////////////
// Level Data ---

int level_init = 0;

int pause_state = 0;

View *level_view;
float view_rel_x_to_y;

int level_num = -1;
int level_dim_x = -1;
int level_dim_y = -1;
float level_fog_dim = 0;

// Grids of stuff - row major order
Terrain* terrain_grid;

// Lists
//list<Unit*> unit_list;
//list<Effect*> effect_list;

// Vision
//Unit *player = NULL;

// Debugging
bool show_framerate;

//////////////////////////////////////////////////////////////////////
// UI Data ---

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

/*
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
*/

//////////////////////////////////////////////////////////////////////
// Adding stuff ---

/*
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
*/

//////////////////////////////////////////////////////////////////////
// Vision ---


//////////////////////////////////////////////////////////////////////
// Targetting ---


//////////////////////////////////////////////////////////////////////
// Player interface ---


//////////////////////////////////////////////////////////////////////
// Loading ---

void initTextures()
{

}

void initLevel()
{
   if (level_init) return;

   level_view = new View();
   view_rel_x_to_y = ((float)config::height()) / ((float)config::width());

   initTextures();

   level_init = true;
}

void clearGrids()
{
   if (terrain_grid) {
      delete[] terrain_grid;
      terrain_grid = NULL;
   }
}

void clearUnits()
{
   /*
   list<Unit*>::iterator it;
   for (it = unit_list.begin(); it != unit_list.end(); ++it)
   {
      Unit *u = (*it);
      if (u)
         delete u;
   }

   unit_list.clear();
   */
}

void clearEffects()
{
   /*
   list<Effect*>::iterator it;
   for (it = effect_list.begin(); it != effect_list.end(); ++it)
   {
      Effect *e = (*it);
      if (e)
         delete e;
   }

   effect_list.clear();
   */
}

void clearAll()
{
   clearGrids();
   clearUnits();
   clearEffects();
}

int initGrids(int x, int y)
{
   if (x < 1 || x > c_max_dimension || y < 1 || y > c_max_dimension)
      return -1;

   clearGrids();

   level_dim_x = x;
   level_dim_y = y;

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

// Okay what is happening here
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

int updateLevel( int dt )
{
   calculateFramerate( dt );

   if (updatePause( dt ) == 2)
      return 2;

   return 0;
}

//////////////////////////////////////////////////////////////////////
// GUI ---

//////////////////////////////////////////////////////////////////////
// Draw ---

int drawLevel()
{
   SFML_GlobalRenderWindow::get()->setView(*level_view);
   SFML_GlobalRenderWindow::get()->clear( Color( 64, 64, 64 ) );
   // Level
   // drawBackground();
   // drawTerrain();

   // Gui

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


      // Pause
      else if (kb == KB_PAUSE)
         togglePause();

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
