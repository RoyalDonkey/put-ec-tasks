#include "../../src/tsp.h"
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <float.h>

#define N_CANDIDATES 10

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
static struct sp_stack *nodes[ARRLEN(nodes_files)];

void lsearch_candidates_delta_steepest(struct tsp_graph *graph);

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

void lsearch_candidates_delta_steepest(struct tsp_graph *graph)
{
	struct sp_stack *const active = graph->nodes_active;
	struct sp_stack *const vacant = graph->nodes_vacant;
	const size_t n_nodes = graph->dist_matrix.size;
	struct tsp_cand_matrix *const cand_matrix = tsp_graph_compute_candidates(graph, N_CANDIDATES);
	bool *const inter_swap_adds_candidate = tsp_graph_cache_inter_swap_adds_candidates(graph, cand_matrix);
	bool *const swap_nodes_adds_candidate = tsp_nodes_cache_swap_nodes_adds_candidates(active, cand_matrix);
	bool *const swap_edges_adds_candidate = tsp_nodes_cache_swap_edges_adds_candidates(active, cand_matrix);
	tsp_cand_matrix_destroy(cand_matrix);

	bool did_improve = true;
	while (did_improve) {
		struct lsearch_move best_move = {0};
		long min_delta = 0;
		did_improve = false;

		for (size_t i = 0; i < active->size; i++) {
			for (size_t j = i; j < active->size; j++) {
				if (swap_nodes_adds_candidate[i * n_nodes + j]) {
					const long delta = tsp_nodes_evaluate_swap_nodes(active, &graph->dist_matrix, i, j);
					if (delta < min_delta) {
						min_delta = delta;
						best_move.indices.src = i;
						best_move.indices.dest = j;
						best_move.type = MOVE_TYPE_NODES;
						did_improve = true;
					}
				}

				if (swap_edges_adds_candidate[i * n_nodes + j]) {
					const long delta = tsp_nodes_evaluate_swap_edges(active, &graph->dist_matrix, i, j);
					if (delta < min_delta) {
						min_delta = delta;
						best_move.indices.src = i;
						best_move.indices.dest = j;
						best_move.type = MOVE_TYPE_EDGES;
						did_improve = true;
					}
				}
			}
		}

		for (size_t i = 0; i < active->size; i++) {
			for (size_t j = 0; j < vacant->size; j++) {
				if (!inter_swap_adds_candidate[i * n_nodes + j]) {
					continue;
				}
				const long delta = tsp_graph_evaluate_inter_swap(graph, i, j);
				if (delta < min_delta) {
					min_delta = delta;
					best_move.indices.src = i;
					best_move.indices.dest = j;
					best_move.type = MOVE_TYPE_INTER;
					did_improve = true;
				}
			}
		}

		if (did_improve) {
			const size_t i = best_move.indices.src,
			             j = best_move.indices.dest;
			switch (best_move.type) {
				case MOVE_TYPE_NODES:
					tsp_nodes_swap_nodes(active, i, j);
				break;
				case MOVE_TYPE_EDGES:
					tsp_nodes_swap_edges(active, i, j);
				break;
				case MOVE_TYPE_INTER:
					tsp_graph_inter_swap(graph, i, j);
				break;
			}
		}
	}
	free(inter_swap_adds_candidate);
	free(swap_nodes_adds_candidate);
	free(swap_edges_adds_candidate);
}

void run_lsearch_algorithm(const char *label, lsearch_func_t lsearch_algo)
{
	unsigned long score_min[ARRLEN(nodes_files)];
	unsigned long score_max[ARRLEN(nodes_files)];
	double        score_sum[ARRLEN(nodes_files)];
	double time_min[ARRLEN(nodes_files)];
	double time_max[ARRLEN(nodes_files)];
	double time_sum[ARRLEN(nodes_files)];
	struct tsp_graph *best_solution[ARRLEN(nodes_files)];

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		score_min[i] = ULONG_MAX;
		score_max[i] = 0;
		score_sum[i] = 0.0;
		time_min[i] = DBL_MAX;
		time_max[i] = 0.0;
		time_sum[i] = 0.0;
	}

	random_seed(0);

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		struct tsp_graph *const graph = tsp_graph_create(nodes[i]);
		const size_t target_size = nodes[i]->size / 2;
		best_solution[i] = tsp_graph_create(nodes[i]);

		for (int j = 0; j < 200; j++) {
			tsp_graph_deactivate_all(graph);
			tsp_graph_activate_random(graph, target_size);

			clock_t time_before, time_after;
			time_before = clock();
			lsearch_algo(graph);
			time_after = clock();

			const unsigned long score = tsp_nodes_evaluate(graph->nodes_active, &graph->dist_matrix);
			const double time = (double)(time_after - time_before) / CLOCKS_PER_SEC;
			score_min[i] = MIN(score, score_min[i]);
			time_min[i] = MIN(time, time_min[i]);
			time_max[i] = MAX(time, time_max[i]);
			if (score > score_max[i]) {
				score_max[i] = score;
				tsp_graph_copy(best_solution[i], graph);
			}
			score_sum[i] += score;
			time_sum[i] += time;
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
	printf("running time (milliseconds):\n");
	printf("%-20s\t%8s\t%8s\t%8s\n", "file", "min", "avg", "max");
	for (size_t i = 0; i < ARRLEN(best_solution); i++) {
		printf("%-20s\t%8.3f\t%8.3f\t%8.3f\n",
			nodes_files[i],
			1000.0 * time_min[i],
			1000.0 * time_sum[i] / 200.0,
			1000.0 * time_max[i]
		);
	}
}

int main(void)
{
	assert(sp_is_abort());
	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		nodes[i] = tsp_nodes_read(nodes_files[i]);
	}

	run_lsearch_algorithm("lscd-steepest-random", lsearch_candidates_delta_steepest);

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		sp_stack_destroy(nodes[i], NULL);
	}
	return 0;
}
