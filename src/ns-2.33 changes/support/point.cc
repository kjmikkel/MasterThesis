#include <math.h>
#include <float.h>
#include <stdio.h>

#include "point.h"
using namespace std;

Point::Point(double x, double y) {
                xval = x;
                yval = y;
        }

double Point::x() { return xval; }
double Point::y() { return yval; }

double Point::check_limit(double test_value) {
  if (test_value > DBL_MAX)
    return sqrt(DBL_MAX);

  if (test_value < DBL_MIN) {
    return 0;
  }
}

double Point::dist(Point other) {
  
  double local_new_x = check_limit(other.xval);
  double local_new_y = check_limit(other.yval);

  double xd = xval - local_new_x;
  double yd = yval - local_new_y;
  
  fprintf(stderr, "local: %f, %f\n", xval, yval);
  fprintf(stderr, "new: %f, %f\n", local_new_x, local_new_y);
  fprintf(stderr, "dist 1: %f, %f\n", xd, yd);
  return sqrt(xd*xd + yd*yd);
}
