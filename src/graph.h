#ifndef TSP_GRAPH_H
#define TSP_GRAPH_H

#include <stdlib.h>
#include "../libstaple/src/staple.h"


/* Structs */
struct tsp_node {
	int x;
	int y;
	int cost;
};

struct tsp_graph {
	/* The structure I use for nodes is called a stack,
	 * but it has the properties of a normal array. */
	struct sp_stack *nodes_active;  /* Nodes chosen for the route */
	struct sp_stack *nodes_vacant;  /* Remaining, unchosen nodes */
};


/* Functions */
struct sp_stack *tsp_nodes_read(const char *fpath);
struct tsp_graph *tsp_graph_create(const struct sp_stack *nodes);
void tsp_graph_destroy(struct tsp_graph *graph);
void tsp_node_print(const struct tsp_node *node);
void tsp_graph_print(const struct tsp_graph *graph);

void tsp_graph_deactivate_all(struct tsp_graph *graph);
void tsp_graph_activate_node(struct tsp_graph *graph, size_t idx);
void tsp_graph_activate_random(struct tsp_graph *graph, size_t n_nodes);


#endif /* TSP_GRAPH_H */
