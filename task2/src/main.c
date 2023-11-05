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
	"data/TSPD.csv",
};
static struct sp_stack *nodes[ARRLEN(files)];


void greedy_cycle_2regret(struct tsp_graph *graph, size_t target_size)
{
	struct sp_stack *vacant = graph->nodes_vacant;
	struct sp_stack *active = graph->nodes_active;

	if (active->size == 0 && target_size != 0)
		tsp_graph_activate_random(graph, 1);
	if (active->size == 1 && target_size != 1) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_peek(active);
		const size_t idx = tsp_nodes_find_nn(vacant, &graph->dist_matrix, node);
		tsp_graph_activate_node(graph, idx);
	}
	while (active->size < target_size) {
		struct tsp_node node;
		struct sp_stack *const rcl = tsp_graph_find_rcl(graph, 10, 0.04);
		const struct tsp_move move = tsp_graph_find_2regret(graph, rcl);
		sp_stack_destroy(rcl, NULL);
		node = *(struct tsp_node*)sp_stack_get(vacant, move.src);
		sp_stack_remove(vacant, move.src, NULL);
		sp_stack_insert(active, move.dest, &node);
	}
}

void greedy_cycle_wsc(struct tsp_graph *graph, size_t target_size)
{
	struct sp_stack *vacant = graph->nodes_vacant;
	struct sp_stack *active = graph->nodes_active;

	if (active->size == 0 && target_size != 0)
		tsp_graph_activate_random(graph, 1);
	if (active->size == 1 && target_size != 1) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_peek(active);
		const size_t idx = tsp_nodes_find_nn(vacant, &graph->dist_matrix, node);
		tsp_graph_activate_node(graph, idx);
	}
	while (active->size < target_size) {
		struct tsp_node node;
		struct sp_stack *const rcl = tsp_graph_find_rcl(graph, 10, 0.04);
		const struct tsp_move move = tsp_graph_find_wsc(graph, rcl, 0.5);
		sp_stack_destroy(rcl, NULL);
		node = *(struct tsp_node*)sp_stack_get(vacant, move.src);
		sp_stack_remove(vacant, move.src, NULL);
		sp_stack_insert(active, move.dest, &node);
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
		struct tsp_graph *const graph = tsp_graph_create(nodes[i]);
		const size_t target_size = nodes[i]->size / 2;
		best_solution[i] = tsp_graph_create(nodes[i]);

		/* 200 solutions starting from each node */
		for (int j = 0; j < 200; j++) {
			tsp_graph_deactivate_all(graph);
			tsp_graph_activate_node(graph, j);
			greedy_algo(graph, target_size);

			const unsigned long score = tsp_nodes_evaluate(graph->nodes_active, &graph->dist_matrix);
			score_max[i] = MAX(score, score_max[i]);
			if (score < score_min[i]) {
				score_min[i] = score;
				tsp_graph_copy(best_solution[i], graph);
			}
			score_sum[i] += score;
		}

		/* 200 completely random solutions */
		for (int j = 0; j < 200; j++) {
			tsp_graph_deactivate_all(graph);
			greedy_algo(graph, target_size);

			const unsigned long score = tsp_nodes_evaluate(graph->nodes_active, &graph->dist_matrix);
			score_max[i] = MAX(score, score_max[i]);
			if (score < score_min[i]) {
				score_min[i] = score;
				tsp_graph_copy(best_solution[i], graph);
			}
			score_sum[i] += score;
		}

		tsp_graph_destroy(graph);
	}

	printf("solutions found by %s:\n", algo_name);
	printf("%-20s\t%8s\t%8s\t%8s\n", "file", "min", "avg", "max");
	for (size_t i = 0; i < ARRLEN(best_solution); i++) {
		char instance_fpath[128], out_fname[128];
		printf("%-20s\t%8lu\t%8lu\t%8lu\n",
			files[i],
			score_min[i],
			ROUND(score_sum[i] / 400),
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
	for (size_t i = 0; i < ARRLEN(files); i++) {
		nodes[i] = tsp_nodes_read(files[i]);
	}

	run_greedy_algorithm("2-regret", greedy_cycle_2regret);
	run_greedy_algorithm("weighted-sum-criterion", greedy_cycle_wsc);

	for (size_t i = 0; i < ARRLEN(files); i++) {
		sp_stack_destroy(nodes[i], NULL);
	}
	return 0;
}
