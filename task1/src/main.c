#include "../../src/tsp.h"
#include <staple.h>

int main()
{
	struct sp_stack *graph = tsp_graph_read("data/TSPA.csv");
	tsp_graph_print(graph);
	tsp_graph_destroy(graph);
	return 0;
}
