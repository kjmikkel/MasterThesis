#ifndef POINT_H
#define POINT_H

#include <iostream>
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
        double x( void );
        double y( void );

        // Distance to another point.  Pythagorean thm.
        double dist(Point other);
};

#endif
