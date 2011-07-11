#include <math.h>
#include "point.h"
using namespace std;

Point::Point(double x, double y) {
                xval = x;
                yval = y;
        }

double Point::x() { return xval; }
double Point::y() { return yval; }

double Point::dist(Point other) {
  double xd = xval - other.xval;
  double yd = yval - other.yval;
  return sqrt(xd*xd + yd*yd);
}
