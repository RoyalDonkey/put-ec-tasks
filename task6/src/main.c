#include "../../src/tsp.h"
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <float.h>

/* Typedefs */
typedef void (*lsearch_func_t)(struct tsp_graph *graph);
typedef void (*perturb_func_t)(struct tsp_graph *graph);

/* Auxiliary struct for defining intra moves */
#define MOVE_TYPE_NODES 0  /* intra-route node swap */
#define MOVE_TYPE_EDGES 1  /* intra-route edge swap */
#define MOVE_TYPE_INTER 2  /* inter-route node swap */
struct lsearch_move {
	struct tsp_move indices;
	char type;
};

#define N_MULTISTART 200
#define N_EXPERIMENTS 20
#define ITERATED_TIMEOUT_MS 16323

/* Adjustable parameters */
#define PERTURB_MAGNITUDE 10
#define ANNEAL_INIT_TEMP 600.0
#define ANNEAL_RATIO 1.1

/* Global variables */
static const char *nodes_files[] = {
	"data/TSPA.csv",
	"data/TSPB.csv",
	"data/TSPC.csv",
	"data/TSPD.csv",
};
static struct sp_stack *nodes[ARRLEN(nodes_files)];
static size_t lsearch_counter;

void lsearch_steepest(struct tsp_graph *graph);
void iterated_lsearch_steepest_perturb(struct tsp_graph *graph, perturb_func_t perturb_func, clock_t deadline);
void perturb(struct tsp_graph *graph);
void multistart_lsearch_steepest(struct tsp_graph *graph);
void iterated_lsearch_steepest(struct tsp_graph *graph);

void lsearch_steepest(struct tsp_graph *graph)
{
	++lsearch_counter;
	struct sp_stack *const active = graph->nodes_active;
	struct sp_stack *const vacant = graph->nodes_vacant;

	bool did_improve = true;
	while (did_improve) {
		struct lsearch_move best_move = {0};
		long min_delta = 0;
		did_improve = false;

		for (size_t i = 0; i < active->size; i++) {
			for (size_t j = i; j < active->size; j++) {
				long delta;

				delta = tsp_nodes_evaluate_swap_nodes(active, &graph->dist_matrix, i, j);
				if (delta < min_delta) {
					min_delta = delta;
					best_move.indices.src = i;
					best_move.indices.dest = j;
					best_move.type = MOVE_TYPE_NODES;
					did_improve = true;
				}

				delta = tsp_nodes_evaluate_swap_edges(active, &graph->dist_matrix, i, j);
				if (delta < min_delta) {
					min_delta = delta;
					best_move.indices.src = i;
					best_move.indices.dest = j;
					best_move.type = MOVE_TYPE_EDGES;
					did_improve = true;
				}
			}
		}

		for (size_t i = 0; i < active->size; i++) {
			for (size_t j = 0; j < vacant->size; j++) {
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
				default:
					error(("invalid move type"));
				break;
			}
		}
	}
}

void iterated_lsearch_steepest_perturb(struct tsp_graph *graph, perturb_func_t perturb_func, clock_t deadline)
{
	struct tsp_graph *const graph_copy = tsp_graph_empty();
	tsp_graph_copy(graph_copy, graph);
	const size_t target_size = graph->dist_matrix.size / 2;

	unsigned long best_score = ULONG_MAX;
	for (size_t i = 0; i < N_MULTISTART && clock() < deadline; i++) {
		tsp_graph_deactivate_all(graph_copy);
		tsp_graph_activate_random(graph_copy, target_size);

		lsearch_steepest(graph_copy);

		while (clock() < deadline) {
			perturb_func(graph_copy);
			lsearch_steepest(graph_copy);
			const unsigned long score = tsp_nodes_evaluate(graph_copy->nodes_active, &graph_copy->dist_matrix);
			if (score < best_score) {
				best_score = score;
				tsp_graph_copy(graph, graph_copy);
			}
		}
		/* printf("perturb delta:\t%ld [TERMINATE; temp=%.3f]\n", perturb_delta, anneal_temp); */
	}
	tsp_graph_destroy(graph_copy);
}

struct lsearch_move random_move(const struct tsp_graph *graph)
{
	struct lsearch_move ret;
	ret.type = randint(0, 2);
	ret.indices.dest = randint(0, graph->nodes_active->size - 1);
	switch (ret.type) {
		case MOVE_TYPE_NODES:
		case MOVE_TYPE_EDGES:
			ret.indices.src = randint(0, graph->nodes_active->size - 1);
		break;
		case MOVE_TYPE_INTER:
			ret.indices.src = randint(0, graph->nodes_vacant->size - 1);
		break;
		default:
			error(("invalid move type"));
		break;
	}
	return ret;
}

void perturb(struct tsp_graph *graph)
{
	for (size_t i = 0; i < PERTURB_MAGNITUDE; i++) {
		const struct lsearch_move move = random_move(graph);
		const size_t i = move.indices.src,
			     j = move.indices.dest;
		switch (move.type) {
			case MOVE_TYPE_NODES:
				tsp_nodes_swap_nodes(graph->nodes_active, i, j);
			break;
			case MOVE_TYPE_EDGES:
				tsp_nodes_swap_edges(graph->nodes_active, i, j);
			break;
			case MOVE_TYPE_INTER:
				tsp_graph_inter_swap(graph, i, j);
			break;
			default:
				error(("invalid move type"));
			break;
		}
	}
}

