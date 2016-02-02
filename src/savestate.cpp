#include "savestate.h"
#include "log.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace std;

namespace robot
{


int loadSaveGame()
{
   /*
   initRecords();

   int num, record;
   ifstream save_in;
   save_in.open("res/.save");

   if (save_in.is_open()) {
      while (true) {
         // Get next line
         save_in >> num >> record;
         if (save_in.eof())
            break;

         if (save_in.bad()) {
            log("Error in reading save file - save_in is 'bad' - INVESTIGATE");
            break;
         }

         setRecord( num, record );
      }

      save_in.close();
   }

   */
   return 0;
}

int saveSaveGame()
{
   /*
   ofstream save_out;
   save_out.open("res/.save"); 

   for (int i = 0; i < NUM_LEVELS; ++i) {
      save_out << i << " " << level_scores[i] << " ";
   }

   save_out.close();
   
   */
   return 0;
}

};
