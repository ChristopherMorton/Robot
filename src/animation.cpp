#include "animation.h"
#include "log.h"
#include <sstream>
#include <SFML/Graphics.hpp> 

using namespace robot;
using namespace sf;

Animation::Animation()
   : texture(NULL),
     image_size_x(0),
     image_size_y(0),
     num_images(0),
     duration(0)
{ }

int Animation::load( sf::Texture *t, int i_size_x, int i_size_y, int num, int dur )
{
   if (NULL == t) return -1;

   texture = t;
   image_size_x = i_size_x;
   image_size_y = i_size_y;
   num_images = num;
   duration = dur;

   sprite = new Sprite( *t );

   return 0;
}

int getFrame( int dt, int duration, int num )
{
   dt = dt % duration; // Skip cycles
   
   int ret = (num * dt) / duration;

   return ret;
}

sf::Sprite *Animation::getSprite( int dt )
{
   if (NULL == texture) return NULL;

   int frame = getFrame( dt, duration, num_images );

   const IntRect ir( (frame * image_size_x), 0, image_size_x, image_size_y );
   sprite->setTextureRect( ir );

   return sprite;
}
