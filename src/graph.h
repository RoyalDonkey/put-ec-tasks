#ifndef TSP_GRAPH_H
#define TSP_GRAPH_H

#include <stdlib.h>
#include <staple.h>


/* Structs */
struct tsp_node {
	int x;
	int y;
	int cost;
};


/* Functions */
struct sp_stack *tsp_graph_read(const char *fpath);
void tsp_graph_destroy(struct sp_stack *graph);
void tsp_node_print(const struct tsp_node *node);
void tsp_graph_print(const struct sp_stack *graph);


#endif /* TSP_GRAPH_H */
