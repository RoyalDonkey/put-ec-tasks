#include "../../src/tsp.h"
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <stdbool.h>

/* Typedefs */
typedef void (*lsearch_func_t)(struct tsp_graph *graph);

/* Auxiliary struct for defining intra moves */
#define MOVE_TYPE_NODES 0  /* intra-route node swap */
#define MOVE_TYPE_EDGES 1  /* intra-route edge swap */
#define MOVE_TYPE_INTER 2  /* inter-route node swap */
struct lsearch_move {
	struct tsp_move indices;
	char type;
};

/* Global variables */
static const char *nodes_files[] = {
	"data/TSPA.csv",
	"data/TSPB.csv",
	"data/TSPC.csv",
	"data/TSPD.csv",
};
static const char *graph_files[] = {
	"data/best_greedy_TSPA.graph",
	"data/best_greedy_TSPB.graph",
	"data/best_greedy_TSPC.graph",
	"data/best_greedy_TSPD.graph",
};
static struct sp_stack *nodes[ARRLEN(nodes_files)];
static struct tsp_graph *starting_graphs[ARRLEN(graph_files)];

void lsearch_greedy(struct tsp_graph *graph);

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

void run_lsearch_algorithm(const char *label, lsearch_func_t lsearch_algo, bool random_start)
{
	unsigned long score_min[ARRLEN(nodes_files)];
	unsigned long score_max[ARRLEN(nodes_files)];
	double        score_sum[ARRLEN(nodes_files)];
	struct tsp_graph *best_solution[ARRLEN(nodes_files)];

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		score_min[i] = ULONG_MAX;
		score_max[i] = 0;
		score_sum[i] = 0.0;
	}

	random_seed(0);

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		struct tsp_graph *const graph = tsp_graph_create(nodes[i]);
		const size_t target_size = nodes[i]->size / 2;
		best_solution[i] = tsp_graph_create(nodes[i]);

		for (int j = 0; j < 200; j++) {
			if (random_start) {
				tsp_graph_deactivate_all(graph);
				tsp_graph_activate_random(graph, target_size);
			} else {
				tsp_graph_copy(graph, starting_graphs[i]);
			}

			lsearch_algo(graph);

			const unsigned long score = tsp_nodes_evaluate(graph->nodes_active, &graph->dist_matrix);
			score_min[i] = MIN(score, score_min[i]);
			if (score > score_max[i]) {
				score_max[i] = score;
				tsp_graph_copy(best_solution[i], graph);
			}
			score_sum[i] += score;
		}

		tsp_graph_destroy(graph);
	}

	printf("solutions found by %s:\n", label);
	printf("%-20s\t%8s\t%8s\t%8s\n", "file", "min", "avg", "max");
	for (size_t i = 0; i < ARRLEN(best_solution); i++) {
		char instance_fpath[128], out_fname[128];
		printf("%-20s\t%8lu\t%8lu\t%8lu\n",
			nodes_files[i],
			score_min[i],
			ROUND(score_sum[i] / 200),
			score_max[i]
		);
		strncpy(instance_fpath, nodes_files[i], ARRLEN(instance_fpath) - 1);
		*strrchr(instance_fpath, '.') = '\0';  /* Trim file extension */
		sprintf(out_fname, "results/best_%s_%s.graph", label, basename(instance_fpath));
		tsp_graph_export(best_solution[i], out_fname);
		sprintf(out_fname, "results/best_%s_%s.pdf", label, basename(instance_fpath));
		tsp_graph_to_pdf(best_solution[i], out_fname);
		tsp_graph_destroy(best_solution[i]);
	}
}

int main(void)
{
	assert(sp_is_abort());
	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		nodes[i] = tsp_nodes_read(nodes_files[i]);
		starting_graphs[i] = tsp_graph_import(graph_files[i]);
	}

	run_lsearch_algorithm("ls-greedy-random", lsearch_greedy, true);
	run_lsearch_algorithm("ls-greedy-preset", lsearch_greedy, false);

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		sp_stack_destroy(nodes[i], NULL);
		tsp_graph_destroy(starting_graphs[i]);
	}
	return 0;
}
