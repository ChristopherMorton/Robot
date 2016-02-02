// Robot includes
#include "shutdown.h"
#include "config.h"
#include "savestate.h" 
#include "menustate.h"
#include "util.h"
#include "log.h"

// SFML includes
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>

// lib includes
#include "SFML_GlobalRenderWindow.hpp"
#include "SFML_WindowEventManager.hpp"
#include "SFML_TextureManager.hpp"
#include "IMGuiManager.hpp"
#include "IMCursorManager.hpp"

// C includes
#include <stdio.h>

// C++ includes
#include <deque>
#include <fstream>
#include <sstream>

using namespace sf;

///////////////////////////////////////////////////////////////////////////////
// Data ---

Font *menu_font;
Font *ingame_font;

MenuState menu_state;

namespace robot
{

// Global app-state variables

RenderWindow *r_window = NULL;

// Managers
IMGuiManager* gui_manager;
IMCursorManager* cursor_manager;
SFML_TextureManager* texture_manager;
SFML_WindowEventManager* event_manager;

// Clock
Clock *game_clock = NULL;

void resetView()
{
   r_window->setView( r_window->getDefaultView() );
}

void resetWindow()
{
   ContextSettings settings( 0, 0, 10, 2, 0 );
   if (r_window != NULL)
      r_window->create(VideoMode(config::width(), config::height(), 32), "Robot", Style::None, settings);
   else 
      r_window = new RenderWindow(VideoMode(config::width(), config::height(), 32), "Robot", Style::None, settings);
}

///////////////////////////////////////////////////////////////////////////////
// Definitions ---

void refitGuis(); // Predeclared

///////////////////////////////////////////////////////////////////////////////
// Basic Menu ---

// Options menu in own file, since it's an overlay that can be opened anywhere

// Init --

int progressiveInitMenus()
{
   static int count = 0;
   log("Progressive init - Menus");

   if (count == 0) { 
      menu_font = new Font();
      if (menu_font->loadFromFile("./res/LiberationSans-Regular.ttf"))
         log("Successfully loaded menu font");
      ingame_font = new Font();
      if (ingame_font->loadFromFile("./res/FreeSansBold.ttf"))
         log("Successfully loaded ingame font");
      count = 1;
      return 0; // done
   }

   return -1; // repeat
}

///////////////////////////////////////////////////////////////////////////////
// Loading ---

#define PROGRESS_CAP 100

enum AssetType {
   T_TEXTURE,
   T_SOUND,
   T_SUBLIST,
   T_END
};

struct Asset
{
   AssetType type;
   string path;

