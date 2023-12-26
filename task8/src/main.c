#include "../../src/tsp.h"
#include "../../gnuplot_i/gnuplot_i.h"
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <float.h>

/* Typedefs */
typedef size_t (*sim_func_t)(const struct sp_stack *nodes1, const struct sp_stack *nodes2);

#define NO_ITERS 1000

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

/* Generate a single plot.
 * Arguments:
 * - `data_fpath`: output filename (csv)
 * - `instance`: the type of instance (`INST_TSPA`, `INST_TSPB`, ...)
 * - `similarity`: the similarity function to use
 * - `reference_solution`: a solution to compare to, or `NULL` to compare with the average of NO_ITERS instances.
 */
void compute_similarities(const char *data_fpath, enum instance instance, sim_func_t similarity, const struct sp_stack *reference_solution)
{
	/* Initialization */
	struct tsp_graph **const graphs = malloc_or_die(NO_ITERS * sizeof(struct tsp_graph*));
	unsigned long *const graph_scores = malloc_or_die(NO_ITERS * sizeof(unsigned long));
	size_t *const sims = malloc_or_die(NO_ITERS * sizeof(size_t));
	const size_t target_size = nodes[instance]->size / 2;

	/* Generate NO_ITERS local optima solutions and compute their similarities to the reference solution */
	for (size_t i = 0; i < NO_ITERS; i++) {
		fprintf(stderr, "\r   Generating solutions...  %3zu.%zu%%", i * 100 / NO_ITERS, (i * 1000 / NO_ITERS) % 10);
		graphs[i] = tsp_graph_create(nodes[instance]);
		tsp_graph_activate_random(graphs[i], target_size);
		lsearch_greedy(graphs[i]);
		graph_scores[i] = tsp_nodes_evaluate(graphs[i]->nodes_active, &graphs[i]->dist_matrix);

		if (reference_solution != NULL) {
			sims[i] = similarity(graphs[i]->nodes_active, reference_solution);
		}
	}
	fprintf(stderr, "\r   Generating solutions...  100%%  \n");

	if (reference_solution == NULL) {
		/* Compute each solution's average similarity to all other solutions */
		for (size_t i = 0; i < NO_ITERS; i++) {
			fprintf(stderr, "\r   Computing average similarities...  %3zu.%zu%%", i / 10, i % 10);
			double sum = 0.0;
			for (size_t j = 0; j < NO_ITERS; j++) {
				if (j == i) {
					continue;
				}
				sum += similarity(graphs[i]->nodes_active, graphs[j]->nodes_active);
			}
			sims[i] = ROUND(sum / NO_ITERS);
		}
		fprintf(stderr, "\r   Computing average similarities...  100%%  \n");
	}

	/* Write out to file */
	FILE *fout = fopen(data_fpath, "w");
	if (fout == NULL) {
		perror("Failed to open file for writing");
		return;
	}
	for (size_t i = 0; i < NO_ITERS; i++) {
		fprintf(fout, "%lu,%zu\n", graph_scores[i], sims[i]);
	}
	fclose(fout);

	/* Cleanup */
	for (size_t i = 0; i < NO_ITERS; i++) {
		tsp_graph_destroy(graphs[i]);
	}
	free(graphs);
	free(graph_scores);
	free(sims);
}

void plot_similarities(const char *data_fpath, const char *plot_fpath)
{
	fprintf(stderr, "   Plotting results... ");
	gnuplot_ctrl *handle = gnuplot_init();
	gnuplot_cmd(handle, "set datafile separator \",\"");
	gnuplot_cmd(handle, "set terminal pdf");
	gnuplot_cmd(handle, "unset key");
	gnuplot_cmd(handle, "set ytics 1");
	gnuplot_cmd(handle, "set grid");
	gnuplot_cmd(handle, "set style fill solid noborder");
	gnuplot_cmd(handle, "set jitter vertical spread 0.3");
	gnuplot_cmd(handle, "set output \"%s\"", plot_fpath);
	gnuplot_cmd(handle, "plot \"%s\" u 1:2 lc rgb 0xA02222FF pt 7 ps 0.5", data_fpath, data_fpath);
	gnuplot_close(handle);
	fprintf(stderr, "done.\n");
}

