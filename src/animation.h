#ifndef ANIMATION_H__
#define ANIMATION_H__

/* An Animation is made from a texture that contains consecutive images.
 * You pass in a time value and the Animation returns a new Sprite that
 * will display the correct image.
 */

namespace sf { class Texture; class Sprite; };

namespace robot
{
   class Animation
   {
   public:
      sf::Texture *texture;
      sf::Sprite *sprite;
      int image_size_x, image_size_y;
      int num_images;
      int duration; // In milliseconds

      Animation();

      int load( sf::Texture *t, int i_size_x, int i_size_y, int num, int dur );
      sf::Sprite *getSprite( int dt );
   };

};

#endif
