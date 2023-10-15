#ifndef TSP_GRAPH_H
#define TSP_GRAPH_H

#include <stdlib.h>
#include <stdbool.h>
#include "../libstaple/src/staple.h"

/* Distance between nodes N1 and N2, with optional pointer to distance matrix M
 * (if M == NULL, euclidean_distance is invoked). */
#define DIST(N1, N2, M) ( \
	((M) != NULL) ? \
	(((struct tsp_dist_matrix*)M)->dist[(N1).id * ((struct tsp_dist_matrix*)M)->size + (N2).id]) : \
	ROUND_SCORE(euclidean_dist((N1).x, (N1).y, (N2).x, (N2).y)))


/* Structs */
struct tsp_node {
	unsigned id;  /* Unique ID used to access the distance matrix */
	int x;
	int y;
	int cost;
};

struct tsp_dist_matrix {
	unsigned *dist;
	size_t size;  /* Number of nodes */
};

struct tsp_graph {
	/* The structure I use for nodes is called a stack,
	 * but it has the properties of a normal array. */
	struct sp_stack *nodes_active;  /* Nodes chosen for the route */
	struct sp_stack *nodes_vacant;  /* Remaining, unchosen nodes */
	struct tsp_dist_matrix dist_matrix;  /* Distance cache */
};


/* Functions */
struct sp_stack *tsp_nodes_read(const char *fpath);
void tsp_dist_matrix_init(struct tsp_dist_matrix *matrix, const struct sp_stack *nodes);
struct tsp_graph *tsp_graph_create(const struct sp_stack *nodes);
struct tsp_graph *tsp_graph_empty();
struct tsp_graph *tsp_graph_import(const char *fpath);
void tsp_graph_copy(struct tsp_graph *dest, const struct tsp_graph *src);
void tsp_graph_destroy(struct tsp_graph *graph);
void tsp_node_print(const struct tsp_node *node);
bool tsp_node_eq(const struct tsp_node *node1, const struct tsp_node *node2);
void tsp_nodes_print(const struct sp_stack *nodes);
void tsp_graph_export(const struct tsp_graph *graph, const char *fpath);
void tsp_graph_to_pdf(const struct tsp_graph *graph, const char *fpath);
void tsp_graph_print(const struct tsp_graph *graph);
unsigned long tsp_nodes_evaluate(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix);

void tsp_graph_deactivate_all(struct tsp_graph *graph);
void tsp_graph_activate_node(struct tsp_graph *graph, size_t idx);
void tsp_graph_activate_random(struct tsp_graph *graph, size_t n_nodes);
size_t tsp_nodes_find_nn(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, const struct tsp_node *node);
size_t tsp_nodes_find_2nn(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, const struct tsp_node *node1, const struct tsp_node *node2);
void tsp_graph_find_nc(const struct tsp_graph *graph, size_t *idx, size_t *pos);


#endif /* TSP_GRAPH_H */
