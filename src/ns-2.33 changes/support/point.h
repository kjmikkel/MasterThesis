#ifndef ns_point_h
#define ns_point_h

// Class to represent points.
class Point {
private:
        double xval, yval;
	
	double check_limit(double test_value);
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