   Asset(AssetType t, string& s) { type=t; path=s; }
};

deque<Asset> asset_list;

int loadAssetList( string assetpath )
{
   string type, path, line;
   ifstream alist_in;
   alist_in.open( assetpath.c_str() );

   while (true) {
      // Get next line
      getline( alist_in, line );
      stringstream s_line(line);

      if (alist_in.eof())
         break;

      if (alist_in.bad()) {
         log("Error in AssetList loading - alist_in is 'bad' - INVESTIGATE");
         break;
      }

      s_line >> type;

      if (type == "//") // comment
         continue;

      if (type == "TEXTURE") {
         s_line >> path;
         asset_list.push_back( Asset( T_TEXTURE, path ) );
      }
      if (type == "SOUND") {
         s_line >> path;
         asset_list.push_back( Asset( T_SOUND, path ) );
      }
      if (type == "SUBLIST") {
         s_line >> path;
         loadAssetList( path );
      }

   }

   return 0;
}

int loadAsset( Asset& asset )
{
   if (asset.type == T_TEXTURE)
      texture_manager->addTexture( asset.path );

   return 0;
}

// preload gets everything required by the loading animation
int preload()
{
   log("Preload");

   menu_state = MENU_LOADING;
   return 0;
}

int progressiveInit()
{
   static int progress = 0;

   switch (progress) {
      case 0:
         if (progressiveInitMenus() == 0)
            progress = 7;
         break;

      default:
         return 0;
   }

   return 1;
}

// progressiveLoad works through the assets a bit at a time
int progressiveLoad()
{
   static int asset_segment = 0;
   int progress = 0;

   switch (asset_segment) {
      case 0: // Load asset list
         loadAssetList("res/AssetList.txt");
         asset_segment = 1;
         return -1;
      case 1: // Load up to PROGRESS_CAP assets
         while (!asset_list.empty())
         {
            loadAsset( asset_list.front() );
            asset_list.pop_front();
            if (progress++ > PROGRESS_CAP)
               return -1;
         }
         asset_segment = 2;
         return -1;
      case 2: // Initialize a variety of things
         if (progressiveInit() == 0)
            asset_segment = 3;
         return -1;
      case 3: // Load save game
         loadSaveGame();
         saveSaveGame();
         asset_segment = 4;
         return -1;
      default:
         menu_state = MENU_POSTLOAD;
   }

   return 0;
}

// postload clears the loading structures and drops us in the main menu
int postload()
{
   log("Postload");
   //delete(asset_list);

   menu_state = MENU_MAIN | MENU_PRI_SPLASH;
   return 0;
}

int loadingAnimation(int dt)
{
   sf::Sprite sp_hgg( *texture_manager->getTexture( "HGGLoadingScreenLogo128.png" ) );
   int x = (config::width() - 128) / 2;
   int y = (config::height() - 128) / 2;
   sp_hgg.setPosition( x, y );

   r_window->clear(Color::Black);
   r_window->draw( sp_hgg );
   r_window->display();
   return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Propogation ---

void refitGuis()
{
   /*
   fitGui_Splash(); 

   fitGui_Options();
   fitGui_AVOptions();
   fitGui_InputOptions();

   fitGui_Map();
   fitGui_FocusMenu();

   fitGui_Level();
   fitGui_LevelEditor();
   */
}

///////////////////////////////////////////////////////////////////////////////
// Main Listeners ---

struct MainWindowListener : public My_SFML_WindowListener
{
   virtual bool windowClosed( );
   virtual bool windowResized( const Event::SizeEvent &resized );
   virtual bool windowLostFocus( );
   virtual bool windowGainedFocus( );
};

struct MainMouseListener : public My_SFML_MouseListener
{
   virtual bool mouseMoved( const Event::MouseMoveEvent &mouse_move );
   virtual bool mouseButtonPressed( const Event::MouseButtonEvent &mouse_button_press );
   virtual bool mouseButtonReleased( const Event::MouseButtonEvent &mouse_button_release );
   virtual bool mouseWheelMoved( const Event::MouseWheelEvent &mouse_wheel_move );
};

struct MainKeyListener : public My_SFML_KeyListener
{
   virtual bool keyPressed( const Event::KeyEvent &key_press );
   virtual bool keyReleased( const Event::KeyEvent &key_release );
};

// Window
bool MainWindowListener::windowClosed( )
{
   shutdown(1,1);
   return true;
}

bool MainWindowListener::windowResized( const Event::SizeEvent &resized )
{

   return true;
}

bool MainWindowListener::windowLostFocus( )
{

   return true;
}

bool MainWindowListener::windowGainedFocus( )
{

   return true;
}

// Mouse
bool MainMouseListener::mouseMoved( const Event::MouseMoveEvent &mouse_move )
{
   return true;
}

bool MainMouseListener::mouseButtonPressed( const Event::MouseButtonEvent &mouse_button_press )
{
   IMCursorManager::getSingleton().setCursor( IMCursorManager::CLICKING );
   return true;
}

bool MainMouseListener::mouseButtonReleased( const Event::MouseButtonEvent &mouse_button_release )
{
   IMCursorManager::getSingleton().setCursor( IMCursorManager::DEFAULT );
   return true;
}

bool MainMouseListener::mouseWheelMoved( const Event::MouseWheelEvent &mouse_wheel_move )
{
   return true;
}

// Key
bool MainKeyListener::keyPressed( const Event::KeyEvent &key_press )
{

   if (key_press.code == Keyboard::Escape)
      shutdown(1,1);

   return true;
}

bool MainKeyListener::keyReleased( const Event::KeyEvent &key_release )
{
   return true;
}


///////////////////////////////////////////////////////////////////////////////
// Main loop ---

int mainLoop( int dt )
{
   event_manager->handleEvents();

   r_window->clear(Color::Yellow);

   gui_manager->begin();
   
   gui_manager->end();

   resetView();

   cursor_manager->drawCursor();

   return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Execution starts here ---

int runApp()
{
   // Read the config files
   config::load();
   config::save();

   // Setup the window
   shutdown(1,0);
   resetWindow();

   // Setup various resource managers
   gui_manager = &IMGuiManager::getSingleton();
   cursor_manager = &IMCursorManager::getSingleton();
   texture_manager = &SFML_TextureManager::getSingleton();
   event_manager = &SFML_WindowEventManager::getSingleton();

   SFML_GlobalRenderWindow::set( r_window );
   gui_manager->setRenderWindow( r_window );

   texture_manager->addSearchDirectory( "./res/" ); 

   // Setup event listeners
   MainWindowListener w_listener;
   MainMouseListener m_listener;
   MainKeyListener k_listener;
   event_manager->addWindowListener( &w_listener, "main" );
   event_manager->addMouseListener( &m_listener, "main" );
   event_manager->addKeyListener( &k_listener, "main" );

   // We need to load a loading screen
   menu_state = MENU_PRELOAD;

   // Timing!
   game_clock = new Clock();
   unsigned int old_time, new_time, dt;
   old_time = 0;
   new_time = 0;

   // Loading
   preload();
   while (progressiveLoad())
      loadingAnimation(game_clock->getElapsedTime().asMilliseconds());
   postload();

   // Now setup some things using our new resources
   cursor_manager->createCursor( IMCursorManager::DEFAULT, texture_manager->getTexture( "FingerCursor.png" ), 0, 0, 40, 60);
   //cursor_manager->createCursor( IMCursorManager::CLICKING, texture_manager->getTexture( "FingerCursorClick.png" ), 0, 0, 40, 60);

   log("Entering main loop");
   while (shutdown() == 0)
   {
      if (menu_state & MENU_MAIN) { 

         // Get dt
         new_time = game_clock->getElapsedTime().asMilliseconds();
         dt = new_time - old_time;
         old_time = new_time;

         mainLoop( dt );

         r_window->display();
      }
   }
   log("End main loop");
   
   r_window->close();

   return 0;
}

};

int main(int argc, char* argv[])
{
   return robot::runApp();
}
