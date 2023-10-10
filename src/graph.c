#include "graph.h"
#include "helpers.h"
#include <staple.h>
#include <stdlib.h>

/* Forward declarations */
int _print_node(const void *ptr);


struct tsp_node *tsp_node_create(int x, int y, int cost)
{
	struct tsp_node *node;

	node = malloc_or_die(sizeof(struct tsp_node));
	node->x = x;
	node->y = y;
	node->cost = cost;

	return node;
}

struct sp_stack *tsp_graph_read(const char *fpath)
{
	FILE *f;
	struct sp_stack *graph;

	assert(sp_is_abort());
	graph = sp_stack_create(sizeof(struct tsp_node), 200);
	f = fopen(fpath, "r");
	if (f == NULL) {
		error(("file not found: %s", fpath));
	}
	for (size_t lineno = 1;; ++lineno) {
		int x, y, cost, n;

		/* Parse line */
		n = fscanf(f, "%d;%d;%d\n", &x, &y, &cost);
		if (n == 0) {
			info(("successfully parsed %zu lines from %s", lineno, fpath));
			break;
		} else if (n != 3) {
			error(("failed to parse line %zu in %s: fscanf returned %d", lineno, fpath, n));
		}

		/* Append new node to stack */
		sp_stack_push(graph, tsp_node_create(x, y, cost));
	}
	fclose(f);
	return graph;
}

void tsp_node_destroy(struct tsp_node *node)
{
	free(node);
}

void tsp_graph_destroy(struct sp_stack *graph)
{
	sp_stack_destroy(graph, tsp_node_dtor);
}

int tsp_node_dtor(void *ptr)
{
	struct tsp_node *const node = ptr;
	tsp_node_destroy(node);
	return 0;
}

void tsp_node_print(const struct tsp_node *node)
{
	_print_node(node);
}

void tsp_graph_print(const struct sp_stack *graph)
{
	sp_stack_print(graph, _print_node);
}

int _print_node(const void *ptr)
{
	const struct tsp_node *const node = ptr;
	printf("Node(x=%d, y=%d, cost=%d)\n", node->x, node->y, node->cost);
	return 0;
}
