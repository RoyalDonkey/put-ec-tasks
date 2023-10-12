#include "../../src/tsp.h"
#include <staple.h>

int main()
{
	struct tsp_graph *graph;

	random_seed(0);

	graph = tsp_graph_read("data/TSPA.csv");
	tsp_graph_activate_random(graph, 100);

	tsp_graph_print(graph);
	tsp_graph_destroy(graph);

	return 0;
}
