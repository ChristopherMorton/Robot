#ifndef __FOCUS_H
#define __FOCUS_H

namespace sum
{

#define MAX_FOCUS 25

extern int focus_toughness,
           focus_strength,
           focus_speed,
           focus_perception,
           focus_memory;

int focusMenu();
int initFocusMenuGui();
void fitGui_FocusMenu();

}

#endif
