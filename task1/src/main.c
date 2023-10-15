#include "../../src/tsp.h"
#include <limits.h>
#include <libgen.h>
#include <string.h>

/* Typedefs */
typedef void (*activate_func_t)(struct tsp_graph *graph, size_t n_nodes);


/* Global variables */
static const char *files[] = {
	"data/TSPA.csv",
	"data/TSPB.csv",
	"data/TSPC.csv",
};


void greedy_random(struct tsp_graph *graph, size_t target_size)
{
	tsp_graph_activate_random(graph, target_size - 1);
}

void run_greedy_algorithm(const char *algo_name, activate_func_t greedy_algo)
{
	unsigned long score_min[ARRLEN(files)];
	unsigned long score_max[ARRLEN(files)];
	double        score_sum[ARRLEN(files)];
	struct sp_stack *best_solution[ARRLEN(files)];

	for (size_t i = 0; i < ARRLEN(files); i++) {
		score_min[i] = ULONG_MAX;
		score_max[i] = 0;
		score_sum[i] = 0.0;
		best_solution[i] = sp_stack_create(sizeof(struct tsp_node), 100);
	}

	random_seed(0);

	for (size_t i = 0; i < ARRLEN(files); i++) {
		struct sp_stack *const nodes = tsp_nodes_read(files[i]);
		struct tsp_graph *const graph = tsp_graph_create(nodes);
		const size_t target_size = nodes->size / 2;

		/* 200 solutions starting from each node */
		for (int j = 0; j < 200; j++) {
			tsp_graph_deactivate_all(graph);
			tsp_graph_activate_node(graph, j);
			greedy_algo(graph, target_size - 1);

			const unsigned long score = tsp_nodes_evaluate(graph->nodes_active);
			score_min[i] = MIN(score, score_min[i]);
			if (score > score_max[i]) {
				score_max[i] = score;
				sp_stack_copy(best_solution[i], graph->nodes_active, NULL);
			}
			score_sum[i] += score;
		}

		/* 200 completely random solutions */
		for (int j = 0; j < 200; j++) {
			tsp_graph_deactivate_all(graph);
			greedy_algo(graph, target_size);

			const unsigned long score = tsp_nodes_evaluate(graph->nodes_active);
			score_min[i] = MIN(score, score_min[i]);
			if (score > score_max[i]) {
				score_max[i] = score;
				sp_stack_copy(best_solution[i], graph->nodes_active, NULL);
			}
			score_sum[i] += score;
		}

		tsp_graph_destroy(graph);
		sp_stack_destroy(nodes, NULL);
	}

	printf("solutions found by %s:\n", algo_name);
	printf("%-20s\t%8s\t%8s\t%8s\n", "file", "min", "avg", "max");
	for (size_t i = 0; i < ARRLEN(best_solution); i++) {
		char instance_fpath[32], out_fname[128];
		printf("%-20s\t%8lu\t%8lu\t%8lu\n",
			files[i],
			score_min[i],
			ROUND_SCORE(score_sum[i] / 400),
			score_max[i]
		);
		strncpy(instance_fpath, files[i], ARRLEN(instance_fpath));
		sprintf(out_fname, "best_%s_%s", algo_name, basename(instance_fpath));
		tsp_nodes_export(best_solution[i], out_fname);
		sp_stack_destroy(best_solution[i], NULL);
	}
}

int main(void)
{
	assert(sp_is_abort());
	run_greedy_algorithm("greedy", greedy_random);
	return 0;
}
