#include "graph.h"
#include "helpers.h"
#include <staple.h>
#include <stdlib.h>

/* Forward declarations */
int _print_node(const void *ptr);


struct tsp_graph *tsp_graph_read(const char *fpath)
{
	FILE *f;
	struct tsp_graph *graph;

	assert(sp_is_abort());
	graph = malloc_or_die(sizeof(struct tsp_graph));
	graph->nodes_active = sp_stack_create(sizeof(struct tsp_node), 200);
	graph->nodes_vacant = sp_stack_create(sizeof(struct tsp_node), 200);

	f = fopen(fpath, "r");
	if (f == NULL) {
		error(("file not found: %s", fpath));
	}
	for (size_t lineno = 1;; ++lineno) {
		struct tsp_node node;
		int n;

		/* Parse line */
		n = fscanf(f, "%d;%d;%d\n", &node.x, &node.y, &node.cost);
		if (n == EOF) {
			info(("successfully parsed %zu lines from %s", lineno - 1, fpath));
			break;
		} else if (n != 3) {
			error(("failed to parse line %zu in %s: fscanf returned %d", lineno, fpath, n));
		}

		/* Append new node to stack */
		sp_stack_push(graph->nodes_vacant, &node);
	}
	fclose(f);
	return graph;
}

void tsp_graph_destroy(struct tsp_graph *graph)
{
	sp_stack_destroy(graph->nodes_active, NULL);
	sp_stack_destroy(graph->nodes_vacant, NULL);
	free(graph);
}

void tsp_node_print(const struct tsp_node *node)
{
	_print_node(node);
}

void tsp_graph_print(const struct tsp_graph *graph)
{
	printf("tsp_graph: %zu/%zu nodes active\n",
		graph->nodes_active->size,
		graph->nodes_vacant->size);
	printf("vacant nodes:\n");
	sp_stack_print(graph->nodes_vacant, _print_node);
	printf("active nodes:\n");
	sp_stack_print(graph->nodes_active, _print_node);
}

int _print_node(const void *ptr)
{
	const struct tsp_node *const node = ptr;
	printf("Node(x=%4d, y=%4d, cost=%4d)\n", node->x, node->y, node->cost);
	return 0;
}
