#ifndef __UTIL_H
#define __UTIL_H

#include "types.h"
#include <SFML/Window/Keyboard.hpp>
#include <SFML/System/String.hpp>

namespace sf { class Sprite; }

void normalizeTo1x1( sf::Sprite *s );

sf::String keybindTargetToString( robot::KeybindTarget target );
sf::String keyToString( sf::Keyboard::Key key );

#endif
