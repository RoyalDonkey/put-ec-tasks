#include "../../src/tsp.h"
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <float.h>

/* Typedefs */
typedef size_t (*sim_func_t)(const struct sp_stack *nodes1, const struct sp_stack *nodes2);

/* Auxiliary struct for defining intra moves */
#define MOVE_TYPE_NODES 0  /* intra-route node swap */
#define MOVE_TYPE_EDGES 1  /* intra-route edge swap */
#define MOVE_TYPE_INTER 2  /* inter-route node swap */
struct lsearch_move {
	struct tsp_move indices;
	char type;
};

enum instance {
	INST_TSPA,
	INST_TSPB,
	INST_TSPC,
	INST_TSPD,
};

/* Global variables */
static const char *nodes_files[] = {
	"data/TSPA.csv",
	"data/TSPB.csv",
	"data/TSPC.csv",
	"data/TSPD.csv",
};
static const char *best_graphs[] = {
	"data/best_TSPA.graph",
	"data/best_TSPB.graph",
	"data/best_TSPC.graph",
	"data/best_TSPD.graph",
};
static struct sp_stack *nodes[ARRLEN(nodes_files)];
static struct tsp_graph *starting_graphs[ARRLEN(best_graphs)];

struct sp_stack *init_moves(size_t n_nodes)
{
	const size_t n_moves = n_nodes * (n_nodes - 1) + n_nodes * n_nodes;
	struct sp_stack *const moves = sp_stack_create(sizeof(struct lsearch_move), n_moves);
	for (size_t i = 0; i < n_nodes; i++) {
		for (size_t j = i; j < n_nodes; j++) {
			struct lsearch_move m;
			m.indices.src = i;
			m.indices.dest = j;
			m.type = MOVE_TYPE_NODES;
			sp_stack_push(moves, &m);
			m.type = MOVE_TYPE_EDGES;
			sp_stack_push(moves, &m);
		}
	}
	for (size_t i = 0; i < n_nodes; i++) {
		for (size_t j = 0; j < n_nodes; j++) {
			struct lsearch_move m;
			m.indices.src = i;
			m.indices.dest = j;
			m.type = MOVE_TYPE_INTER;
			sp_stack_push(moves, &m);
		}
	}

	return moves;
}

void lsearch_greedy(struct tsp_graph *graph)
{
	struct sp_stack *const active = graph->nodes_active;
	struct sp_stack *const all_moves = init_moves(active->size);
	struct sp_stack *const moves = sp_stack_create(sizeof(struct lsearch_move), all_moves->size);

	bool did_improve = true;
	while (did_improve) {
		sp_stack_copy(moves, all_moves, NULL);
		did_improve = false;
		while (moves->size != 0) {
			const size_t move_idx = randint(0, moves->size - 1);
			const struct lsearch_move m = *(struct lsearch_move*)sp_stack_get(moves, move_idx);
			sp_stack_qremove(moves, move_idx, NULL);

			long delta;
			switch (m.type) {
				case MOVE_TYPE_NODES:
					delta = tsp_nodes_evaluate_swap_nodes(active, &graph->dist_matrix, m.indices.src, m.indices.dest);
					if (delta < 0) {
						tsp_nodes_swap_nodes(active, m.indices.src, m.indices.dest);
						did_improve = true;
					}
				break;
				case MOVE_TYPE_EDGES:
					delta = tsp_nodes_evaluate_swap_edges(active, &graph->dist_matrix, m.indices.src, m.indices.dest);
					if (delta < 0) {
						tsp_nodes_swap_edges(active, m.indices.src, m.indices.dest);
						did_improve = true;
					}
				break;
				case MOVE_TYPE_INTER:
					delta = tsp_graph_evaluate_inter_swap(graph, m.indices.src, m.indices.dest);
					if (delta < 0) {
						tsp_graph_inter_swap(graph, m.indices.src, m.indices.dest);
						did_improve = true;
					}
				break;
				default:
					assert(0);
				break;
			}
			if (did_improve) {
				break;
			}
		}
	}

	sp_stack_destroy(moves, NULL);
	sp_stack_destroy(all_moves, NULL);
}

