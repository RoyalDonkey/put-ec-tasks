#include "../../src/tsp.h"
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <float.h>

/* Typedefs */
typedef void (*offspring_func_t)(struct tsp_graph *child, const struct tsp_graph *parent1, const struct tsp_graph *parent2);

/* Auxiliary struct for defining intra moves */
#define MOVE_TYPE_NODES 0  /* intra-route node swap */
#define MOVE_TYPE_EDGES 1  /* intra-route edge swap */
#define MOVE_TYPE_INTER 2  /* inter-route node swap */
struct lsearch_move {
	struct tsp_move indices;
	char type;
};

#define N_EXPERIMENTS 20
#define TIMEOUT_MS 16323
#define POPULATION_SIZE 20

/* Global variables */
static const char *nodes_files[] = {
	"data/TSPA.csv",
	"data/TSPB.csv",
	"data/TSPC.csv",
	"data/TSPD.csv",
};
static struct sp_stack *nodes[ARRLEN(nodes_files)];

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

void offspring_func_fill_random(struct tsp_graph *graph, const struct tsp_graph *parent1, const struct tsp_graph *parent2)
{
	assert(parent1->nodes_active->size == parent2->nodes_active->size);
	tsp_graph_deactivate_all(graph);
	tsp_graph_activate_common_from_parents(graph, parent1, parent2);
	if (graph->nodes_active->size < parent1->nodes_active->size) {
		tsp_graph_activate_random(graph, parent1->nodes_active->size - graph->nodes_active->size);
	}
	assert(graph->nodes_active->size == parent1->nodes_active->size);
}

void offspring_func_fill_repair(struct tsp_graph *graph, const struct tsp_graph *parent1, const struct tsp_graph *parent2)
{
	assert(parent1->nodes_active->size == parent2->nodes_active->size);
	tsp_graph_deactivate_all(graph);
	tsp_graph_activate_common_from_parents(graph, parent1, parent2);
	if (graph->nodes_active->size < parent1->nodes_active->size) {
		/* TODO */
	}
	assert(graph->nodes_active->size == parent1->nodes_active->size);
}

void run_evolutionary_algorithm(const char *label, size_t population_size, offspring_func_t offspring_func)
{
	const clock_t timeout_cycles = (TIMEOUT_MS / 1000.0) * CLOCKS_PER_SEC;
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
		struct tsp_graph **const population = malloc_or_die(population_size * sizeof(struct tsp_graph*));

		const size_t target_size = nodes[i]->size / 2;
		best_solution[i] = tsp_graph_create(nodes[i]);
		for (size_t j = 0; j < population_size; j++) {
			population[j] = tsp_graph_create(nodes[i]);
		}

		/* Run evolutionary from greedy solutions */
		for (int j = 0; j < N_EXPERIMENTS; j++) {
			/* Initialize population with local optima */
			for (size_t k = 0; k < population_size; k++) {
				tsp_graph_deactivate_all(population[k]);
				tsp_graph_activate_random(population[k], target_size);
				lsearch_greedy(population[k]);
			}

			const clock_t deadline = clock() + timeout_cycles;
			while (clock() < deadline) {
				/* Advance population by `population_size` new children (steady-state) */
				struct tsp_graph *const child = tsp_graph_create(nodes[i]);
				for (size_t k = 0; k < population_size; k++) {
					/* Choose parents */
					const size_t parent1_idx = randint(0, population_size - 1);
					size_t parent2_idx = parent1_idx;
					while (parent2_idx == parent1_idx) {
						parent2_idx = randint(0, population_size - 1);
					}
					const struct tsp_graph *const parent1 = population[parent1_idx];
					const struct tsp_graph *const parent2 = population[parent2_idx];

					/* Create and evaluate child */
					tsp_graph_deactivate_all(child);
					offspring_func(child, parent1, parent2);
					for (size_t l = 0; l < population_size; l++) {
						if (tsp_nodes_eq(child->nodes_active, population[l]->nodes_active)) {
							/* This solution already exists in the population */
							goto skip_to_next_child;
						}
					}
					const unsigned long child_score = tsp_nodes_evaluate(child->nodes_active, &child->dist_matrix);

					/* Find the worst solution in the population */
					size_t worst_idx = 0;
					unsigned long worst_score = 0;
					for (size_t l = 0; l < population_size; l++) {
						const unsigned long score = tsp_nodes_evaluate(population[l]->nodes_active, &population[l]->dist_matrix);
						if (worst_score < score) {
							worst_score = score;
							worst_idx = l;
						}
					}

					/* Replace the worst solution with the child, if the child is better */
					if (child_score < worst_score) {
						tsp_graph_copy(population[worst_idx], child);
					}

					skip_to_next_child: ;
				}
				tsp_graph_destroy(child);
			}

			/* Find the best solution in the population */
			size_t best_idx = 0;
			unsigned long best_score = ULONG_MAX;
			for (size_t k = 0; k < population_size; k++) {
				const unsigned long score = tsp_nodes_evaluate(population[k]->nodes_active, &population[k]->dist_matrix);
				if (score < best_score) {
					best_score = score;
					best_idx = k;
				}
			}
			const struct tsp_graph *const graph = population[best_idx];

			score_min[i] = MIN(best_score, score_min[i]);
			if (best_score > score_max[i]) {
				score_max[i] = best_score;
				tsp_graph_copy(best_solution[i], graph);
			}
			score_sum[i] += best_score;
		}

		for (size_t j = 0; j < population_size; j++) {
			tsp_graph_destroy(population[j]);
		}
		free(population);
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

	run_evolutionary_algorithm("evo-op1", POPULATION_SIZE, offspring_func_fill_random);
	/* run_evolutionary_algorithm("evo-op2", POPULATION_SIZE, offspring_func_fill_repair); */

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		sp_stack_destroy(nodes[i], NULL);
	}
	return 0;
}
