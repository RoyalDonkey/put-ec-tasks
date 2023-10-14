#include "graph.h"
#include "helpers.h"
#include "../libstaple/src/staple.h"
#include <stdlib.h>
#include <unistd.h>

/* Forward declarations */
int _print_node(const void *ptr);


struct sp_stack *tsp_nodes_read(const char *fpath)
{
	FILE *f;
	struct sp_stack *nodes;

	nodes = sp_stack_create(sizeof(struct tsp_node), 200);

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
		sp_stack_push(nodes, &node);
	}
	fclose(f);
	return nodes;
}

struct tsp_graph *tsp_graph_create(const struct sp_stack *nodes)
{
	struct tsp_graph *graph;
	graph = malloc_or_die(sizeof(struct tsp_graph));
	graph->nodes_active = sp_stack_create(sizeof(struct tsp_node), 200);
	graph->nodes_vacant = sp_stack_create(sizeof(struct tsp_node), 200);
	sp_stack_copy(graph->nodes_vacant, nodes, NULL);
	return graph;
}

struct tsp_graph *tsp_graph_import(const char *fpath)
{
	FILE *f;
	struct tsp_graph *const graph = malloc_or_die(sizeof(struct tsp_graph));
	int n_vacant, n_active, n_total;

	f = fopen(fpath, "r");
	if (f == NULL) {
		error(("file not found: %s", fpath));
	}
	assert(2 == fscanf(f, "%d;%d\n", &n_vacant, &n_active));
	n_total = n_vacant + n_active;

	graph->nodes_vacant = sp_stack_create(sizeof(struct tsp_node), n_total);
	graph->nodes_active = sp_stack_create(sizeof(struct tsp_node), n_total);

	while (n_vacant-- > 0) {
		struct tsp_node node;
		int n;

		/* Parse line */
		n = fscanf(f, "%d;%d;%d\n", &node.x, &node.y, &node.cost);
		if (n == EOF)
			error(("unexpected EOF while importing from %s", fpath));
		else if (n != 3)
			error(("failed to parse %s: fscanf returned %d", fpath, n));

		/* Append new node to stack */
		sp_stack_push(graph->nodes_vacant, &node);
	}

	while (n_active-- > 0) {
		struct tsp_node node;
		int n;

		/* Parse line */
		n = fscanf(f, "%d;%d;%d\n", &node.x, &node.y, &node.cost);
		if (n == EOF)
			error(("unexpected EOF while importing from %s", fpath));
		else if (n != 3)
			error(("failed to parse %s: fscanf returned %d", fpath, n));

		/* Append new node to stack */
		sp_stack_push(graph->nodes_active, &node);
	}

	info(("successfully parsed %zu lines from %s", n_total, fpath));
	fclose(f);
	return graph;
}

void tsp_graph_copy(struct tsp_graph *dest, const struct tsp_graph *src)
{
	sp_stack_copy(dest->nodes_active, src->nodes_active, NULL);
	sp_stack_copy(dest->nodes_vacant, src->nodes_vacant, NULL);
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

void tsp_nodes_print(const struct sp_stack *nodes)
{
	printf("%zu nodes:\n", nodes->size);
	sp_stack_print(nodes, _print_node);
	printf("score: %lu\n", tsp_nodes_evaluate(nodes));
}

void tsp_graph_export(const struct tsp_graph *graph, const char *fpath)
{
	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;
	FILE *const f = fopen(fpath, "w");
	if (f == NULL) {
		warn(("tsp_nodes_export: failed to open file %s for writing\n"));
		return;
	}

	fprintf(f, "%zu;%zu\n", vacant->size, active->size);
	for (size_t i = vacant->size; i-- > 0;) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(vacant, i);
		fprintf(f, "%d;%d;%d\n", node.x, node.y, node.cost);
	}
	for (size_t i = active->size; i-- > 0;) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(active, i);
		fprintf(f, "%d;%d;%d\n", node.x, node.y, node.cost);
	}
	fclose(f);
}

