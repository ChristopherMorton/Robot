#include "log.h"
#include "config.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace std;
using namespace sf;
using namespace robot;

namespace config
{

bool show_keybindings = false;
bool display_damage = true;

///////////////////////////////////////////////////////////////////////////////
// Window

int w_width;
int w_height;
int w_flags;

int width()
{
   return w_width;
}

int height()
{
   return w_height;
}

int flags()
{
   return w_flags;
}

int validateWindow( int w, int h, int f )
{
   if (w == 0 || h == 0)
      return 0;
   else return 1;
}

int setWindow( int w, int h, int f )
{
   if (validateWindow( w, h, f ))
   {
      w_width = w;
      w_height = h;
      w_flags = f;
   }
   else
   {
      w_width = 800;
      w_height = 600;
      w_flags = 0;
   }
   return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Key Binding

KeybindTarget keyToTarget[(int)Keyboard::KeyCount];
Keyboard::Key targetToKey[(int)KB_COUNT];

void clearKeybinds()
{
   int i;
   for ( i = 0; i < (int)Keyboard::KeyCount; ++i )
      keyToTarget[i] = KB_NOTHING;
   for ( i = 0; i < (int)KB_COUNT; ++i )
      targetToKey[i] = Keyboard::Unknown;
}

void setDefaultKeybindings()
{
   clearKeybinds();

   bindKey( KB_PAUSE, Keyboard::Tab );

   bindKey( KB_DEBUG_TOGGLE_FRAMERATE, Keyboard::F1 );

   bindKey( KB_FORCE_QUIT, Keyboard::Escape );
}

sf::Keyboard::Key getBoundKey( KeybindTarget target )
{
   return targetToKey[(int)target];
}

KeybindTarget getBoundTarget( sf::Keyboard::Key key )
{
   return keyToTarget[(int)key];
}

Keyboard::Key bindKey( KeybindTarget target, sf::Keyboard::Key key )
{
   Keyboard::Key key_old = getBoundKey( target ); // This used to be bound to target
   KeybindTarget kb_old = getBoundTarget( key ); // Key used to be bound to this

   keyToTarget[(int)key] = target;
   targetToKey[(int)target] = key;

   // Clear old association
   if (key_old != Keyboard::Unknown && key_old != key)
      keyToTarget[(int)key_old] = KB_NOTHING;

   // Clear previous binding for this key
   if (kb_old != KB_NOTHING && kb_old != target)
      targetToKey[(int)kb_old] = Keyboard::Unknown;

   return key_old;
}

void setDefaults()
{
   setWindow( 800, 600, 0 );
   setDefaultKeybindings();
}

int load()
{
   setDefaults();

   string type, value;
   ifstream config_in;
   config_in.open("res/config.txt"); 

   if (config_in.is_open()) {
      while (true) {
         // Get next line
         config_in >> type;
         if (config_in.eof())
            break;

         if (config_in.bad()) {
            log("Error in reading config - config_in is 'bad' - INVESTIGATE");
            break;
         }

         if (type == "WINDOW") {
            int w = 0, h = 0, f = 0;
            config_in >> w >> h >> f;
            setWindow( w, h, f );
         }
         if (type == "KB") {
            int key = -1, kb = 0;
            config_in >> kb >> key;
            bindKey( (KeybindTarget)kb, (Keyboard::Key)key );
         }
      }

      config_in.close();
   }

   return 0;
}

int save()
{
   ofstream config_out;
   config_out.open("res/config.txt"); 

   config_out << "WINDOW " << w_width << " " << w_height << " " << w_flags << endl;

   for (int kb = 1; kb < (int)KB_COUNT; ++kb) {
      int key = targetToKey[kb];
      if (key != (int)Keyboard::Unknown)
         config_out << "KB " << kb << " " << key << endl;
   }

   config_out.close();

   return 0;
}

};
