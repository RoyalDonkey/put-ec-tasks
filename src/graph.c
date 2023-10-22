#include "graph.h"
#include "helpers.h"
#include "../libstaple/src/staple.h"
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>


/* Forward declarations */
int _print_node(const void *ptr);


struct sp_stack *tsp_nodes_read(const char *fpath)
{
	FILE *f;
	struct sp_stack *nodes;
	unsigned next_id = 0;

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

		assert(next_id != UINT_MAX);  /* Overflow detection */
		node.id = next_id++;

		/* Append new node to stack */
		sp_stack_push(nodes, &node);
	}
	fclose(f);
	return nodes;
}

void tsp_dist_matrix_init(struct tsp_dist_matrix *matrix, const struct sp_stack *nodes)
{
	matrix->dist = malloc_or_die(nodes->size * nodes->size * sizeof(unsigned));
	for (size_t i = 0; i < nodes->size; i++) {
		for (size_t j = i + 1; j < nodes->size; j++) {
			const struct tsp_node node1 = *(struct tsp_node*)sp_stack_get(nodes, i);
			const struct tsp_node node2 = *(struct tsp_node*)sp_stack_get(nodes, j);
			const unsigned dist = DIST(node1, node2, NULL);
			matrix->dist[node1.id * nodes->size + node2.id] = dist;
			matrix->dist[node2.id * nodes->size + node1.id] = dist;
		}
	}
	matrix->size = nodes->size;
}

struct tsp_graph *tsp_graph_create(const struct sp_stack *nodes)
{
	struct tsp_graph *const graph = tsp_graph_empty();
	sp_stack_copy(graph->nodes_vacant, nodes, NULL);
	tsp_dist_matrix_init(&graph->dist_matrix, nodes);
	return graph;
}

struct tsp_graph *tsp_graph_empty()
{
	struct tsp_graph *graph;
	graph = malloc_or_die(sizeof(struct tsp_graph));
	graph->nodes_active = sp_stack_create(sizeof(struct tsp_node), 200);
	graph->nodes_vacant = sp_stack_create(sizeof(struct tsp_node), 200);
	graph->dist_matrix.dist = NULL;
	graph->dist_matrix.size = 0;
	return graph;
}

struct tsp_graph *tsp_graph_import(const char *fpath)
{
	FILE *f;
	struct tsp_graph *const graph = malloc_or_die(sizeof(struct tsp_graph));
	struct sp_stack *all_nodes;
	int n_vacant, n_active, n_total;
	unsigned next_id = 0;

	f = fopen(fpath, "r");
	if (f == NULL) {
		error(("file not found: %s", fpath));
	}
	assert(2 == fscanf(f, "%d;%d\n", &n_vacant, &n_active));
	n_total = n_vacant + n_active;

	graph->nodes_vacant = sp_stack_create(sizeof(struct tsp_node), n_total);
	graph->nodes_active = sp_stack_create(sizeof(struct tsp_node), n_total);
	all_nodes = sp_stack_create(sizeof(struct tsp_node), n_total);

	while (n_vacant-- > 0) {
		struct tsp_node node;
		int n;

		/* Parse line */
		n = fscanf(f, "%d;%d;%d\n", &node.x, &node.y, &node.cost);
		if (n == EOF)
			error(("unexpected EOF while importing from %s", fpath));
		else if (n != 3)
			error(("failed to parse %s: fscanf returned %d", fpath, n));

		assert(next_id != UINT_MAX);  /* Overflow detection */
		node.id = next_id++;

		/* Append new node to stack */
		sp_stack_push(graph->nodes_vacant, &node);
		sp_stack_push(all_nodes, &node);
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

		assert(next_id != UINT_MAX);  /* Overflow detection */
		node.id = next_id++;

		/* Append new node to stack */
		sp_stack_push(graph->nodes_active, &node);
		sp_stack_push(all_nodes, &node);
	}

	tsp_dist_matrix_init(&graph->dist_matrix, all_nodes);

	info(("successfully parsed %zu lines from %s", n_total, fpath));
	fclose(f);
	sp_stack_destroy(all_nodes, NULL);
	return graph;
}