void run(enum instance instance, sim_func_t similarity, const struct sp_stack *reference_solution)
{
	char data_fpath[64];
	char plot_fpath[64];

	fprintf(stderr, "-> ");
	switch (instance) {
		case INST_TSPA: fprintf(stderr, "TSPA, "); break;
		case INST_TSPB: fprintf(stderr, "TSPB, "); break;
		case INST_TSPC: fprintf(stderr, "TSPC, "); break;
		case INST_TSPD: fprintf(stderr, "TSPD, "); break;
	}
	if (similarity == tsp_nodes_compute_similarity_nodes) {
		fprintf(stderr, "sim=nodes, ");
	} else if (similarity == tsp_nodes_compute_similarity_edges) {
		fprintf(stderr, "sim=edges, ");
	}
	switch ((int)(reference_solution != NULL)) {
		case 1: fprintf(stderr, "ref=best\n"); break;
		case 0: fprintf(stderr, "ref=avg\n"); break;
	}

	sprintf(data_fpath, "results/TSP%c_%s_%s.csv",
		'A' + instance,
		similarity == tsp_nodes_compute_similarity_nodes ? "nodes" : "edges",
		reference_solution != NULL ? "best" : "avg"
	);
	sprintf(plot_fpath, "results/TSP%c_%s_%s.pdf",
		'A' + instance,
		similarity == tsp_nodes_compute_similarity_nodes ? "nodes" : "edges",
		reference_solution != NULL ? "best" : "avg"
	);

	/* Ensure data file exists */
	FILE *const data_file = fopen(data_fpath, "r");
	if (data_file == NULL) {
		compute_similarities(data_fpath, instance, similarity, reference_solution);
	} else {
		fprintf(stderr, "   Found data at %s\n", data_fpath);
		fclose(data_file);
	}

	plot_similarities(data_fpath, plot_fpath);
}


int main(void)
{
	assert(sp_is_abort());
	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		nodes[i] = tsp_nodes_read(nodes_files[i]);
		starting_graphs[i] = tsp_graph_import(best_graphs[i]);
	}

	run(INST_TSPA, tsp_nodes_compute_similarity_nodes, starting_graphs[INST_TSPA]->nodes_active);
	run(INST_TSPB, tsp_nodes_compute_similarity_nodes, starting_graphs[INST_TSPB]->nodes_active);
	run(INST_TSPC, tsp_nodes_compute_similarity_nodes, starting_graphs[INST_TSPC]->nodes_active);
	run(INST_TSPD, tsp_nodes_compute_similarity_nodes, starting_graphs[INST_TSPD]->nodes_active);

	run(INST_TSPA, tsp_nodes_compute_similarity_edges, starting_graphs[INST_TSPA]->nodes_active);
	run(INST_TSPB, tsp_nodes_compute_similarity_edges, starting_graphs[INST_TSPB]->nodes_active);
	run(INST_TSPC, tsp_nodes_compute_similarity_edges, starting_graphs[INST_TSPC]->nodes_active);
	run(INST_TSPD, tsp_nodes_compute_similarity_edges, starting_graphs[INST_TSPD]->nodes_active);

	run(INST_TSPA, tsp_nodes_compute_similarity_nodes, NULL);
	run(INST_TSPB, tsp_nodes_compute_similarity_nodes, NULL);
	run(INST_TSPC, tsp_nodes_compute_similarity_nodes, NULL);
	run(INST_TSPD, tsp_nodes_compute_similarity_nodes, NULL);

	run(INST_TSPA, tsp_nodes_compute_similarity_edges, NULL);
	run(INST_TSPB, tsp_nodes_compute_similarity_edges, NULL);
	run(INST_TSPC, tsp_nodes_compute_similarity_edges, NULL);
	run(INST_TSPD, tsp_nodes_compute_similarity_edges, NULL);

	for (size_t i = 0; i < ARRLEN(nodes_files); i++) {
		sp_stack_destroy(nodes[i], NULL);
		tsp_graph_destroy(starting_graphs[i]);
	}
	return 0;
}
