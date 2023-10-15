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
	const size_t size = graph->nodes_active->size;
	if (size < target_size)
		tsp_graph_activate_random(graph, target_size - size);
}

void greedy_nn(struct tsp_graph *graph, size_t target_size)
{
	struct tsp_node prev_node;
	if (graph->nodes_active->size == 0 && target_size != 0)
		tsp_graph_activate_random(graph, 1);
	prev_node = *(struct tsp_node*)sp_stack_peek(graph->nodes_active);
	while (graph->nodes_active->size < target_size) {
		const size_t next_idx = tsp_nodes_find_nn(graph->nodes_vacant, &graph->dist_matrix, &prev_node);
		prev_node = *(struct tsp_node*)sp_stack_get(graph->nodes_vacant, next_idx);
		tsp_graph_activate_node(graph, next_idx);
	}
}

void greedy_cycle(struct tsp_graph *graph, size_t target_size)
{
	struct sp_stack *vacant = graph->nodes_vacant;
	struct sp_stack *active = graph->nodes_active;

	if (active->size == 0 && target_size != 0)
		tsp_graph_activate_random(graph, 1);
	if (active->size == 1 && target_size != 1) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_peek(active);
		const size_t idx = tsp_nodes_find_nn(vacant, &graph->dist_matrix, &node);
		tsp_graph_activate_node(graph, idx);
	}
	while (active->size < target_size) {
		size_t idx, pos;
		struct tsp_node node;
		tsp_graph_find_nc(graph, &idx, &pos);
		node = *(struct tsp_node*)sp_stack_get(vacant, idx);
		sp_stack_remove(vacant, idx, NULL);
		sp_stack_insert(active, pos, &node);
	}
}

void run_greedy_algorithm(const char *algo_name, activate_func_t greedy_algo)
{
	unsigned long score_min[ARRLEN(files)];
	unsigned long score_max[ARRLEN(files)];
	double        score_sum[ARRLEN(files)];
	struct tsp_graph *best_solution[ARRLEN(files)];

	for (size_t i = 0; i < ARRLEN(files); i++) {
		score_min[i] = ULONG_MAX;
		score_max[i] = 0;
		score_sum[i] = 0.0;
	}

	random_seed(0);

	for (size_t i = 0; i < ARRLEN(files); i++) {
		struct sp_stack *const nodes = tsp_nodes_read(files[i]);
		struct tsp_graph *const graph = tsp_graph_create(nodes);
		const size_t target_size = nodes->size / 2;
		best_solution[i] = tsp_graph_create(nodes);

		/* 200 solutions starting from each node */
		for (int j = 0; j < 200; j++) {
			tsp_graph_deactivate_all(graph);
			tsp_graph_activate_node(graph, j);
			greedy_algo(graph, target_size);

			const unsigned long score = tsp_nodes_evaluate(graph->nodes_active, &graph->dist_matrix);
			score_min[i] = MIN(score, score_min[i]);
			if (score > score_max[i]) {
				score_max[i] = score;
				tsp_graph_copy(best_solution[i], graph);
			}
			score_sum[i] += score;
		}

		/* 200 completely random solutions */
		for (int j = 0; j < 200; j++) {
			tsp_graph_deactivate_all(graph);
			greedy_algo(graph, target_size);

			const unsigned long score = tsp_nodes_evaluate(graph->nodes_active, &graph->dist_matrix);
			score_min[i] = MIN(score, score_min[i]);
			if (score > score_max[i]) {
				score_max[i] = score;
				tsp_graph_copy(best_solution[i], graph);
			}
			score_sum[i] += score;
		}

		tsp_graph_destroy(graph);
		sp_stack_destroy(nodes, NULL);
	}

	printf("solutions found by %s:\n", algo_name);
	printf("%-20s\t%8s\t%8s\t%8s\n", "file", "min", "avg", "max");
	for (size_t i = 0; i < ARRLEN(best_solution); i++) {
		char instance_fpath[128], out_fname[128];
		printf("%-20s\t%8lu\t%8lu\t%8lu\n",
			files[i],
			score_min[i],
			ROUND_SCORE(score_sum[i] / 400),
			score_max[i]
		);
		strncpy(instance_fpath, files[i], ARRLEN(instance_fpath) - 1);
		*strrchr(instance_fpath, '.') = '\0';  /* Trim file extension */
		sprintf(out_fname, "results/best_%s_%s.graph", algo_name, basename(instance_fpath));
		tsp_graph_export(best_solution[i], out_fname);
		sprintf(out_fname, "results/best_%s_%s.pdf", algo_name, basename(instance_fpath));
		tsp_graph_to_pdf(best_solution[i], out_fname);
		tsp_graph_destroy(best_solution[i]);
	}
}

int main(void)
{
	assert(sp_is_abort());
	run_greedy_algorithm("random", greedy_random);
	run_greedy_algorithm("nearest-neighbor", greedy_nn);
	run_greedy_algorithm("greedy-cycle", greedy_cycle);
	return 0;
}
