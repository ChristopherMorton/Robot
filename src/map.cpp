#include "map.h"
#include "level.h"
#include "savestate.h"
#include "menustate.h"
#include "config.h"
#include "util.h"
#include "shutdown.h"
#include "clock.h"
#include "log.h"

#include "SFML_GlobalRenderWindow.hpp"
#include "SFML_TextureManager.hpp"
#include "SFML_WindowEventManager.hpp"

#include "IMButton.hpp"
#include "IMTextButton.hpp"
#include "IMEdgeTextButton.hpp"
#include "IMGuiManager.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <sstream>

#define MOUSE_DOWN_SELECT_TIME 200

#define GRID_AT(GRID,X,Y) (GRID[((X) + ((Y) * c_map_dimension))])

using namespace sf;

namespace sum
{

///////////////////////////////////////////////////////////////////////////////
// Data ---

Sprite *s_map = NULL;
View *map_view = NULL;

IMButton *b_map_to_splash = NULL;
IMEdgeTextButton *b_map_to_options = NULL;
IMEdgeTextButton *b_map_to_focus = NULL;
IMEdgeTextButton *b_map_to_presets = NULL;
IMEdgeTextButton *b_map_start_level = NULL;

bool init_map_gui = false;

string s_map_to_options = "Options";
string s_map_to_focus = "Focus";
string s_map_to_presets = "Order Sets";
string s_map_start_level = "Start Level";

int selected_level = -1;
int *a_selection_grid;

const int c_map_dimension = 30;
const float c_view_size = 9;

Vector3i *a_level_locations; // Z value is 1 for unlocked, 0 for locked
Sprite *sp_level_marker = NULL;
Sprite *sp_star = NULL;
Sprite *sp_star_left = NULL;
Sprite *sp_star_right = NULL;
Sprite *sp_star_empty = NULL;
Sprite *sp_star_empty_left = NULL;
Sprite *sp_star_empty_right = NULL;

///////////////////////////////////////////////////////////////////////////////
// View ---

Vector2f coordsWindowToMapView( int window_x, int window_y )
{
   RenderWindow *r_window = SFML_GlobalRenderWindow::get();
   float view_x = ( ((float)window_x / (float)(r_window->getSize().x)) *c_view_size ) 
                  + ((map_view->getCenter().x) - (c_view_size / 2.0));
   float view_y = ( ((float)window_y / (float)(r_window->getSize().y)) *c_view_size ) 
                  + ((map_view->getCenter().y) - (c_view_size / 2.0));

   return Vector2f( view_x, view_y );
}

int setMapView( Vector2f center )
{ 
   if (center.x < (c_view_size / 2)) center.x = (c_view_size / 2);
   if (center.y < (c_view_size / 2)) center.y = (c_view_size / 2);
   if (center.x > (c_map_dimension - (c_view_size / 2))) center.x = (c_map_dimension - (c_view_size / 2));
   if (center.y > (c_map_dimension - (c_view_size / 2))) center.y = (c_map_dimension - (c_view_size / 2));

   map_view->setCenter( center.x, center.y );

   return 0;
}

// Shifts in terms of view coords
int shiftMapView( float x_shift, float y_shift )
{
   Vector2f old_center = map_view->getCenter();
   Vector2f new_center (old_center.x + x_shift, old_center.y + y_shift);

   setMapView( new_center );

   return 0;
}

int selectMapObject( Vector2f coords )
{
   int cx = (int)coords.x,
       cy = (int)coords.y;

   std::stringstream ls;
   ls << "Selecting a map object (level) at x=" << cx << ", y=" << cy;
   log(ls.str());

   if (cx < 0 || cy < 0 || cx >= c_map_dimension || cy >= c_map_dimension) {
      selected_level = -1;
      log("FAILED, coords out of bounds");
      return -1;
   }

   selected_level = GRID_AT(a_selection_grid,cx,cy);

   std::stringstream ls2;
   ls2 << "Selected level #" << selected_level;
   log(ls2.str());

   return 0;
}

///////////////////////////////////////////////////////////////////////////////
// API ---

void initMapLevels()
{
   a_level_locations = new Vector3i[NUM_LEVELS];

   // Set up level locations
   a_level_locations[0].x = 15; a_level_locations[0].y = 26; a_level_locations[0].z = 1;

   a_selection_grid = new int[c_map_dimension * c_map_dimension];

   for (int i = 0; i < c_map_dimension; ++i)
      for (int j = 0; j < c_map_dimension; ++j)
         GRID_AT(a_selection_grid,i,j) = -1;

   for (int i = 0; i < NUM_LEVELS; ++i) {
      if (a_level_locations[i].z == 1) {
         int x = a_level_locations[i].x, y = a_level_locations[i].y;
         GRID_AT(a_selection_grid,x,y) = i;
         GRID_AT(a_selection_grid,x+1,y) = i;
         GRID_AT(a_selection_grid,x+2,y) = i;
      }
   }

   sp_level_marker = new Sprite( *(SFML_TextureManager::getSingleton().getTexture( "MapButtonScratch.png" )));
   normalizeTo1x1( sp_level_marker );

   sp_star = new Sprite( *(SFML_TextureManager::getSingleton().getTexture( "StarFull.png" )));
   normalizeTo1x1( sp_star );
   sp_star->scale( 0.5, 0.5 );
   sp_star_right = new Sprite( *(SFML_TextureManager::getSingleton().getTexture( "StarFullRight.png" )));
   normalizeTo1x1( sp_star_right );
   sp_star_right->scale( 0.5, 0.5 );
   sp_star_left = new Sprite( *(SFML_TextureManager::getSingleton().getTexture( "StarFullLeft.png" )));
   normalizeTo1x1( sp_star_left );
   sp_star_left->scale( 0.5, 0.5 );

   sp_star_empty = new Sprite( *(SFML_TextureManager::getSingleton().getTexture( "StarEmpty.png" )));
   normalizeTo1x1( sp_star_empty );
   sp_star_empty->scale( 0.5, 0.5 );
   sp_star_empty_right = new Sprite( *(SFML_TextureManager::getSingleton().getTexture( "StarEmptyRight.png" )));
   normalizeTo1x1( sp_star_empty_right );
   sp_star_empty_right->scale( 0.5, 0.5 );
   sp_star_empty_left = new Sprite( *(SFML_TextureManager::getSingleton().getTexture( "StarEmptyLeft.png" )));
   normalizeTo1x1( sp_star_empty_left );
   sp_star_empty_left->scale( 0.5, 0.5 );
}

void fitGui_Map()
{
   int width = config::width(),
       height = config::height();

   int bar_height = height / 15;
   int bar_but_width = width / 4.5;

   // Bar
   int bar_fill = 0;

   b_map_to_splash->setPosition( bar_fill, 0 );
   b_map_to_splash->setSize( bar_height, bar_height );
   bar_fill += bar_height + 10; 

   b_map_to_options->setPosition( bar_fill, 0 );
   b_map_to_options->setSize( bar_but_width, bar_height );
   b_map_to_options->setTextSize( 24 );
   b_map_to_options->centerText();
   bar_fill += bar_but_width + 10; 

   b_map_to_focus->setPosition( bar_fill, 0 );
   b_map_to_focus->setSize( bar_but_width, bar_height );
   b_map_to_focus->setTextSize( 24 );
   b_map_to_focus->centerText();
   bar_fill += bar_but_width + 10; 

   b_map_to_presets->setPosition( bar_fill, 0 );
   b_map_to_presets->setSize( bar_but_width, bar_height );
   b_map_to_presets->setTextSize( 24 );
   b_map_to_presets->centerText();

   b_map_start_level->setPosition( (width / 2) - 150, height - 60);
   b_map_start_level->setSize( 300, 60 );
   b_map_start_level->setTextSize( 24 );
   b_map_start_level->centerText();
}

void initMapGui()
{
   SFML_TextureManager *texture_manager = &SFML_TextureManager::getSingleton();
   IMGuiManager *gui_manager = &IMGuiManager::getSingleton();

   b_map_to_splash = new IMButton();
   b_map_to_splash->setAllTextures( texture_manager->getTexture( "GoBackButtonScratch.png" ) );
   gui_manager->registerWidget( "Map to Splash", b_map_to_splash);

   b_map_to_options = new IMEdgeTextButton();
   b_map_to_options->setAllTextures( texture_manager->getTexture( "UICenterBrown.png" ) );
   b_map_to_options->setCornerAllTextures( texture_manager->getTexture( "UICornerBrown3px.png" ) );
   b_map_to_options->setEdgeAllTextures( texture_manager->getTexture( "UIEdgeBrown3px.png" ) );
   b_map_to_options->setEdgeWidth( 3 );
   b_map_to_options->setText( &s_map_to_options );
   b_map_to_options->setFont( menu_font );
   b_map_to_options->setTextColor( sf::Color::Black );
   gui_manager->registerWidget( "Map to Options", b_map_to_options);

   b_map_to_focus = new IMEdgeTextButton();
   b_map_to_focus->setAllTextures( texture_manager->getTexture( "UICenterBrown.png" ) );
   b_map_to_focus->setCornerAllTextures( texture_manager->getTexture( "UICornerBrown3px.png" ) );
   b_map_to_focus->setEdgeAllTextures( texture_manager->getTexture( "UIEdgeBrown3px.png" ) );
   b_map_to_focus->setEdgeWidth( 3 );
   b_map_to_focus->setText( &s_map_to_focus );
   b_map_to_focus->setFont( menu_font );
   b_map_to_focus->setTextColor( sf::Color::Black );
   gui_manager->registerWidget( "Map to Focus", b_map_to_focus);

   b_map_to_presets = new IMEdgeTextButton();
   b_map_to_presets->setAllTextures( texture_manager->getTexture( "UICenterBrown.png" ) );
   b_map_to_presets->setCornerAllTextures( texture_manager->getTexture( "UICornerBrown3px.png" ) );
   b_map_to_presets->setEdgeAllTextures( texture_manager->getTexture( "UIEdgeBrown3px.png" ) );
   b_map_to_presets->setEdgeWidth( 3 );
   b_map_to_presets->setText( &s_map_to_presets );
   b_map_to_presets->setFont( menu_font );
   b_map_to_presets->setTextColor( sf::Color::Black );
   gui_manager->registerWidget( "Map to Presets", b_map_to_presets);

   b_map_start_level = new IMEdgeTextButton();
   b_map_start_level->setAllTextures( texture_manager->getTexture( "UICenterBrown.png" ) );
   b_map_start_level->setCornerAllTextures( texture_manager->getTexture( "UICornerBrown3px.png" ) );
   b_map_start_level->setEdgeAllTextures( texture_manager->getTexture( "UIEdgeBrown3px.png" ) );
   b_map_start_level->setEdgeWidth( 3 );
   b_map_start_level->setText( &s_map_start_level );
   b_map_start_level->setFont( menu_font );
   b_map_start_level->setTextColor( sf::Color::Black );
   gui_manager->registerWidget( "Map Start Level", b_map_start_level);

   fitGui_Map();
   init_map_gui = true;
}

int initMap()
{
   static int progress = 0;
   if (progress == 0) {
      initMapLevels();
      progress = 1;
      return -1;
   } else if (progress == 1) {
      initMapGui();
      progress = 2;
      return -1;
   } else {
      SFML_TextureManager *texture_manager = &SFML_TextureManager::getSingleton();
      s_map = new Sprite( *texture_manager->getTexture( "MapScratch.png" ));
      normalizeTo1x1( s_map );
      s_map->scale( c_map_dimension, c_map_dimension );

      map_view = new View();
      map_view->setSize( c_view_size, c_view_size );
      setMapView( Vector2f( c_map_dimension / 2, c_map_dimension / 2 ) );

      return 0;
   }
}

void drawStars( float x, float y, LevelRecord record )
{
   RenderWindow *r_window = SFML_GlobalRenderWindow::get();

   Sprite *which_star = NULL;

   if (record & LR_EASY) which_star = sp_star;
   else which_star = sp_star_empty;
   which_star->setPosition( x + 0, y + 0.5 );
   r_window->draw( *which_star );
   which_star->setPosition( x + 0.5, y + 0.5 );
   r_window->draw( *which_star );

   if (record & LR_HARD) which_star = sp_star;
   else which_star = sp_star_empty;
   which_star->setPosition( x + 0.25, y );
   r_window->draw( *which_star );
   
   if (record & LR_EASY_RT) which_star = sp_star;
   else which_star = sp_star_empty;
   which_star->setPosition( x + 1.0, y + 0.5 );
   r_window->draw( *which_star );
   
   if (record & LR_EASY_MM) which_star = sp_star;
   else which_star = sp_star_empty;
   which_star->setPosition( x + 1.5, y + 0.5 );
   r_window->draw( *which_star );
   
   if (record & LR_HARD_RT) which_star = sp_star_left;
   else which_star = sp_star_empty_left; 
   which_star->setPosition( x + 1.25, y );
   r_window->draw( *which_star );
   
   if (record & LR_HARD_MM) which_star = sp_star_right;
   else which_star = sp_star_empty_right;
   which_star->setPosition( x + 1.25, y );
   r_window->draw( *which_star ); 
}

void drawLevelLocations()
{
   RenderWindow *r_window = SFML_GlobalRenderWindow::get();
   for( int i = 0; i < NUM_LEVELS; ++i ) {
      if (a_level_locations[i].z == 1) {
         // Draw the marker for the level
         sp_level_marker->setPosition( a_level_locations[i].x, a_level_locations[i].y );
         r_window->draw( *sp_level_marker );

         LevelRecord record = getRecord( i );
         drawStars( a_level_locations[i].x + 1, a_level_locations[i].y, record );
      }
   }
}

void drawMapBar()
{
   int bar_height = config::height() / 15;
   RectangleShape gui_bar( Vector2f( config::width(), bar_height ) );
   Color c( 249, 204, 159 );
   gui_bar.setFillColor(c);
   gui_bar.setOutlineThickness( 0 );

   SFML_GlobalRenderWindow::get()->draw( gui_bar );

   if (b_map_to_splash->doWidget()) {
      menu_state = MENU_MAIN | MENU_PRI_SPLASH;
   }
   if (b_map_to_options->doWidget()) {
      menu_state = MENU_MAIN | MENU_PRI_MAP | MENU_SEC_OPTIONS;
   }
   if (b_map_to_focus->doWidget()) {
      menu_state = MENU_MAIN | MENU_PRI_MAP | MENU_MAP_FOCUS;
   }
   if (b_map_to_presets->doWidget()) {
      menu_state = MENU_MAIN | MENU_PRI_MAP | MENU_MAP_PRESETS;
   }
}

int drawMap( int dt )
{
   int retval = 0;
   RenderWindow *r_window = SFML_GlobalRenderWindow::get();
   r_window->setView(*map_view);
   r_window->draw( *s_map );

   drawLevelLocations();

   r_window->setView( r_window->getDefaultView() );

   drawMapBar();

   if (selected_level >= 0) {
      if (b_map_start_level->doWidget())
         loadLevel( selected_level );
   }

   return retval;
}

///////////////////////////////////////////////////////////////////////////////
// Listener ---

extern bool left_mouse_down;
extern int left_mouse_down_time;

struct MapEventHandler : public My_SFML_MouseListener, public My_SFML_KeyListener
{
   virtual bool keyPressed( const Event::KeyEvent &key_press )
   {
      if (key_press.code == Keyboard::Q)
         shutdown(1,1);
      if (key_press.code == Keyboard::Right)
         shiftMapView( 15, 0 );
      if (key_press.code == Keyboard::Left)
         shiftMapView( -15, 0 );
      if (key_press.code == Keyboard::Down)
         shiftMapView( 0, 15 );
      if (key_press.code == Keyboard::Up)
         shiftMapView( 0, -15 );

      return true;
   }
    
