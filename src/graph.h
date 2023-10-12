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

struct tsp_graph {
	/* The structure I use for nodes is called a stack,
	 * but it has the properties of a normal array. */
	struct sp_stack *nodes_active;  /* Nodes chosen for the route */
	struct sp_stack *nodes_vacant;  /* Remaining, unchosen nodes */
};


/* Functions */
struct tsp_graph *tsp_graph_read(const char *fpath);
void tsp_graph_destroy(struct tsp_graph *graph);
void tsp_node_print(const struct tsp_node *node);
void tsp_graph_print(const struct tsp_graph *graph);

void tsp_graph_activate_random(struct tsp_graph *graph, size_t n_nodes);


#endif /* TSP_GRAPH_H */
