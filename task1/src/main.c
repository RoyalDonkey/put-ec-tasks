#include "../../src/tsp.h"

/* Constants */
#define SEED 0


/* Typedefs */
typedef void (*solution_func_t)(struct tsp_graph *graph);


/* Global variables */
static const char *files[] = {
	"data/TSPA.csv",
	"data/TSPB.csv",
	"data/TSPC.csv",
};


void greedy_random(struct tsp_graph *graph)
{
}

void run_greedy_solution(solution_func_t func)
{
	random_seed(SEED);
	for (int i = 0; i < ARRLEN(files); i++) {
		struct sp_stack *const nodes = tsp_nodes_read(files[i]);
		struct tsp_graph *const graph = tsp_graph_create(nodes);

		/* 200 solutions starting from each node */
		for (int j = 0; j < 200; j++) {
			tsp_graph_deactivate_all(graph);
			tsp_graph_activate_node(graph, j);
			tsp_graph_activate_random(graph, 99);
		}

		tsp_graph_destroy(graph);
		sp_stack_destroy(nodes, NULL);
	}
}

int main(void)
{
	assert(sp_is_abort());
	run_greedy_solution(greedy_random);
	return 0;
}