/* Requires graphviz's "neato" utility. */
void tsp_graph_to_pdf(const struct tsp_graph *graph, const char *fpath)
{
	const char graphviz_head[] =
		"graph {\n"
		"\tsize = \"7,5\";\n";
	const char graphviz_vacant_node_fmt[] =
		"\tnode%d [\n"
		"\t\tlabel = \"%d\"\n"
		"\t\tpos = \"%d,%d!\"\n"
		"\t\tfixedsize = \"true\"\n"
		"\t\twidth = \"25\"\n"
		"\t\tshape = \"circle\"\n"
		"\t\tstyle = \"filled\"\n"
		"\t\tcolor = \"black\"\n"
		"\t\tfillcolor = \"black\"\n"
		"\t\tfontcolor = \"white\"\n"
		"\t\tfontsize = \"600\"\n"
		"\t];\n";
	const char graphviz_active_node_fmt[] =
		"\tnode%d [\n"
		"\t\tlabel = \"%d\"\n"
		"\t\tpos = \"%d,%d!\"\n"
		"\t\tfixedsize = \"true\"\n"
		"\t\twidth = \"80\"\n"
		"\t\tshape = \"circle\"\n"
		"\t\tstyle = \"filled\"\n"
		"\t\tcolor = \"blue\"\n"
		"\t\tfillcolor = \"blue\"\n"
		"\t\tfontcolor = \"white\"\n"
		"\t\tfontsize = \"2000\"\n"
		"\t];\n";
	const char graphviz_edge_fmt[] =
		"\tnode%d -- node%d [\n"
		"\t\tcolor = \"blue\"\n"
		"\t\tpenwidth = 3\n"
		"\t];\n";
	const char graphviz_tail[] = "}\n";

	char buf[1024];  /* Auxiliary buffer */
	char tmp_fname[] = ".neato.XXXXXX";
	const int fd = mkstemp(tmp_fname);

	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;

	if (fd == -1) {
		perror("failed to open temporary file");
		return;
	}

	write(fd, graphviz_head, sizeof(graphviz_head) - 1);

	/* Draw vacant nodes */
	for (size_t i = 0; i < vacant->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(vacant, i);
		const size_t nbytes = sprintf(
			buf, graphviz_vacant_node_fmt,
			(int)(i + active->size), node.cost, node.x, node.y
		);
		write(fd, buf, nbytes);
	}

	/* Draw active nodes */
	for (size_t i = 0; i < active->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(active, i);
		const size_t nbytes = sprintf(
			buf, graphviz_active_node_fmt,
			(int)i, node.cost, node.x, node.y
		);
		write(fd, buf, nbytes);
	}

	/* Draw edges between active nodes */
	for (size_t i = 1; i < active->size; i++) {
		const size_t nbytes = sprintf(
			buf, graphviz_edge_fmt,
			(int)(i - 1), (int)i
		);
		write(fd, buf, nbytes);
	}
	{
		const size_t nbytes = sprintf(
			buf, graphviz_edge_fmt,
			0, (int)active->size - 1
		);
		write(fd, buf, nbytes);
	}

	write(fd, graphviz_tail, sizeof(graphviz_tail) - 1);
	sprintf(buf, "neato -Tpdf '%s' >'%s'", tmp_fname, fpath);
	system(buf);  /* I am aware this is a horrible security leak -- doesn't matter, it's just a uni project */
	unlink(tmp_fname);
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
	printf("score: %lu\n", tsp_nodes_evaluate(graph->nodes_active));
}

int _print_node(const void *ptr)
{
	const struct tsp_node *const node = ptr;
	printf("Node(x=%4d, y=%4d, cost=%4d)\n", node->x, node->y, node->cost);
	return 0;
}

unsigned long tsp_nodes_evaluate(const struct sp_stack *nodes)
{
	struct tsp_node prev_node = *(struct tsp_node*)sp_stack_get(nodes, 0);
	unsigned long score = prev_node.cost;
	for (size_t i = 1; i < nodes->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(nodes, i);
		const double dist = euclidean_dist(prev_node.x, prev_node.y, node.x, node.y);
		score += node.cost + ROUND_SCORE(dist);
		prev_node = node;
	}

	const struct tsp_node first_node = *(struct tsp_node*)sp_stack_get(nodes, 0);
	const struct tsp_node last_node = *(struct tsp_node*)sp_stack_get(nodes, nodes->size - 1);
	const double dist = euclidean_dist(last_node.x, last_node.y, first_node.x, first_node.y);
	return score + ROUND_SCORE(dist);
}

/* Deactivates all nodes in a graph. */
void tsp_graph_deactivate_all(struct tsp_graph *graph)
{
	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;

	while (active->size != 0) {
		struct tsp_node node;
		node = *(struct tsp_node*)sp_stack_peek(active);
		sp_stack_pop(active, NULL);
		sp_stack_push(vacant, &node);
	}
}

/* Activates a single node in a graph. */
void tsp_graph_activate_node(struct tsp_graph *graph, size_t idx)
{
	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;

	const struct tsp_node node = *(struct tsp_node*)sp_stack_get(vacant, idx);
	sp_stack_qremove(vacant, idx, NULL);
	sp_stack_push(active, &node);
}

/* Activates `n_nodes` random nodes in graph. */
void tsp_graph_activate_random(struct tsp_graph *graph, size_t n_nodes)
{
	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;

	if (vacant->size < n_nodes) {
		warn(("tsp_graph_activate_function: not enough vacant nodes, truncating"));
		n_nodes = vacant->size;
	}
	for (size_t i = 0; i < n_nodes; i++) {
		/* Move a random node from vacant to active */
		const int idx = randint(0, vacant->size - 1);
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(vacant, idx);
		sp_stack_qremove(vacant, idx, NULL);
		sp_stack_push(active, &node);
	}
}
