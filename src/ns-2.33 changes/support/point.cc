#include <math.h>

using namespace std;

// Class to represent points.
class Point {
private:
        double xval, yval;
public:
        // Constructor uses default arguments to allow calling with zero, one,
        // or two values.
  Point(double x = 0.0, double y = 0.0);
 
  // Extractors.
  double x();
  double y();

        // Distance to another point.  Pythagorean thm.
  double dist(Point other);
};

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

