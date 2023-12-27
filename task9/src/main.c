#include "../../src/tsp.h"
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <float.h>

/* Typedefs */
typedef void (*search_func_t)(struct tsp_graph *graph);

#define N_EXPERIMENTS 20
#define ITERATED_TIMEOUT_MS 16323

/* Global variables */
static const char *nodes_files[] = {
	"data/TSPA.csv",
	"data/TSPB.csv",
	"data/TSPC.csv",
	"data/TSPD.csv",
};
static struct sp_stack *nodes[ARRLEN(nodes_files)];

void run_search_algorithm(const char *label, search_func_t search_algo)
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

		/* Run search_algo from greedy solutions */
		for (int j = 0; j < N_EXPERIMENTS; j++) {
			tsp_graph_deactivate_all(graph);
			tsp_graph_activate_random(graph, target_size);

			search_algo(graph);

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
	for (size_t i = 0; i < ARRLEN(best_solution); i++) {  /* NOLINT(bugprone-sizeof-expression) */
		char instance_fpath[128], out_fname[128];
		printf("%-20s\t%8lu\t%8lu\t%8lu\n",
			nodes_files[i],
			score_min[i],
			ROUND(score_sum[i] / N_EXPERIMENTS),
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
	}

	/* run_search_algorithm("lsns-nolsearch-from-greedy", large_scale_search, true); */
	/* run_search_algorithm("lsns-nolsearch-from-lsearch", large_scale_search, false); */
	/* run_search_algorithm("lsns-lsearch-from-greedy", large_scale_search_with_lsearch, true); */
	/* run_search_algorithm("lsns-lsearch-from-lsearch", large_scale_search_with_lsearch, false); */

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		sp_stack_destroy(nodes[i], NULL);
	}
	return 0;
}
