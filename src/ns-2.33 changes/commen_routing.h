#ifndef _COMMEN_ROUTING_h
#define _COMMEN_ROUTING_h

struct PeriEnt {
    double x;
    double y;
    double z;
    nsaddr_t ip;
};

// opaque type: returned by NeighbTable iterator, holds place in table
typedef unsigned int NeighbTableIter;

#endif # COMMEN_ROUTING_h