   virtual bool keyReleased( const Event::KeyEvent &key_release )
   {
      return true;
   }

   virtual bool mouseMoved( const Event::MouseMoveEvent &mouse_move )
   {
      static int old_mouse_x, old_mouse_y;

      if (left_mouse_down) {
         Vector2f old_spot = coordsWindowToMapView( old_mouse_x, old_mouse_y );
         Vector2f new_spot = coordsWindowToMapView( mouse_move.x, mouse_move.y );
         shiftMapView( old_spot.x - new_spot.x, old_spot.y - new_spot.y );
      }

      old_mouse_x = mouse_move.x;
      old_mouse_y = mouse_move.y;

      return true;
   }

   virtual bool mouseButtonPressed( const Event::MouseButtonEvent &mbp )
   {
      if (mbp.button == Mouse::Left) {
         left_mouse_down = 1;
         left_mouse_down_time = game_clock->getElapsedTime().asMilliseconds();
      }

      return true;
   }

   virtual bool mouseButtonReleased( const Event::MouseButtonEvent &mbr )
   {
      if (mbr.button == Mouse::Left) {
         left_mouse_down = 0;
         int left_mouse_up_time = game_clock->getElapsedTime().asMilliseconds();

         if (left_mouse_up_time - left_mouse_down_time < MOUSE_DOWN_SELECT_TIME) {

            // FIRST: is the mouse in the gui?
            if (mbr.y <= config::height() / 15)
               return true;

            selectMapObject( coordsWindowToMapView( mbr.x, mbr.y ) );
         }
      }

      return true;
   }

   virtual bool mouseWheelMoved( const Event::MouseWheelEvent &mwm )
   {
      //zoomView( mwm.delta, coordsWindowToMapView( mwm.x, mwm.y ) );
      return true;
   }
};

void setMapListener( bool set )
{
   static MapEventHandler map_listener;
   SFML_WindowEventManager& event_manager = SFML_WindowEventManager::getSingleton();
   if (set) {
      event_manager.addMouseListener( &map_listener, "map mouse" );
      event_manager.addKeyListener( &map_listener, "map key" );
   } else {
      event_manager.removeMouseListener( &map_listener );
      event_manager.removeKeyListener( &map_listener );
   }
}

};
