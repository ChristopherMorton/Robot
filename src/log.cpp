#include <string>
#include <fstream>
#include "log.h"

using namespace std;

ofstream logfile;

void log( string out )
{
   static int init = 0;

   if (!init) {
      logfile.open(".log");
      init = 1;
   }

   logfile << out << endl;
}


