#ifndef TSP_GRAPH_H
#define TSP_GRAPH_H

#include <stdlib.h>
#include <stdbool.h>
#include "../libstaple/src/staple.h"

#define TSP_MAX_NODE_ID 200

/* Distance between nodes N1 and N2, with optional pointer to distance matrix M
 * (if M == NULL, euclidean_distance is invoked). */
#define DIST(N1, N2, M) ( \
	((M) != NULL) ? \
	(((struct tsp_dist_matrix*)M)->dist[(N1).id * ((struct tsp_dist_matrix*)M)->size + (N2).id]) : \
	ROUND(euclidean_dist((N1).x, (N1).y, (N2).x, (N2).y)))


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

struct tsp_cand_matrix {
	bool *cand;
	size_t size;  /* Number of nodes */
};

struct tsp_graph {
	/* The structure I use for nodes is called a stack,
	 * but it has the properties of a normal array. */
	struct sp_stack *nodes_active;  /* Nodes chosen for the route */
	struct sp_stack *nodes_vacant;  /* Remaining, unchosen nodes */
	struct tsp_dist_matrix dist_matrix;  /* Distance cache */
};

/* Represents a move operation from src index to dest index */
struct tsp_move {
	size_t src;
	size_t dest;
};


/* Functions */
struct sp_stack *tsp_nodes_read(const char *fpath);
void tsp_dist_matrix_init(struct tsp_dist_matrix *matrix, const struct sp_stack *nodes);
void tsp_dist_matrix_print(struct tsp_dist_matrix matrix);
struct tsp_graph *tsp_graph_create(const struct sp_stack *nodes);
struct tsp_graph *tsp_graph_empty();
struct tsp_graph *tsp_graph_import(const char *fpath);
void tsp_graph_copy(struct tsp_graph *dest, const struct tsp_graph *src);
void tsp_graph_destroy(struct tsp_graph *graph);
void tsp_node_print(struct tsp_node node);
bool tsp_node_eq(struct tsp_node node1, struct tsp_node node2);
void tsp_nodes_print(const struct sp_stack *nodes);
void tsp_graph_export(const struct tsp_graph *graph, const char *fpath);
void tsp_graph_to_pdf(const struct tsp_graph *graph, const char *fpath);
void tsp_graph_print(const struct tsp_graph *graph);
unsigned long tsp_nodes_evaluate(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix);
long tsp_graph_evaluate_move(const struct tsp_graph *graph, struct tsp_move move);

void tsp_graph_deactivate_all(struct tsp_graph *graph);
void tsp_graph_activate_node(struct tsp_graph *graph, size_t idx);
void tsp_graph_activate_random(struct tsp_graph *graph, size_t n_nodes);
size_t tsp_nodes_find_nn(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, struct tsp_node node);
size_t tsp_nodes_find_2nn(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, struct tsp_node node1, struct tsp_node node2);
struct tsp_move tsp_graph_find_nc(const struct tsp_graph *graph);
struct sp_stack *tsp_graph_find_rcl(const struct tsp_graph *graph, size_t size, double p);
unsigned long tsp_graph_compute_2regret(const struct tsp_graph *graph, size_t vacant_idx);
struct tsp_move tsp_graph_find_2regret(const struct tsp_graph *graph, const struct sp_stack *rcl);
struct tsp_move tsp_graph_find_wsc(const struct tsp_graph *graph, const struct sp_stack *rcl, double ratio);


void tsp_graph_inter_swap(struct tsp_graph *graph, size_t vacant_idx, size_t active_idx);
void tsp_nodes_swap_nodes(struct sp_stack *nodes, size_t idx1, size_t idx2);
void tsp_nodes_swap_edges(struct sp_stack *nodes, size_t idx1, size_t idx2);

long tsp_graph_evaluate_inter_swap(const struct tsp_graph *graph, size_t vacant_idx, size_t active_idx);
long tsp_nodes_evaluate_swap_nodes(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, size_t idx1, size_t idx2);
long tsp_nodes_evaluate_swap_edges(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, size_t idx1, size_t idx2);

struct tsp_cand_matrix *tsp_cand_matrix_create(size_t size);
struct tsp_cand_matrix *tsp_graph_compute_candidates(const struct tsp_graph *graph, size_t n);
void tsp_cand_matrix_destroy(struct tsp_cand_matrix *cand_matrix);
bool tsp_graph_inter_swap_adds_candidate(const struct tsp_graph *graph, const struct tsp_cand_matrix *cand_matrix, size_t vacant_idx, size_t active_idx);
bool tsp_nodes_swap_nodes_adds_candidate(const struct sp_stack *nodes, const struct tsp_cand_matrix *cand_matrix, size_t idx1, size_t idx2);
bool tsp_nodes_swap_edges_adds_candidate(const struct sp_stack *nodes, const struct tsp_cand_matrix *cand_matrix, size_t idx1, size_t idx2);
bool *tsp_graph_cache_inter_swap_adds_candidates(const struct tsp_graph *graph, const struct tsp_cand_matrix *cand_matrix);
bool *tsp_nodes_cache_swap_nodes_adds_candidates(const struct sp_stack *nodes, const struct tsp_cand_matrix *cand_matrix);
bool *tsp_nodes_cache_swap_edges_adds_candidates(const struct sp_stack *nodes, const struct tsp_cand_matrix *cand_matrix);

#endif /* TSP_GRAPH_H */
