#ifndef ELLIPSIS_H
#define ELLIPSIS_H

#include <math.h>
#include <stdio.h>

#include "point.h"

// Ellipsis class 
class Ellipsis {
private:
  Point p1, p2;
  double major, minor;
 
  void find_minor();

public:
// Constructer
  Ellipsis(Point point1, Point point2);
  
  void change_major(double new_major);
  
  double get_major();
  Point get_p1();
  Point get_p2();

  bool point_in_ellipsis(Point p);
};

#endif
