#include <math.h>
#include <stdio.h>

#include "point.h"

// Ellipsis class 

class Ellipsis {
private:
  Point p1, p2;
  double major, minor;
  int hit_edge_counter;
 
  void find_minor();

public:
// Constructer
  Ellipsis(Point p1, Point p2);
  
  void change_major(double new_major);
  bool hit_edge();
  
  double get_major();
  Point get_p1();
  Point get_p2();

  bool point_in_ellipsis(double x, double y);
  inline Ellipsis* copy() const;
};


Ellipsis::Ellipsis(Point point1, Point point2) {
  p1 = point1;
  p2 = point2;

  hit_edge_counter = 0;
  
  major = 2 * p1.dist(p2);
  find_minor();
}

double Ellipsis::get_major() {
  return major;
}

void Ellipsis::change_major(double new_major) {
  major = new_major;
  find_minor();
}

bool Ellipsis::hit_edge() {
  hit_edge_counter = (hit_edge_counter + 1) % 2;
  return (hit_edge_counter == 0); 
}

void Ellipsis::find_minor() {
  double distance = p1.dist(p2) / 2.0;
  
  minor = sqrt(major * major - distance * distance);
  printf("Major: %f, Minor: %f\n", major, minor);
}

bool Ellipsis::point_in_ellipsis(double x, double y) {
  Point p = Point(x, y);
  double distance_to_p1 = p1.dist(p);
  double distance_to_p2 = p2.dist(p);
  
  return (distance_to_p1 + distance_to_p2 < major);

}

inline Ellipsis* Ellipsis::copy() const {
  Ellipsis *e = new Ellipsis(p1, p2);
  e->change_major(major);
  return e;
}

/*
int 
main ()
{
  Point p1 =  Point(1.0, 1.0);
  Point p2 =  Point(2.0, 1.0);
  Ellipsis e =  Ellipsis(p1, p2);
  
  bool in = e.point_in_ellipsis(Point(1.5, 1));
  printf("%d\n", in);  


  return 0;
}
*/
