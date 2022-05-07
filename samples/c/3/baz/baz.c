#include <curses.h>

#include "../foo/foo.h"

int baz(void) {
  WINDOW *W;
  int x, y;
  
  W = initscr();
  x = getmaxx(W);
  y = getmaxy(W);
  endwin();
  
  return foo(x, y);
}

