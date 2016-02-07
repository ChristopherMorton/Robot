#include "background.h"

#include "SFML_GlobalRenderWindow.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

namespace robot
{
   int drawBackground() {
      RenderWindow *r_window = SFML_GlobalRenderWindow::get();
      r_window->clear(Color::Black);
   }
}
