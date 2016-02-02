#include "shutdown.h"

int shutdown(int set, int value)
{
   static int val;
   if (set) val = value;
   return val;
}
