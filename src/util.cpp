#include "util.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "stdlib.h"

using namespace robot;
using namespace sf;

void normalizeTo1x1( Sprite *s )
{
   if (s) {
      float x = s->getTexture()->getSize().x;
      float y = s->getTexture()->getSize().y;
      float scale_x = 1.0 / x;
      float scale_y = 1.0 / y;
      s->setScale( scale_x, scale_y );
   }
}

// ToString methods --

String keybindTargetToString( robot::KeybindTarget target )
{
   switch (target) {
      case KB_NOTHING:
         return String( "N/A" );
      case KB_FORCE_QUIT:
         return String( "Force Quit" );
      case KB_PAUSE:
         return String( "Pause" );
      case KB_DEBUG_TOGGLE_FRAMERATE:
         return String( "DEBUG: Toggle Framerate" );
      default:
         return String( "Keybind has no string representation" );
   }
}

String keyToString( Keyboard::Key key )
{
   switch (key) {
      case Keyboard::A:
         return String( "A" );
      case Keyboard::B:
         return String( "B" );
      case Keyboard::C:
         return String( "C" );
      case Keyboard::D:
         return String( "D" );
      case Keyboard::E:
         return String( "E" );
      case Keyboard::F:
         return String( "F" );
      case Keyboard::G:
         return String( "G" );
      case Keyboard::H:
         return String( "H" );
      case Keyboard::I:
         return String( "I" );
      case Keyboard::J:
         return String( "J" );
      case Keyboard::K:
         return String( "K" );
      case Keyboard::L:
         return String( "L" );
      case Keyboard::M:
         return String( "M" );
      case Keyboard::N:
         return String( "N" );
      case Keyboard::O:
         return String( "O" );
      case Keyboard::P:
         return String( "P" );
      case Keyboard::Q:
         return String( "Q" );
      case Keyboard::R:
         return String( "R" );
      case Keyboard::S:
         return String( "S" );
      case Keyboard::T:
         return String( "T" );
      case Keyboard::U:
         return String( "U" );
      case Keyboard::V:
         return String( "V" );
      case Keyboard::W:
         return String( "W" );
      case Keyboard::X:
         return String( "X" );
      case Keyboard::Y:
         return String( "Y" );
      case Keyboard::Z:
         return String( "Z" );
      case Keyboard::Num0:
         return String( "0" );
      case Keyboard::Num1:
         return String( "1" );
      case Keyboard::Num2:
         return String( "2" );
      case Keyboard::Num3:
         return String( "3" );
      case Keyboard::Num4:
         return String( "4" );
      case Keyboard::Num5:
         return String( "5" );
      case Keyboard::Num6:
         return String( "6" );
      case Keyboard::Num7:
         return String( "7" );
      case Keyboard::Num8:
         return String( "8" );
      case Keyboard::Num9:
         return String( "9" );
      case Keyboard::Escape:
         return String( "Esc" );
      case Keyboard::LControl:
         return String( "LCtrl" );
      case Keyboard::LShift:
         return String( "LShift" );
      case Keyboard::LAlt:
         return String( "LAlt" );
      case Keyboard::LSystem:
         return String( "LSys" );
      case Keyboard::RControl:
         return String( "RCtrl" );
      case Keyboard::RShift:
         return String( "RShift" );
      case Keyboard::RAlt:
         return String( "RAlt" );
      case Keyboard::RSystem:
         return String( "RSys" );
      case Keyboard::Menu:
         return String( "Menu" );
      case Keyboard::LBracket:
         return String( "[" );
      case Keyboard::RBracket:
         return String( "]" );
      case Keyboard::SemiColon:
         return String( ";" );
      case Keyboard::Comma:
         return String( "," );
      case Keyboard::Period:
         return String( "." );
      case Keyboard::Quote:
         return String( "'" );
      case Keyboard::Slash:
         return String( "/" );
      case Keyboard::BackSlash:
         return String( "\\" );
      case Keyboard::Tilde:
         return String( "~" );
      case Keyboard::Equal:
         return String( "=" );
      case Keyboard::Dash:
         return String( "-" );
      case Keyboard::Space:
         return String( "Space" );
      case Keyboard::Return:
         return String( "Ret" );
      case Keyboard::BackSpace:
         return String( "BSp" );
      case Keyboard::Tab:
         return String( "Tab" );
      case Keyboard::PageUp:
         return String( "PUp" );
      case Keyboard::PageDown:
         return String( "PDown" );
      case Keyboard::End:
         return String( "End" );
      case Keyboard::Home:
         return String( "Home" );
      case Keyboard::Insert:
         return String( "Ins" );
      case Keyboard::Delete:
         return String( "Del" );
      case Keyboard::Add:
         return String( "Num+" );
      case Keyboard::Subtract:
         return String( "Num-" );
      case Keyboard::Multiply:
         return String( "Num*" );
      case Keyboard::Divide:
         return String( "Num/" );
      case Keyboard::Left:
         return String( "Left" );
      case Keyboard::Right:
         return String( "Right" );
      case Keyboard::Up:
         return String( "Up" );
      case Keyboard::Down:
         return String( "Down" );
      case Keyboard::Numpad0:
         return String( "Num0" );
      case Keyboard::Numpad1:
         return String( "Num1" );
      case Keyboard::Numpad2:
         return String( "Num2" );
      case Keyboard::Numpad3:
         return String( "Num3" );
      case Keyboard::Numpad4:
         return String( "Num4" );
      case Keyboard::Numpad5:
         return String( "Num5" );
      case Keyboard::Numpad6:
         return String( "Num6" );
      case Keyboard::Numpad7:
         return String( "Num7" );
      case Keyboard::Numpad8:
         return String( "Num8" );
      case Keyboard::Numpad9:
         return String( "Num9" );
      case Keyboard::F1:
         return String( "F1" );
      case Keyboard::F2:
         return String( "F2" );
      case Keyboard::F3:
         return String( "F3" );
      case Keyboard::F4:
         return String( "F4" );
      case Keyboard::F5:
         return String( "F5" );
      case Keyboard::F6:
         return String( "F6" );
      case Keyboard::F7:
         return String( "F7" );
      case Keyboard::F8:
         return String( "F8" );
      case Keyboard::F9:
         return String( "F9" );
      case Keyboard::F10:
         return String( "F10" );
      case Keyboard::F11:
         return String( "F11" );
      case Keyboard::F12:
         return String( "F12" );
      case Keyboard::F13:
         return String( "F13" );
      case Keyboard::F14:
         return String( "F14" );
      case Keyboard::F15:
         return String( "F15" );
      case Keyboard::Pause:
         return String( "Pause" );
      default:
         return String( "N/A" );
   }
}
