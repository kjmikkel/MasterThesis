#include <math.h>
#include <stdio.h>

#include "point.h"
#include "ellipsis.h"

Ellipsis::Ellipsis(Point point1, Point point2)  
{
    p1 = point1;
    p2 = point2;

    hit_edge_counter = 0;
    fprintf(stderr, "before set major %f\n", p1.dist(p2));
    major = 2 * p1.dist(p2);
    fprintf(stderr, "after set major\n");
    fprintf(stderr, "before set mionr\n");
    find_minor();
    fprintf(stderr, "after set minor\n");
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
}

bool Ellipsis::point_in_ellipsis(double x, double y) {
  Point p = Point(x, y);

  double distance_to_p1 = p1.dist(p);
  double distance_to_p2 = p2.dist(p);

  bool inside = (distance_to_p1 + distance_to_p2 < major);

  return inside;
}
