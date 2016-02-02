#include "focus.h"
#include "savestate.h"
#include "map.h"
#include "menustate.h"
#include "config.h"
#include "util.h"

#include "SFML_GlobalRenderWindow.hpp"
#include "SFML_TextureManager.hpp"
#include "IMButton.hpp"
#include "IMGuiManager.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <sstream>

using namespace sf;

namespace sum
{

int focus_toughness,
    focus_strength,
    focus_speed,
    focus_perception,
    focus_memory;

int focus_total;

int countFocus()
{
   focus_total = 0;
   for (int i = 0; i < NUM_LEVELS; ++i)
   {
      LevelRecord lr = getRecord( i );
      if (lr & LR_EASY) focus_total += 2;
      if (lr & LR_EASY_RT) focus_total += 1;
      if (lr & LR_EASY_MM) focus_total += 1;
      if (lr & LR_HARD) focus_total += 1;
      if (lr & LR_HARD_RT) focus_total += 1;
      if (lr & LR_HARD_MM) focus_total += 1;
   }

   return focus_total;
}

int focusRemaining()
{
   return focus_total - focus_toughness - focus_strength - focus_speed - focus_perception - focus_memory;
}

int setFocus( int &focus, int amount )
{
   if (amount > MAX_FOCUS) amount = MAX_FOCUS;
   if (amount < 0) amount = 0;

   int remaining = focusRemaining();
   remaining += focus;

   if (remaining < amount)
      return focus = remaining;
   else
      return focus = amount;
}

int addFocus( int &focus, int add )
{
   setFocus( focus, focus + add );

   return 0;
}

// Focus Menu ---

bool initFocusMenu = false;

IMButton *b_focus_bar_selection,
         *b_focus_arrow_left,
         *b_focus_arrow_right;

void fitGui_FocusMenu()
{
   b_focus_bar_selection->setSize( 20, 40 );

   b_focus_arrow_left->setSize( 60, 60 );
   b_focus_arrow_right->setSize( 60, 60 );
}

int initFocusMenuGui()
{
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();
   IMGuiManager *gui_manager = &IMGuiManager::getSingleton();

   b_focus_bar_selection = new IMButton();
   b_focus_bar_selection->setAllTextures( t_manager.getTexture( "FocusPipHover.png" ) );
   gui_manager->registerWidget( "Focus bar selection button", b_focus_bar_selection );

   b_focus_arrow_left = new IMButton();
   b_focus_arrow_left->setAllTextures( t_manager.getTexture( "OrderTurnWest.png" ) );
   gui_manager->registerWidget( "Focus arrow left", b_focus_arrow_left );

   b_focus_arrow_right = new IMButton();
   b_focus_arrow_right->setAllTextures( t_manager.getTexture( "OrderTurnEast.png" ) );
   gui_manager->registerWidget( "Focus arrow right", b_focus_arrow_right );

   fitGui_FocusMenu();
   initFocusMenu = true;
   
   return 0;
}

void drawFocusTotal()
{
   Text txt_count;
   txt_count.setFont( *menu_font );
   txt_count.setColor( Color::Yellow );
   txt_count.setCharacterSize( 48 );

   stringstream ss;
   ss << focusRemaining();
   txt_count.setString( String(ss.str()) );
   txt_count.setPosition( config::width() - 200, 200 );
   SFML_GlobalRenderWindow::get()->draw( txt_count );

   txt_count.setString( String("/") );
   txt_count.setCharacterSize( 120 );
   txt_count.setPosition( config::width() - 175, 180 );
   SFML_GlobalRenderWindow::get()->draw( txt_count );

   stringstream ss2;
   ss2 << focus_total;
   txt_count.setString( String(ss2.str()) );
   txt_count.setCharacterSize( 48 );
   txt_count.setPosition( config::width() - 140, 250 );
   SFML_GlobalRenderWindow::get()->draw( txt_count );
}

void drawFocusBar( int &focus, int x_start, int y_start )
{
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();
   RenderWindow* r_window = SFML_GlobalRenderWindow::get();

   Sprite sp_focus_pip_empty( *t_manager.getTexture( "FocusPipEmpty.png" ) );
   //Sprite sp_focus_pip_full( t_manager.getTexture( "FocusPipFull.png" ) );
   Sprite sp_focus_pip_selected( *t_manager.getTexture( "FocusPipSelected.png" ) );
   Sprite sp_focus_pip_glow( *t_manager.getTexture( "FocusPipGlow.png" ) );

   Vector2i mouse_pos = Mouse::getPosition(*r_window);
   bool mouse_in_bar = (mouse_pos.y >= y_start && mouse_pos.y <= y_start + 40
                     && mouse_pos.x > x_start && mouse_pos.x <= x_start + (MAX_FOCUS * 20));
   int focus_remain = focusRemaining();

   for (int i = 0; i < MAX_FOCUS; ++i) {

      if (mouse_pos.x > x_start && mouse_pos.x <= x_start + 20 && mouse_in_bar) {
         b_focus_bar_selection->setPosition( x_start, y_start );
         if (b_focus_bar_selection->doWidget()) {
            // Set new focus value
            setFocus( focus, i+1 );
         }
      } else if (i < focus) {
         sp_focus_pip_selected.setPosition( x_start, y_start );
         r_window->draw( sp_focus_pip_selected );
      } else if (mouse_in_bar && mouse_pos.x >  x_start + 20 && focus_remain > 0) {
         sp_focus_pip_glow.setPosition( x_start, y_start );
         r_window->draw( sp_focus_pip_glow );
         --focus_remain;
      } else {
         sp_focus_pip_empty.setPosition( x_start, y_start );
         r_window->draw( sp_focus_pip_empty );
      }

      x_start += 20;
   }
}

void drawFocusArrows( int &focus, int x, int y )
{
   SFML_TextureManager &t_manager = SFML_TextureManager::getSingleton();
   RenderWindow* r_window = SFML_GlobalRenderWindow::get();

   Sprite sp_left_arrow( *t_manager.getTexture( "OrderTurnWest.png" ) );
   Sprite sp_right_arrow( *t_manager.getTexture( "OrderTurnEast.png" ) );

   normalizeTo1x1( &sp_left_arrow );
   sp_left_arrow.scale( 60, 60 );
   normalizeTo1x1( &sp_right_arrow );
   sp_right_arrow.scale( 60, 60 );

   sp_left_arrow.setPosition( x, y );
   sp_right_arrow.setPosition( x + 80, y );

   Vector2i mouse_pos = Mouse::getPosition(*r_window);
   bool mouse_in_y = (mouse_pos.y >= y && mouse_pos.y <= y + 60);

   if (mouse_pos.x >= x && mouse_pos.x <= x + 60 && mouse_in_y) {
      b_focus_arrow_left->setPosition( x, y );
      if (b_focus_arrow_left->doWidget())
         addFocus( focus, -1 );
   } else
      r_window->draw( sp_left_arrow );

   if (mouse_pos.x >= x + 80 && mouse_pos.x <= x + 140 && mouse_in_y) {
      b_focus_arrow_right->setPosition( x + 80, y );
      if (b_focus_arrow_right->doWidget())
         addFocus( focus, 1 );
   } else
      r_window->draw( sp_right_arrow );

}

void drawFocusBars()
{
   drawFocusBar( focus_toughness, 260, 140 );
   drawFocusArrows( focus_toughness, 80, 130 );

   drawFocusBar( focus_strength, 260, 240 );
   drawFocusArrows( focus_strength, 80, 230 );

   drawFocusBar( focus_speed, 260, 340 );
   drawFocusArrows( focus_speed, 80, 330 );

   drawFocusBar( focus_perception, 260, 440 );
   drawFocusArrows( focus_perception, 80, 430 );

   drawFocusBar( focus_memory, 260, 540 );
   drawFocusArrows( focus_memory, 80, 530 );
}

int focusMenu()
{
   if (!initFocusMenu)
      initFocusMenuGui();

   RenderWindow* r_window = SFML_GlobalRenderWindow::get();
   r_window->setView( r_window->getDefaultView() );
   r_window->clear(Color( 127, 127, 127, 255 ));

   drawMapBar();

   countFocus();
   drawFocusTotal();
   drawFocusBars();

   return 0;
}

}