void plot_similarities(enum instance instance, sim_func_t similarity, const struct sp_stack *reference_solution)
{
	/* Initialization */
	struct sp_stack *const nodes = tsp_nodes_read(nodes_files[instance]);
	struct tsp_graph **const graphs = malloc_or_die(1000 * sizeof(struct tsp_graph*));
	size_t *const sims_to_ref = malloc_or_die(1000 * sizeof(size_t));
	size_t *const sims_to_others = malloc_or_die(1000 * sizeof(size_t));
	const size_t target_size = nodes->size / 2;

	/* Generate 1000 local optima solutions and compute their similarities to the reference solution */
	for (size_t i = 0; i < 1000; i++) {
		graphs[i] = tsp_graph_create(nodes);
		tsp_graph_activate_random(graphs[i], target_size);
		lsearch_greedy(graphs[i]);
		
		sims_to_ref[i] = similarity(graphs[i]->nodes_active, reference_solution);
	}

	/* Compute each solution's average similarity to all other solutions */
	for (size_t i = 0; i < 1000; i++) {
		double sum = 0.0;
		for (size_t j = 0; j < 1000; j++) {
			if (j == i) {
				continue;
			}
			sum += similarity(graphs[i]->nodes_active, graphs[j]->nodes_active);
		}
		sims_to_others[i] = ROUND(sum / 1000);
	}

	/* Cleanup */
	for (size_t i = 0; i < 1000; i++) {
		tsp_graph_destroy(graphs[i]);
	}
	free(sims_to_ref);
	sp_stack_destroy(nodes, NULL);
}


int main(void)
{
	assert(sp_is_abort());
	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		nodes[i] = tsp_nodes_read(nodes_files[i]);
		starting_graphs[i] = tsp_graph_import(best_graphs[i]);
	}

	plot_similarities(INST_TSPA, tsp_nodes_compute_similarity_nodes, starting_graphs[INST_TSPA]->nodes_active);
	plot_similarities(INST_TSPB, tsp_nodes_compute_similarity_nodes, starting_graphs[INST_TSPB]->nodes_active);
	plot_similarities(INST_TSPC, tsp_nodes_compute_similarity_nodes, starting_graphs[INST_TSPC]->nodes_active);
	plot_similarities(INST_TSPD, tsp_nodes_compute_similarity_nodes, starting_graphs[INST_TSPD]->nodes_active);

	plot_similarities(INST_TSPA, tsp_nodes_compute_similarity_edges, starting_graphs[INST_TSPA]->nodes_active);
	plot_similarities(INST_TSPB, tsp_nodes_compute_similarity_edges, starting_graphs[INST_TSPB]->nodes_active);
	plot_similarities(INST_TSPC, tsp_nodes_compute_similarity_edges, starting_graphs[INST_TSPC]->nodes_active);
	plot_similarities(INST_TSPD, tsp_nodes_compute_similarity_edges, starting_graphs[INST_TSPD]->nodes_active);

	plot_similarities(INST_TSPA, tsp_nodes_compute_similarity_nodes, NULL);
	plot_similarities(INST_TSPB, tsp_nodes_compute_similarity_nodes, NULL);
	plot_similarities(INST_TSPC, tsp_nodes_compute_similarity_nodes, NULL);
	plot_similarities(INST_TSPD, tsp_nodes_compute_similarity_nodes, NULL);

	plot_similarities(INST_TSPA, tsp_nodes_compute_similarity_edges, NULL);
	plot_similarities(INST_TSPB, tsp_nodes_compute_similarity_edges, NULL);
	plot_similarities(INST_TSPC, tsp_nodes_compute_similarity_edges, NULL);
	plot_similarities(INST_TSPD, tsp_nodes_compute_similarity_edges, NULL);

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		sp_stack_destroy(nodes[i], NULL);
		tsp_graph_destroy(starting_graphs[i]);
	}
	return 0;
}