void multistart_lsearch_steepest(struct tsp_graph *graph)
{
	struct tsp_graph *const graph_copy = tsp_graph_empty();
	tsp_graph_copy(graph_copy, graph);
	const size_t target_size = graph->dist_matrix.size / 2;

	unsigned long best_score = ULONG_MAX;
	for (size_t i = 0; i < N_MULTISTART; i++) {
		/* Always start from a random solution, abandoning the initial state of "graph".
		 * This kind of destroys the meaningfulness of the "graph" parameter, but
		 * since steepest search is deterministic, there's no point in keeping "graph". */
		tsp_graph_deactivate_all(graph_copy);
		tsp_graph_activate_random(graph_copy, target_size);

		lsearch_steepest(graph_copy);
		const unsigned long score = tsp_nodes_evaluate(graph_copy->nodes_active, &graph_copy->dist_matrix);
		if (score < best_score) {
			best_score = score;
			tsp_graph_copy(graph, graph_copy);
		}
	}
	tsp_graph_destroy(graph_copy);
}

void iterated_lsearch_steepest(struct tsp_graph *graph)
{
	const clock_t time_before = clock();
	const clock_t timeout_cycles = (ITERATED_TIMEOUT_MS / 1000.0) * CLOCKS_PER_SEC;
	iterated_lsearch_steepest_perturb(graph, perturb, time_before + timeout_cycles);
}

void run_lsearch_algorithm(const char *label, lsearch_func_t lsearch_algo)
{
	unsigned long score_min[ARRLEN(nodes_files)];
	unsigned long score_max[ARRLEN(nodes_files)];
	double        score_sum[ARRLEN(nodes_files)];
	double time_min[ARRLEN(nodes_files)];
	double time_max[ARRLEN(nodes_files)];
	double time_sum[ARRLEN(nodes_files)];
	size_t lsearch_runs_min[ARRLEN(nodes_files)];
	size_t lsearch_runs_max[ARRLEN(nodes_files)];
	double lsearch_runs_sum[ARRLEN(nodes_files)];
	struct tsp_graph *best_solution[ARRLEN(nodes_files)];

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		score_min[i] = ULONG_MAX;
		score_max[i] = 0;
		score_sum[i] = 0.0;
		time_min[i] = DBL_MAX;
		time_max[i] = 0.0;
		time_sum[i] = 0.0;
		lsearch_runs_min[i] = SIZE_MAX;
		lsearch_runs_max[i] = 0;
		lsearch_runs_sum[i] = 0.0;
	}

	random_seed(0);

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		struct tsp_graph *const graph = tsp_graph_create(nodes[i]);
		const size_t target_size = nodes[i]->size / 2;
		best_solution[i] = tsp_graph_create(nodes[i]);

		for (int j = 0; j < N_EXPERIMENTS; j++) {
			tsp_graph_deactivate_all(graph);
			tsp_graph_activate_random(graph, target_size);
			lsearch_counter = 0;

			clock_t time_before, time_after;
			time_before = clock();
			lsearch_algo(graph);
			time_after = clock();

			const unsigned long score = tsp_nodes_evaluate(graph->nodes_active, &graph->dist_matrix);
			const double time = (double)(time_after - time_before) / CLOCKS_PER_SEC;
			score_min[i] = MIN(score, score_min[i]);
			time_min[i] = MIN(time, time_min[i]);
			time_max[i] = MAX(time, time_max[i]);
			lsearch_runs_min[i] = MIN(lsearch_counter, lsearch_runs_min[i]);
			lsearch_runs_max[i] = MAX(lsearch_counter, lsearch_runs_max[i]);
			if (score > score_max[i]) {
				score_max[i] = score;
				tsp_graph_copy(best_solution[i], graph);
			}
			score_sum[i] += score;
			time_sum[i] += time;
			lsearch_runs_sum[i] += lsearch_counter;
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
	printf("running time (milliseconds):\n");
	printf("%-20s\t%8s\t%8s\t%8s\n", "file", "min", "avg", "max");
	for (size_t i = 0; i < ARRLEN(best_solution); i++) {  /* NOLINT(bugprone-sizeof-expression) */
		printf("%-20s\t%8.3f\t%8.3f\t%8.3f\n",
			nodes_files[i],
			1000.0 * time_min[i],
			1000.0 * time_sum[i] / N_EXPERIMENTS,
			1000.0 * time_max[i]
		);
	}

	printf("number of lsearch runs:\n");
	printf("%-20s\t%8s\t%8s\t%8s\n", "file", "min", "avg", "max");
	for (size_t i = 0; i < ARRLEN(best_solution); i++) {  /* NOLINT(bugprone-sizeof-expression) */
		printf("%-20s\t%8zu\t%8.1f\t%8zu\n",
			nodes_files[i],
			lsearch_runs_min[i],
			lsearch_runs_sum[i] / N_EXPERIMENTS,
			lsearch_runs_max[i]
		);
	}
}

int main(void)
{
	assert(sp_is_abort());
	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		nodes[i] = tsp_nodes_read(nodes_files[i]);
	}

	run_lsearch_algorithm("msls-steepest-random", multistart_lsearch_steepest);
	run_lsearch_algorithm("ils-steepest-random", iterated_lsearch_steepest);

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		sp_stack_destroy(nodes[i], NULL);
	}
	return 0;
}
