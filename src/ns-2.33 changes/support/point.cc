#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdexcept>

#include "point.h"
using namespace std;

Point::Point(double x, double y) {
                xval = x;
                yval = y;
        }

double Point::x() { return xval; }
double Point::y() { return yval; }

bool Point::check_limit(double test_value) {
  if (test_value > sqrt(DBL_MAX) / 2)
    return true;

  // Quick escape
  if (test_value < -1000000000) {
    return true;
  } else {
    return false;
  }

}

double Point::dist(Point other) {
  bool x_exceed = check_limit(other.xval);
  bool y_exceed = check_limit(other.yval);

  if (x_exceed || y_exceed) {
    return sqrt(DBL_MAX) / 2.0;
  }

  double xd = xval - other.xval;
  double yd = yval - other.yval;

  x_exceed = check_limit(xd);
  y_exceed = check_limit(yd);

  if (x_exceed || y_exceed) {
    return sqrt(DBL_MAX) / 2.0;
  }

  return sqrt(xd*xd + yd*yd);
}