void tsp_graph_copy(struct tsp_graph *dest, const struct tsp_graph *src)
{
	sp_stack_copy(dest->nodes_active, src->nodes_active, NULL);
	sp_stack_copy(dest->nodes_vacant, src->nodes_vacant, NULL);

	const size_t dist_matrix_nbytes = src->dist_matrix.size * src->dist_matrix.size * sizeof(unsigned);
	if (dest->dist_matrix.size != src->dist_matrix.size)
		dest->dist_matrix.dist = realloc(dest->dist_matrix.dist, dist_matrix_nbytes);
	memcpy(dest->dist_matrix.dist, src->dist_matrix.dist, dist_matrix_nbytes);
	dest->dist_matrix.size = src->dist_matrix.size;
}

void tsp_graph_destroy(struct tsp_graph *graph)
{
	sp_stack_destroy(graph->nodes_active, NULL);
	sp_stack_destroy(graph->nodes_vacant, NULL);
	free(graph->dist_matrix.dist);
	free(graph);
}

void tsp_node_print(struct tsp_node node)
{
	_print_node(&node);
}

bool tsp_node_eq(struct tsp_node node1, struct tsp_node node2)
{
	return node1.x == node2.x && node1.y == node2.y;
}

void tsp_nodes_print(const struct sp_stack *nodes)
{
	printf("%zu nodes:\n", nodes->size);
	sp_stack_print(nodes, _print_node);
	printf("score: %lu\n", tsp_nodes_evaluate(nodes, NULL));
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
		"\t\twidth = \"%f\"\n"
		"\t\tshape = \"circle\"\n"
		"\t\tstyle = \"filled\"\n"
		"\t\tcolor = \"0.000 0.000 0.000 0.800\"\n"
		"\t\tfillcolor = \"0.000 0.000 0.000 0.800\"\n"
		"\t\tfontcolor = \"white\"\n"
		"\t\tfontsize = \"%f\"\n"
		"\t];\n";
	const char graphviz_active_node_fmt[] =
		"\tnode%d [\n"
		"\t\tlabel = \"%d\"\n"
		"\t\tpos = \"%d,%d!\"\n"
		"\t\tfixedsize = \"true\"\n"
		"\t\twidth = \"%f\"\n"
		"\t\tshape = \"circle\"\n"
		"\t\tstyle = \"filled\"\n"
		"\t\tcolor = \"blue\"\n"
		"\t\tfillcolor = \"%3f 1.000 0.800 0.800\"\n"
		"\t\tfontcolor = \"white\"\n"
		"\t\tfontsize = \"%f\"\n"
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

	/* Find minimum and maximum node costs */
	int cost_min = INT_MAX;
	int cost_max = INT_MIN;
	for (size_t i = 0; i < vacant->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(vacant, i);
		cost_min = MIN(cost_min, node.cost);
		cost_max = MAX(cost_max, node.cost);
	}
	for (size_t i = 0; i < active->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(active, i);
		cost_min = MIN(cost_min, node.cost);
		cost_max = MAX(cost_max, node.cost);
	}

	if (fd == -1) {
		perror("failed to open temporary file");
		return;
	}

	write(fd, graphviz_head, sizeof(graphviz_head) - 1);

	/* Draw vacant nodes */
	for (size_t i = 0; i < vacant->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(vacant, i);
		const double cost_norm = (double)(node.cost - cost_min) / (cost_max - cost_min);
		const size_t nbytes = sprintf(
			buf, graphviz_vacant_node_fmt,
			(int)(i + active->size), node.cost, node.x, node.y,
			60.0 + 100.0 * cost_norm,
			1200.0 + 2000.0 * cost_norm
		);
		write(fd, buf, nbytes);
	}

	/* Draw active nodes */
	for (size_t i = 0; i < active->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(active, i);
		const double cost_norm = (double)(node.cost - cost_min) / (cost_max - cost_min);
		const size_t nbytes = sprintf(
			buf, graphviz_active_node_fmt,
			(int)i, node.cost, node.x, node.y,
			60.0 + 100.0 * cost_norm,
			0.5 * (1.0 - cost_norm),
			1200.0 + 2000.0 * cost_norm
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
	if (graph->nodes_active->size != 0)
		printf("score: %lu\n", tsp_nodes_evaluate(graph->nodes_active, &graph->dist_matrix));
}

int _print_node(const void *ptr)
{
	const struct tsp_node *const node = ptr;
	printf("Node(x=%4d, y=%4d, cost=%4d)\n", node->x, node->y, node->cost);
	return 0;
}

unsigned long tsp_nodes_evaluate(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix)
{
	assert(nodes->size != 0);
	struct tsp_node prev_node = *(struct tsp_node*)sp_stack_peek(nodes);
	unsigned long score = prev_node.cost;
	for (size_t i = 1; i < nodes->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(nodes, i);
		score += node.cost + DIST(node, prev_node, matrix);
		prev_node = node;
	}

	const struct tsp_node first_node = *(struct tsp_node*)sp_stack_peek(nodes);
	const struct tsp_node last_node = *(struct tsp_node*)sp_stack_get(nodes, nodes->size - 1);
	return score + DIST(last_node, first_node, matrix);
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

size_t tsp_nodes_find_nn(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, struct tsp_node node)
{
	size_t ret = 0;
	double lowest_delta = DBL_MAX;
	for (size_t i = 0; i < nodes->size; i++) {
		const struct tsp_node node2 = *(struct tsp_node*)sp_stack_get(nodes, i);
		const double delta = DIST(node, node2, matrix) + node2.cost;
		if (delta < lowest_delta) {
			ret = i;
			lowest_delta = delta;
		}
	}
	return ret;
}

size_t tsp_nodes_find_2nn(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, struct tsp_node node1, struct tsp_node node2)
{
	size_t ret = 0;
	double lowest_delta = DBL_MAX;
	for (size_t i = 0; i < nodes->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(nodes, i);
		const double delta =
			+ DIST(node, node1, matrix)
			+ DIST(node, node2, matrix)
			+ node.cost;
		if (delta < lowest_delta) {
			ret = i;
			lowest_delta = delta;
		}
	}
	return ret;
}

/* Output vacant idx and position where to insert it for minimum cycle length increase. */
struct tsp_move tsp_graph_find_nc(const struct tsp_graph *graph)
{
	struct tsp_move ret = { SIZE_MAX, SIZE_MAX };
	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;
	assert(active->size >= 2);
	struct tsp_node prev_node = *(struct tsp_node*)sp_stack_peek(active);

	double lowest_delta = DBL_MAX;
	for (size_t i = 1; i < active->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(active, i);
		struct tsp_node nn_node;
		size_t nn_node_idx;

		/* Find nearest neighbor to the two adjacent nodes in the cycle */
		nn_node_idx = tsp_nodes_find_2nn(vacant, &graph->dist_matrix, prev_node, node);
		nn_node = *(struct tsp_node*)sp_stack_get(vacant, nn_node_idx);

		/* Calculate difference in score if the considered vacant node
		 * was inserted between the 2 closest nodes */
		const double delta =
			- DIST(node, prev_node, &graph->dist_matrix)
			+ DIST(nn_node, node, &graph->dist_matrix)
			+ DIST(nn_node, prev_node, &graph->dist_matrix)
			+ nn_node.cost;
		if (delta < lowest_delta) {
			ret.src = nn_node_idx;
			ret.dest = i;
			lowest_delta = delta;
		}
		prev_node = node;
	}

	/* Consider the last edge from first to last node */
	const struct tsp_node first_node = *(struct tsp_node*)sp_stack_peek(active);
	const struct tsp_node last_node = *(struct tsp_node*)sp_stack_get(active, active->size - 1);
	struct tsp_node nn_node;
	size_t nn_node_idx;

	/* Find nearest neighbor to the two adjacent nodes in the cycle */
	nn_node_idx = tsp_nodes_find_2nn(vacant, &graph->dist_matrix, first_node, last_node);
	nn_node = *(struct tsp_node*)sp_stack_get(vacant, nn_node_idx);

	/* Calculate difference in score if the considered vacant node
	 * was inserted between the 2 closest nodes */
	const double delta =
		- DIST(first_node, last_node, &graph->dist_matrix)
		+ DIST(nn_node, first_node, &graph->dist_matrix)
		+ DIST(nn_node, last_node, &graph->dist_matrix)
		+ nn_node.cost;
	if (delta < lowest_delta) {
		ret.src = nn_node_idx;
		ret.dest = active->size;
		lowest_delta = delta;
	}

	return ret;
}

/* Given a graph, constructs an RCL based on the greedy cycle heuristic.
 * size - no. nodes from graph's vacant list that should be put in the RCL,
 * p    - proportional sample size (0.0 == random, 1.0 == greedy) */
struct sp_stack *tsp_graph_find_rcl(const struct tsp_graph *graph, size_t size, double p)
{
	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;
	assert(size <= vacant->size);
	assert(p >= 0.0 && p <= 1.0);

	struct sp_stack *const rcl = sp_stack_create(sizeof(struct tsp_move), size);
	const size_t pool_size = MAX(1, ROUND(p * vacant->size));
	struct tsp_graph graph_copy;
	graph_copy.nodes_active = graph->nodes_active;
	graph_copy.nodes_vacant = sp_stack_create(vacant->elem_size, vacant->size);
	graph_copy.dist_matrix = graph->dist_matrix;
	struct sp_stack *const vacant_copy = graph_copy.nodes_vacant;
	sp_stack_copy(vacant_copy, vacant, NULL);

	while (rcl->size < size) {
		struct tsp_move best_move;
		const size_t vacant_copy_true_size = vacant_copy->size;

                /* Find the best node to add to graph from a pool_size random sample */
                shuffle(vacant_copy->data, vacant_copy->elem_size, vacant_copy->size, pool_size);
		vacant_copy->size = pool_size;  /* Only choose from the pool segment */
		if (active->size == 0) {
			best_move.src = randint(0, vacant_copy->size - 1);
			best_move.dest = 0;
		} else if (active->size == 1) {
			const struct tsp_node node = *(struct tsp_node*)sp_stack_peek(active);
			best_move.src = tsp_nodes_find_nn(vacant_copy, &graph_copy.dist_matrix, node);
			best_move.dest = 1;
		} else {
			best_move = tsp_graph_find_nc(&graph_copy);
		}
		vacant_copy->size = vacant_copy_true_size;

		/* Remove the best node from future candidates */
		sp_stack_qremove(vacant_copy, best_move.src, NULL);

		/* Add the move that activates the best node to the RCL */
		sp_stack_push(rcl, &best_move);
	}

	sp_stack_destroy(vacant_copy, NULL);
	return rcl;
}

/* "deltas" is an auxiliary array that's shared between runs of this function to
 * avoid repeatedly allocating and deallocating memory */
unsigned long tsp_graph_compute_2regret(const struct tsp_graph *graph, struct tsp_move move, long *deltas)
{
	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;
	const struct tsp_node candidate_node = *(struct tsp_node*)sp_stack_get(vacant, move.src);
	assert(active->size >= 2);

	/* Populate an array of hypothetical cost changes (deltas) */
	struct tsp_node prev_node = *(struct tsp_node*)sp_stack_peek(active);
	for (size_t i = 1; i < active->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(active, i);
		deltas[i] =
			- DIST(node, prev_node, &graph->dist_matrix)
			+ DIST(candidate_node, node, &graph->dist_matrix)
			+ DIST(candidate_node, prev_node, &graph->dist_matrix);
	}
	{
		const struct tsp_node first_node = *(struct tsp_node*)sp_stack_peek(active);
		const struct tsp_node last_node = *(struct tsp_node*)sp_stack_get(active, active->size - 1);
		deltas[0] =
			- DIST(first_node, last_node, &graph->dist_matrix)
			+ DIST(candidate_node, first_node, &graph->dist_matrix)
			+ DIST(candidate_node, last_node, &graph->dist_matrix);
	}

	/* Find the 2 largest regrets */
	long regret[2];
	regret[0] = MIN(deltas[0], deltas[1]);
	regret[1] = MAX(deltas[0], deltas[1]);
	for (size_t i = 2; i < active->size; i++) {
		if (deltas[i] >= regret[1]) {
			regret[0] = regret[1];
			regret[1] = deltas[i];
		} else if (deltas[i] > regret[0]) {
			regret[0] = deltas[i];
		}
	}

	return regret[1] - regret[0];
}

struct tsp_move tsp_graph_find_2regret(const struct tsp_graph *graph, const struct sp_stack *rcl)
{
	struct sp_stack *const active = graph->nodes_active;
	long *const deltas = malloc_or_die(active->size * sizeof(long));
	struct tsp_move best_move = { SIZE_MAX, SIZE_MAX };
	long best_regret = LONG_MIN;

	for (size_t i = 0; i < rcl->size; i++) {
		struct tsp_move move = *(struct tsp_move*)sp_stack_get(rcl, i);
		const long regret = tsp_graph_compute_2regret(graph, move, deltas);
		if (regret > best_regret) {
			best_move = move;
			best_regret = regret;
		}
	}

	free(deltas);
	return best_move;
}
