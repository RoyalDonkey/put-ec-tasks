#include "graph.h"
#include "helpers.h"
#include "../libstaple/src/staple.h"
#include "heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>

/* Compilation-time debug flags */
/* #define TSP_TEST_EVAL */
#define TSP_TEST_DELTA_CACHE

/* Auxiliary structs */
struct id_val_pair {
	size_t id;
	size_t val;
};


/* Forward declarations */
int _print_node(const void *ptr);


inline unsigned long mdist(size_t id1, size_t id2, const struct tsp_dist_matrix *matrix)
{
	return matrix->dist[id1 * matrix->size + id2];
}

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
	matrix->nodes = malloc_or_die(nodes->size * sizeof(struct tsp_node));
	for (size_t i = 0; i < nodes->size; i++) {
		const struct tsp_node node1 = *(struct tsp_node*)sp_stack_get(nodes, i);
		for (size_t j = i + 1; j < nodes->size; j++) {
			const struct tsp_node node2 = *(struct tsp_node*)sp_stack_get(nodes, j);
			const unsigned dist = ROUND(euclidean_dist(node1.x, node1.y, node2.x, node2.y));
			matrix->dist[node1.id * nodes->size + node2.id] = dist;
			matrix->dist[node2.id * nodes->size + node1.id] = dist;
		}
		matrix->nodes[node1.id] = node1;
	}
	matrix->size = nodes->size;
}

void tsp_dist_matrix_print(struct tsp_dist_matrix matrix)
{
	printf("tsp_dist_matrix_print()\n");
	printf("       ");
	for (size_t j = 0; j < matrix.size; j++) {
		printf("[%3zu]  ", j);
	}
	putchar('\n');
	for (size_t i = 0; i < matrix.size; i++) {
		printf("[%3zu]  ", i);
		for (size_t j = 0; j < matrix.size; j++) {
			const unsigned dist = matrix.dist[i * matrix.size + j];
			printf("%5u  ", dist);
		}
		putchar('\n');
	}
}

struct tsp_graph *tsp_graph_create(const struct sp_stack *nodes)
{
	struct tsp_graph *const graph = tsp_graph_empty();
	sp_stack_copy(graph->nodes_vacant, nodes, NULL);
	tsp_dist_matrix_init(&graph->dist_matrix, nodes);
	return graph;
}

struct tsp_graph *tsp_graph_empty(void)
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
	int n_vacant, n_active, n_total, err;
	unsigned next_id = 0;

	f = fopen(fpath, "r");
	if (f == NULL) {
		error(("file not found: %s", fpath));
	}
	err = fscanf(f, "%d;%d\n", &n_vacant, &n_active);
	assert(err == 2);
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
	printf("Node(id=%4d, x=%4d, y=%4d, cost=%4d)\n", node->id, node->x, node->y, node->cost);
	return 0;
}

unsigned long tsp_nodes_evaluate(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix)
{
	assert(nodes->size != 0);
	struct tsp_node prev_node = *(struct tsp_node*)sp_stack_peek(nodes);
	unsigned long score = prev_node.cost;
	for (size_t i = 1; i < nodes->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(nodes, i);
		score += node.cost + mdist(node.id, prev_node.id, matrix);
		prev_node = node;
	}

	const struct tsp_node first_node = *(struct tsp_node*)sp_stack_peek(nodes);
	const struct tsp_node last_node = *(struct tsp_node*)sp_stack_get(nodes, nodes->size - 1);
	return score + mdist(last_node.id, first_node.id, matrix);
}

/* Returns the difference in graph score, if a given move was made  */
long tsp_graph_evaluate_move(const struct tsp_graph *graph, struct tsp_move move)
{
	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;
	assert(move.dest < active->size);
	const struct tsp_node node = *(struct tsp_node*)sp_stack_get(vacant, move.src);
	const struct tsp_node prev_node = *(struct tsp_node*)sp_stack_get(active, move.dest % active->size);
	const struct tsp_node next_node = *(struct tsp_node*)sp_stack_get(active, (move.dest + active->size - 1) % active->size);
	return
		- mdist(prev_node.id, next_node.id, &graph->dist_matrix)
		+ mdist(node.id, prev_node.id, &graph->dist_matrix)
		+ mdist(node.id, next_node.id, &graph->dist_matrix)
		+ node.cost;
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
		const double delta = mdist(node.id, node2.id, matrix) + node2.cost;
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
			+ mdist(node.id, node1.id, matrix)
			+ mdist(node.id, node2.id, matrix)
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
			- mdist(node.id, prev_node.id, &graph->dist_matrix)
			+ mdist(nn_node.id, node.id, &graph->dist_matrix)
			+ mdist(nn_node.id, prev_node.id, &graph->dist_matrix)
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
		- mdist(first_node.id, last_node.id, &graph->dist_matrix)
		+ mdist(nn_node.id, first_node.id, &graph->dist_matrix)
		+ mdist(nn_node.id, last_node.id, &graph->dist_matrix)
		+ nn_node.cost;
	if (delta < lowest_delta) {
		ret.src = nn_node_idx;
		ret.dest = 0;
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

	/* This function is a performance mess. It uses a copy of vacant nodes
	 * for in-place shuffling, relies on node indices, but at the end the
	 * indices are out-of-sync with the original vacant nodes, so we also
	 * need to store copies of node IDs and find their "real" vacant indices
	 * afterwards. Yikes. */
	struct node_move {
		unsigned node_id;      /* ID of the node, for finding the "real" vacant indices after */
		struct tsp_move move;  /* Move from vacant_copy to graph */
	};

	struct sp_stack *const rcl = sp_stack_create(sizeof(struct node_move), size);
	struct tsp_graph graph_copy;
	graph_copy.nodes_active = graph->nodes_active;
	graph_copy.nodes_vacant = sp_stack_create(vacant->elem_size, vacant->size);
	graph_copy.dist_matrix = graph->dist_matrix;
	struct sp_stack *const vacant_copy = graph_copy.nodes_vacant;
	sp_stack_copy(vacant_copy, vacant, NULL);

	while (rcl->size < size) {
		struct node_move nm;
		const size_t pool_size = MAX(1, ROUND(p * vacant_copy->size));
		const size_t vacant_copy_true_size = vacant_copy->size;

		/* Find the best node to add to graph from a pool_size random sample */
		tail_shuffle(vacant_copy->data, vacant_copy->elem_size, vacant_copy->size, pool_size);

		/* Dirty hack - pretend the stack is smaller to only choose from the pool segment */
		vacant_copy->data = ((char*)vacant_copy->data) + (vacant_copy->size - pool_size) * vacant_copy->elem_size;
		vacant_copy->size = pool_size;

		if (active->size == 0) {
			nm.move.src = randint(0, vacant_copy->size - 1);
			nm.move.dest = 0;
		} else if (active->size == 1) {
			const struct tsp_node node = *(struct tsp_node*)sp_stack_peek(active);
			nm.move.src = tsp_nodes_find_nn(vacant_copy, &graph_copy.dist_matrix, node);
			nm.move.dest = 1;
		} else {
			const struct tsp_move best_move = tsp_graph_find_nc(&graph_copy);
			nm.move = best_move;
		}
		nm.node_id = ((struct tsp_node*)sp_stack_get(vacant_copy, nm.move.src))->id;

		/* Revert the dirty hack */
		vacant_copy->size = vacant_copy_true_size;
		vacant_copy->data = ((char*)vacant_copy->data) - (vacant_copy->size - pool_size) * vacant_copy->elem_size;

		/* Remove the best node from future candidates */
		sp_stack_qremove(vacant_copy, nm.move.src, NULL);

		/* Add the move that activates the best node to the RCL */
		sp_stack_push(rcl, &nm);
	}
	sp_stack_destroy(vacant_copy, NULL);

	/* Map rcl elements back to original graph's vacant indices */
	struct sp_stack *const moves = sp_stack_create(sizeof(struct tsp_move), rcl->size);
	size_t *const map = malloc_or_die((TSP_MAX_NODE_ID + 1) * sizeof(size_t));
	for (size_t i = 0; i < vacant->size; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(vacant, i);
		map[node.id] = i;
	}
	for (size_t i = 0; i < rcl->size; i++) {
		const struct node_move nm = *(struct node_move*)sp_stack_get(rcl, i);
		const struct tsp_move move = { map[nm.node_id], nm.move.dest };
		sp_stack_push(moves, &move);
	}
	free(map);

	sp_stack_destroy(rcl, NULL);
	return moves;
}

unsigned long tsp_graph_compute_2regret(const struct tsp_graph *graph, size_t vacant_idx)
{
	struct sp_stack *const active = graph->nodes_active;
	long max_deltas[2];
	assert(active->size >= 2);

	/* Populate an array of hypothetical cost changes (deltas) */
	for (size_t dest = 0; dest < 2; dest++) {
		const struct tsp_move move = { vacant_idx, dest };
		max_deltas[dest] = tsp_graph_evaluate_move(graph, move);
	}

	if (max_deltas[0] > max_deltas[1]) {
		const long t = max_deltas[0];
		max_deltas[0] = max_deltas[1];
		max_deltas[1] = t;
	}

	for (size_t dest = 2; dest < active->size; dest++) {
		const struct tsp_move move = { vacant_idx, dest };
		const long delta = tsp_graph_evaluate_move(graph, move);
		if (delta >= max_deltas[1]) {
			max_deltas[0] = max_deltas[1];
			max_deltas[1] = delta;
		} else if (delta > max_deltas[0]) {
			max_deltas[0] = delta;
		}
	}

	return max_deltas[1] - max_deltas[0];
}

struct tsp_move tsp_graph_find_2regret(const struct tsp_graph *graph, const struct sp_stack *rcl)
{
	struct tsp_move best_move = { SIZE_MAX, SIZE_MAX };
	long best_regret = LONG_MIN;

	for (size_t i = 0; i < rcl->size; i++) {
		struct tsp_move move = *(struct tsp_move*)sp_stack_get(rcl, i);
		const long regret = tsp_graph_compute_2regret(graph, move.src);
		if (regret > best_regret) {
			best_move = move;
			best_regret = regret;
		}
	}

	return best_move;
}

/* wsc - "Weighted sum criterion" (2-regret + best change to obj. function)
 * "ratio" - 0.0 == greedy cycle, 1.0 == 2-regret */
struct tsp_move tsp_graph_find_wsc(const struct tsp_graph *graph, const struct sp_stack *rcl, double ratio)
{
	struct tsp_move best_move = { SIZE_MAX, SIZE_MAX };
	double best_criterion = -DBL_MAX;
	assert(0.0 <= ratio && ratio <= 1.0);

	for (size_t i = 0; i < rcl->size; i++) {
		struct tsp_move move = *(struct tsp_move*)sp_stack_get(rcl, i);
		const double regret = tsp_graph_compute_2regret(graph, move.src);
		const double delta = tsp_graph_evaluate_move(graph, move);
		const double criterion = ratio * regret - ((1.0 - ratio) * delta);
		if (criterion > best_criterion) {
			best_move = move;
			best_criterion = regret;
		}
	}

	return best_move;
}

void tsp_graph_inter_swap(struct tsp_graph *graph, size_t vacant_idx, size_t active_idx)
{
	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;

	assert(active->elem_size <= 64);
	assert(active->elem_size == vacant->elem_size);
	assert(sizeof(size_t) <= 64);
	static char tmp[64];  /* Aux buffer for swaps */

	void *const n1 = sp_stack_get(vacant, vacant_idx);
	void *const n2 = sp_stack_get(active, active_idx);
	memcpy(tmp, n1, vacant->elem_size);
	memcpy(n1, n2, vacant->elem_size);
	memcpy(n2, tmp, vacant->elem_size);
}

void tsp_nodes_swap_nodes(struct sp_stack *nodes, size_t idx1, size_t idx2)
{
	assert(nodes->elem_size <= 64);
	assert(sizeof(size_t) <= 64);
	static char tmp[64];  /* Aux buffer for swaps */
	if (idx1 == idx2)
		return;

	void *const n1 = sp_stack_get(nodes, idx1);
	void *const n2 = sp_stack_get(nodes, idx2);
	memcpy(tmp, n1, nodes->elem_size);
	memcpy(n1, n2, nodes->elem_size);
	memcpy(n2, tmp, nodes->elem_size);
}

void tsp_nodes_swap_edges(struct sp_stack *nodes, size_t idx1, size_t idx2)
{
	assert(nodes->elem_size <= 64);
	assert(sizeof(size_t) <= 64);
	static char tmp[64];  /* Aux buffer for swaps */

	/* Make sure idx1 < idx2 */
	if (idx1 > idx2) {
		*(size_t*)tmp = idx1;
		idx1 = idx2;
		idx2 = *(size_t*)tmp;
	}

	for (size_t i = 0; i < (idx2 - idx1 + 1) / 2; i++) {
		tsp_nodes_swap_nodes(
			nodes,
			(idx1 + i) % nodes->size,
			(idx2 + nodes->size - i) % nodes->size
		);
	}
}

long tsp_graph_evaluate_inter_swap(const struct tsp_graph *graph, size_t vacant_idx, size_t active_idx)
{
	#ifdef TSP_TEST_EVAL
	const unsigned long score_before = tsp_nodes_evaluate(graph->nodes_active, &graph->dist_matrix);
	#endif /* TSP_TEST_EVAL */

	const struct sp_stack *const vacant = graph->nodes_vacant;
	const struct sp_stack *const active = graph->nodes_active;

	const struct tsp_node n1 = *(struct tsp_node*)sp_stack_get(vacant, vacant_idx);
	const struct tsp_node n2 = *(struct tsp_node*)sp_stack_get(active, active_idx);
	const size_t n2_prev_idx = (active_idx + active->size - 1) % active->size;
	const size_t n2_next_idx = (active_idx + 1) % active->size;
	const struct tsp_node n2_prev = *(struct tsp_node*)sp_stack_get(active, n2_prev_idx);
	const struct tsp_node n2_next = *(struct tsp_node*)sp_stack_get(active, n2_next_idx);
	const long delta =
		- mdist(n2.id, n2_prev.id, &graph->dist_matrix)
		- mdist(n2.id, n2_next.id, &graph->dist_matrix)
		- n2.cost
		+ mdist(n1.id, n2_prev.id, &graph->dist_matrix)
		+ mdist(n1.id, n2_next.id, &graph->dist_matrix)
		+ n1.cost;

	#ifdef TSP_TEST_EVAL
	struct tsp_graph *const debug_graph = tsp_graph_empty();
	tsp_graph_copy(debug_graph, graph);
	tsp_graph_inter_swap(debug_graph, vacant_idx, active_idx);
	const unsigned long score_after = tsp_nodes_evaluate(debug_graph->nodes_active, &debug_graph->dist_matrix);
	const long target_delta = score_after - score_before;
	if (delta != target_delta) {
		error(("incorrect delta: got %ld, expected %ld", delta, target_delta));
	}
	tsp_graph_destroy(debug_graph);
	#endif /* TSP_TEST_EVAL */

	return delta;
}

long tsp_nodes_evaluate_swap_nodes(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, size_t idx1, size_t idx2)
{
	#ifdef TSP_TEST_EVAL
	const unsigned long score_before = tsp_nodes_evaluate(nodes, matrix);
	#endif /* TSP_TEST_EVAL */

	const struct tsp_node n1 = *(struct tsp_node*)sp_stack_get(nodes, idx1);
	const struct tsp_node n2 = *(struct tsp_node*)sp_stack_get(nodes, idx2);
	const size_t n1_prev_idx = (idx1 + nodes->size - 1) % nodes->size;
	const size_t n1_next_idx = (idx1 + 1) % nodes->size;
	const size_t n2_prev_idx = (idx2 + nodes->size - 1) % nodes->size;
	const size_t n2_next_idx = (idx2 + 1) % nodes->size;
	const struct tsp_node n1_prev = *(struct tsp_node*)sp_stack_get(nodes, n1_prev_idx);
	const struct tsp_node n1_next = *(struct tsp_node*)sp_stack_get(nodes, n1_next_idx);
	const struct tsp_node n2_prev = *(struct tsp_node*)sp_stack_get(nodes, n2_prev_idx);
	const struct tsp_node n2_next = *(struct tsp_node*)sp_stack_get(nodes, n2_next_idx);
	const long delta =
		- mdist(n1.id, n1_prev.id, matrix)
		- mdist(n1.id, n1_next.id, matrix)
		- mdist(n2.id, n2_prev.id, matrix)
		- mdist(n2.id, n2_next.id, matrix)
		+ mdist(n2.id, (n1_prev.id != n2.id ? n1_prev : n1).id, matrix)
		+ mdist(n2.id, (n1_next.id != n2.id ? n1_next : n1).id, matrix)
		+ mdist(n1.id, (n2_prev.id != n1.id ? n2_prev : n2).id, matrix)
		+ mdist(n1.id, (n2_next.id != n1.id ? n2_next : n2).id, matrix);

	#ifdef TSP_TEST_EVAL
	struct sp_stack *const debug_nodes = sp_stack_create(sizeof(struct tsp_node), nodes->size);
	sp_stack_copy(debug_nodes, nodes, NULL);
	tsp_nodes_swap_nodes(debug_nodes, idx1, idx2);
	const unsigned long score_after = tsp_nodes_evaluate(debug_nodes, matrix);
	const long target_delta = score_after - score_before;
	if (delta != target_delta) {
		error(("incorrect delta: got %ld, expected %ld", delta, target_delta));
	}
	sp_stack_destroy(debug_nodes, NULL);
	#endif /* TSP_TEST_EVAL */

	return delta;
}

long tsp_nodes_evaluate_swap_edges(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, size_t idx1, size_t idx2)
{
	#ifdef TSP_TEST_EVAL
	const unsigned long score_before = tsp_nodes_evaluate(nodes, matrix);
	#endif /* TSP_TEST_EVAL */

	/* Make sure idx1 < idx2 */
	if (idx1 > idx2) {
		const size_t tmp = idx1;
		idx1 = idx2;
		idx2 = tmp;
	}

	const struct tsp_node n1 = *(struct tsp_node*)sp_stack_get(nodes, idx1);
	const struct tsp_node n2 = *(struct tsp_node*)sp_stack_get(nodes, idx2);
	const size_t n1_prev_idx = (idx1 + nodes->size - 1) % nodes->size;
	const size_t n2_next_idx = (idx2 + 1) % nodes->size;
	const struct tsp_node n1_prev = *(struct tsp_node*)sp_stack_get(nodes, n1_prev_idx);
	const struct tsp_node n2_next = *(struct tsp_node*)sp_stack_get(nodes, n2_next_idx);
	const long delta =
		- mdist(n1.id, n1_prev.id, matrix)
		- mdist(n2.id, n2_next.id, matrix)
		+ mdist(n1.id, (n2_next.id != n1.id ? n2_next : n2).id, matrix)
		+ mdist(n2.id, (n1_prev.id != n2.id ? n1_prev : n1).id, matrix);

	#ifdef TSP_TEST_EVAL
	struct sp_stack *const debug_nodes = sp_stack_create(sizeof(struct tsp_node), nodes->size);
	sp_stack_copy(debug_nodes, nodes, NULL);
	tsp_nodes_swap_edges(debug_nodes, idx1, idx2);
	const unsigned long score_after = tsp_nodes_evaluate(debug_nodes, matrix);
	const long target_delta = score_after - score_before;
	if (delta != target_delta) {
		error(("incorrect delta: got %ld, expected %ld", delta, target_delta));
	}
	sp_stack_destroy(debug_nodes, NULL);
	#endif /* TSP_TEST_EVAL */

	return delta;
}

bool id_val_pair_min_val_cmp(const void *a, const void *b)
{
	const struct id_val_pair *const p1 = a;
	const struct id_val_pair *const p2 = b;
	return p1->val > p2->val;
}

/* Returns a candidate matrix -- like dist_matrix, but true=candidate, false=non_candidate */
struct tsp_cand_matrix *tsp_graph_compute_candidates(const struct tsp_graph *graph, size_t n)
{
	assert(n > 0);
	const struct sp_stack *const vacant = graph->nodes_vacant;
	const struct sp_stack *const active = graph->nodes_active;
	const struct tsp_dist_matrix *const matrix = &graph->dist_matrix;
	const size_t n_nodes = vacant->size + active->size;
	assert(n_nodes == matrix->size);
	struct tsp_cand_matrix *const ret = tsp_cand_matrix_create(n_nodes);
	ret->size = n_nodes;

	/* Create a helper stack of all nodes */
	if (n >= n_nodes) {
		/* Every node is a candidate */
		memset(ret->cand, 0xff, n_nodes * n_nodes * sizeof(bool));
		return ret;
	}
	struct sp_stack *const nodes = sp_stack_create(sizeof(struct tsp_node), n_nodes);
	sp_stack_copy(nodes, vacant, NULL);
	for (size_t i = 0; i < active->size; i++) {
		sp_stack_push(nodes, sp_stack_get(active, i));
	}
	assert(nodes->size == n_nodes);

	struct tsp_heap *const heap = tsp_heap_create(sizeof(struct id_val_pair), n, id_val_pair_min_val_cmp);

	/* Consider each node in the graph */
	for (size_t i = 0; i < n_nodes; i++) {
		const struct tsp_node node = *(struct tsp_node*)sp_stack_get(nodes, i);

		/* Insert all other nodes into a distance-based min-heap */
		tsp_heap_clear(heap);
		for (size_t j = 0; j < n_nodes; j++) {
			if (j == i) {
				continue;
			}
			const struct tsp_node node_neighbor = *(struct tsp_node*)sp_stack_get(nodes, j);
			const struct id_val_pair p = {
				node_neighbor.id,
				mdist(node.id, node_neighbor.id, matrix),
			};
			tsp_heap_push(heap, &p);
		}

		/* Mark `n` best nodes as candidates */
		for (size_t k = 0; k < n; k++) {
			const struct id_val_pair candidate = *(struct id_val_pair*)tsp_heap_get(heap);
			tsp_heap_pop(heap);
			assert(matrix->dist[node.id * n_nodes + candidate.id] == candidate.val);
			assert(matrix->dist[candidate.id * n_nodes + node.id] == candidate.val);
			ret->cand[node.id * n_nodes + candidate.id] = true;
			ret->cand[candidate.id * n_nodes + node.id] = true;
		}
	}

	tsp_heap_destroy(heap);
	sp_stack_destroy(nodes, NULL);
	return ret;
}

struct tsp_cand_matrix *tsp_cand_matrix_create(size_t size)
{
	struct tsp_cand_matrix *const ret = malloc_or_die(sizeof(struct tsp_cand_matrix));
	ret->cand = calloc_or_die(size * size * sizeof(bool));
	ret->size = size;
	return ret;
}

void tsp_cand_matrix_destroy(struct tsp_cand_matrix *cand_matrix)
{
	free(cand_matrix->cand);
	free(cand_matrix);
}

bool tsp_graph_inter_swap_adds_candidate(const struct tsp_graph *graph, const struct tsp_cand_matrix *cand_matrix, size_t vacant_idx, size_t active_idx)
{
	const struct sp_stack *const vacant = graph->nodes_vacant;
	const struct sp_stack *const active = graph->nodes_active;

	const struct tsp_node n1 = *(struct tsp_node*)sp_stack_get(vacant, vacant_idx);
	const size_t n2_prev_idx = (active_idx + active->size - 1) % active->size;
	const size_t n2_next_idx = (active_idx + 1) % active->size;
	const struct tsp_node n2_prev = *(struct tsp_node*)sp_stack_get(active, n2_prev_idx);
	const struct tsp_node n2_next = *(struct tsp_node*)sp_stack_get(active, n2_next_idx);

	if (
		cand_matrix->cand[n1.id * cand_matrix->size + n2_prev.id] ||
		cand_matrix->cand[n1.id * cand_matrix->size + n2_next.id]
	) {
		return true;
	}
	return false;
}

bool tsp_nodes_swap_nodes_adds_candidate(const struct sp_stack *nodes, const struct tsp_cand_matrix *cand_matrix, size_t idx1, size_t idx2)
{
	const struct tsp_node n1 = *(struct tsp_node*)sp_stack_get(nodes, idx1);
	const struct tsp_node n2 = *(struct tsp_node*)sp_stack_get(nodes, idx2);
	const size_t n1_prev_idx = (idx1 + nodes->size - 1) % nodes->size;
	const size_t n1_next_idx = (idx1 + 1) % nodes->size;
	const size_t n2_prev_idx = (idx2 + nodes->size - 1) % nodes->size;
	const size_t n2_next_idx = (idx2 + 1) % nodes->size;
	const struct tsp_node n1_prev = *(struct tsp_node*)sp_stack_get(nodes, n1_prev_idx);
	const struct tsp_node n1_next = *(struct tsp_node*)sp_stack_get(nodes, n1_next_idx);
	const struct tsp_node n2_prev = *(struct tsp_node*)sp_stack_get(nodes, n2_prev_idx);
	const struct tsp_node n2_next = *(struct tsp_node*)sp_stack_get(nodes, n2_next_idx);

	if (
		cand_matrix->cand[n2.id * cand_matrix->size + (n1_prev.id != n2.id ? n1_prev.id : n1.id)] ||
		cand_matrix->cand[n2.id * cand_matrix->size + (n1_next.id != n2.id ? n1_next.id : n1.id)] ||
		cand_matrix->cand[n1.id * cand_matrix->size + (n2_prev.id != n1.id ? n2_prev.id : n2.id)] ||
		cand_matrix->cand[n1.id * cand_matrix->size + (n2_next.id != n1.id ? n2_next.id : n2.id)]
	) {
		return true;
	}
	return false;
}

bool tsp_nodes_swap_edges_adds_candidate(const struct sp_stack *nodes, const struct tsp_cand_matrix *cand_matrix, size_t idx1, size_t idx2)
{
	/* Make sure idx1 < idx2 */
	if (idx1 > idx2) {
		const size_t tmp = idx1;
		idx1 = idx2;
		idx2 = tmp;
	}

	const struct tsp_node n1 = *(struct tsp_node*)sp_stack_get(nodes, idx1);
	const struct tsp_node n2 = *(struct tsp_node*)sp_stack_get(nodes, idx2);
	const size_t n1_prev_idx = (idx1 + nodes->size - 1) % nodes->size;
	const size_t n2_next_idx = (idx2 + 1) % nodes->size;
	const struct tsp_node n1_prev = *(struct tsp_node*)sp_stack_get(nodes, n1_prev_idx);
	const struct tsp_node n2_next = *(struct tsp_node*)sp_stack_get(nodes, n2_next_idx);

	if (
		cand_matrix->cand[n1.id * cand_matrix->size + n2_next.id] ||
		cand_matrix->cand[n2.id * cand_matrix->size + n1_prev.id]
	) {
		return true;
	}
	return false;
}

bool *tsp_graph_cache_inter_swap_adds_candidates(const struct tsp_graph *graph, const struct tsp_cand_matrix *cand_matrix)
{
	bool *const ret = malloc_or_die(cand_matrix->size * cand_matrix->size * sizeof(bool));
	for (size_t i = 0; i < graph->nodes_active->size; i++) {
		for (size_t j = 0; j < graph->nodes_vacant->size; j++) {
			ret[i * cand_matrix->size + j] = tsp_graph_inter_swap_adds_candidate(graph, cand_matrix, i, j);
		}
	}
	return ret;
}

bool *tsp_nodes_cache_swap_nodes_adds_candidates(const struct sp_stack *nodes, const struct tsp_cand_matrix *cand_matrix)
{
	bool *const ret = malloc_or_die(cand_matrix->size * cand_matrix->size * sizeof(bool));
	for (size_t i = 0; i < nodes->size; i++) {
		for (size_t j = 0; j < nodes->size; j++) {
			ret[i * cand_matrix->size + j] = tsp_nodes_swap_nodes_adds_candidate(nodes, cand_matrix, i, j);
		}
	}
	return ret;
}

bool *tsp_nodes_cache_swap_edges_adds_candidates(const struct sp_stack *nodes, const struct tsp_cand_matrix *cand_matrix)
{
	bool *const ret = malloc_or_die(cand_matrix->size * cand_matrix->size * sizeof(bool));
	for (size_t i = 0; i < nodes->size; i++) {
		for (size_t j = 0; j < nodes->size; j++) {
			ret[i * cand_matrix->size + j] = tsp_nodes_swap_edges_adds_candidate(nodes, cand_matrix, i, j);
		}
	}
	return ret;
}

struct tsp_delta_cache *tsp_delta_cache_create(size_t size)
{
	struct tsp_delta_cache *const ret = malloc_or_die(sizeof(struct tsp_delta_cache));
	ret->inter_swap = malloc_or_die(size * size * sizeof(long));
	ret->swap_nodes = malloc_or_die(size * size * sizeof(long));
	ret->swap_edges = malloc_or_die(size * size * sizeof(long));
	for (size_t i = 0; i < size; i++) {
		for (size_t j = 0; j < size; j++) {
			/* LONG_MIN is the conventional value for "no value" */
			ret->inter_swap[i * size + j] = LONG_MIN;
			ret->swap_nodes[i * size + j] = LONG_MIN;
			ret->swap_edges[i * size + j] = LONG_MIN;
		}
	}
	ret->size = size;
	return ret;
}

void tsp_delta_cache_destroy(struct tsp_delta_cache *delta_matrix)
{
	free(delta_matrix->inter_swap);
	free(delta_matrix->swap_nodes);
	free(delta_matrix->swap_edges);
	free(delta_matrix);
}

long tsp_graph_evaluate_inter_swap_with_delta_cache(const struct tsp_graph *graph, size_t vacant_idx, size_t active_idx, struct tsp_delta_cache *cache)
{
	if (cache->inter_swap[active_idx * cache->size + vacant_idx] != LONG_MIN) {
		#ifdef TSP_TEST_DELTA_CACHE
		tsp_delta_cache_verify_inter_swap(cache, graph);
		#endif /* TSP_TEST_DELTA_CACHE */

		return cache->inter_swap[active_idx * cache->size + vacant_idx];
	}
	const long delta = tsp_graph_evaluate_inter_swap(graph, vacant_idx, active_idx);
	cache->inter_swap[active_idx * cache->size + vacant_idx] = delta;
	return delta;
}

long tsp_nodes_evaluate_swap_nodes_with_delta_cache(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, size_t idx1, size_t idx2, struct tsp_delta_cache *cache)
{
	if (cache->swap_nodes[idx1 * cache->size + idx2] != LONG_MIN) {
		return cache->swap_nodes[idx1 * cache->size + idx2];
	}
	const long delta = tsp_nodes_evaluate_swap_nodes(nodes, matrix, idx1, idx2);
	cache->swap_nodes[idx1 * cache->size + idx2] = delta;
	cache->swap_nodes[idx2 * cache->size + idx1] = delta;
	return delta;
}

long tsp_nodes_evaluate_swap_edges_with_delta_cache(const struct sp_stack *nodes, const struct tsp_dist_matrix *matrix, size_t idx1, size_t idx2, struct tsp_delta_cache *cache)
{
	if (cache->swap_nodes[idx1 * cache->size + idx2] != LONG_MIN) {
		return cache->swap_edges[idx1 * cache->size + idx2];
	}
	const long delta = tsp_nodes_evaluate_swap_edges(nodes, matrix, idx1, idx2);
	/* cache->swap_edges[idx1 * cache->size + idx2] = delta; */
	/* cache->swap_edges[idx2 * cache->size + idx1] = delta; */
	return delta;
}

void tsp_graph_inter_swap_with_delta_cache(struct tsp_graph *graph, size_t vacant_idx, size_t active_idx, struct tsp_delta_cache *cache)
{
	struct sp_stack *const active = graph->nodes_active;

	const size_t n2_prev_idx = (active_idx + active->size - 1) % active->size;
	const size_t n2_next_idx = (active_idx + 1) % active->size;
	tsp_graph_inter_swap(graph, vacant_idx, active_idx);
	tsp_graph_update_delta_cache_for_node(graph, cache, n2_prev_idx);
	tsp_graph_update_delta_cache_for_node(graph, cache, n2_next_idx);

	#ifdef TSP_TEST_DELTA_CACHE
	tsp_delta_cache_verify_inter_swap(cache, graph);
	#endif /* TSP_TEST_DELTA_CACHE */
}

void tsp_graph_swap_nodes_with_delta_cache(struct tsp_graph *graph, size_t idx1, size_t idx2, struct tsp_delta_cache *cache)
{
	struct sp_stack *const active = graph->nodes_active;

	const size_t n1_prev_idx = (idx1 + active->size - 1) % active->size;
	const size_t n1_next_idx = (idx1 + 1) % active->size;
	const size_t n2_prev_idx = (idx2 + active->size - 1) % active->size;
	const size_t n2_next_idx = (idx2 + 1) % active->size;
	tsp_nodes_swap_nodes(graph->nodes_active, idx1, idx2);
	tsp_graph_update_delta_cache_for_node(graph, cache, n1_next_idx);
	tsp_graph_update_delta_cache_for_node(graph, cache, n1_prev_idx);
	tsp_graph_update_delta_cache_for_node(graph, cache, n2_next_idx);
	tsp_graph_update_delta_cache_for_node(graph, cache, n2_prev_idx);

	#ifdef TSP_TEST_DELTA_CACHE
	tsp_delta_cache_verify_swap_nodes(cache, graph);
	#endif /* TSP_TEST_DELTA_CACHE */
}

void tsp_graph_swap_edges_with_delta_cache(struct tsp_graph *graph, size_t idx1, size_t idx2, struct tsp_delta_cache *cache)
{
	struct sp_stack *const active = graph->nodes_active;

	const size_t n1_prev_idx = (idx1 + active->size - 1) % active->size;
	const size_t n1_next_idx = (idx1 + 1) % active->size;
	const size_t n2_prev_idx = (idx2 + active->size - 1) % active->size;
	const size_t n2_next_idx = (idx2 + 1) % active->size;
	tsp_nodes_swap_edges(graph->nodes_active, idx1, idx2);
	tsp_graph_update_delta_cache_for_node(graph, cache, n1_next_idx);
	tsp_graph_update_delta_cache_for_node(graph, cache, n1_prev_idx);
	tsp_graph_update_delta_cache_for_node(graph, cache, n2_next_idx);
	tsp_graph_update_delta_cache_for_node(graph, cache, n2_prev_idx);

	#ifdef TSP_TEST_DELTA_CACHE
	tsp_delta_cache_verify_swap_edges(cache, graph);
	#endif /* TSP_TEST_DELTA_CACHE */
}

void tsp_delta_cache_verify_inter_swap(const struct tsp_delta_cache *cache, const struct tsp_graph *graph)
{
	struct sp_stack *const vacant = graph->nodes_vacant;
	struct sp_stack *const active = graph->nodes_active;

	for (size_t active_idx = 0; active_idx < cache->size; active_idx++) {
		const size_t active_id = ((struct tsp_node*)sp_stack_get(active, active_idx))->id;
		for (size_t vacant_idx = 0; vacant_idx < cache->size; vacant_idx++) {
			const size_t vacant_id = ((struct tsp_node*)sp_stack_get(vacant, vacant_idx))->id;
			const long cached_inter_delta = cache->inter_swap[active_id * cache->size + vacant_id];
			const long true_inter_delta = tsp_graph_evaluate_inter_swap(graph, vacant_idx, active_idx);
			if (cached_inter_delta != LONG_MIN && cached_inter_delta != true_inter_delta) {
				error(("incorrect delta: got %ld, expected %ld", cached_inter_delta, true_inter_delta));
			}
		}
	}
}

void tsp_delta_cache_verify_swap_nodes(const struct tsp_delta_cache *cache, const struct tsp_graph *graph)
{
	struct sp_stack *const active = graph->nodes_active;

	for (size_t idx1 = 0; idx1 < active->size; idx1++) {
		for (size_t idx2 = idx1; idx2 < active->size; idx2++) {
			const size_t id1 = ((struct tsp_node*)sp_stack_get(active, idx1))->id;
			const size_t id2 = ((struct tsp_node*)sp_stack_get(active, idx2))->id;
			const long cached_edges_delta1 = cache->swap_edges[id1 * cache->size + id2];
			const long cached_edges_delta2 = cache->swap_edges[id2 * cache->size + id1];
			const long true_edges_delta = tsp_nodes_evaluate_swap_edges(active, &graph->dist_matrix, idx1, idx2);
			if (cached_edges_delta1 != cached_edges_delta2) {
				error(("diagonal delta mismatch: %ld != %ld", cached_edges_delta1, cached_edges_delta2));
			}
			if (cached_edges_delta1 != LONG_MIN && cached_edges_delta1 != true_edges_delta) {
				error(("incorrect delta: got %ld, expected %ld", cached_edges_delta1, true_edges_delta));
			}
		}
	}
}

void tsp_delta_cache_verify_swap_edges(const struct tsp_delta_cache *cache, const struct tsp_graph *graph)
{
	struct sp_stack *const active = graph->nodes_active;

	for (size_t idx1 = 0; idx1 < active->size; idx1++) {
		for (size_t idx2 = idx1; idx2 < active->size; idx2++) {
			const size_t id1 = ((struct tsp_node*)sp_stack_get(active, idx1))->id;
			const size_t id2 = ((struct tsp_node*)sp_stack_get(active, idx2))->id;
			const long cached_edges_delta1 = cache->swap_edges[id1 * cache->size + id2];
			const long cached_edges_delta2 = cache->swap_edges[id2 * cache->size + id1];
			const long true_edges_delta = tsp_nodes_evaluate_swap_edges(active, &graph->dist_matrix, idx1, idx2);
			if (cached_edges_delta1 != cached_edges_delta2) {
				error(("diagonal delta mismatch: %ld != %ld", cached_edges_delta1, cached_edges_delta2));
			}
			if (cached_edges_delta1 != LONG_MIN && cached_edges_delta1 != true_edges_delta) {
				error(("incorrect delta: got %ld, expected %ld", cached_edges_delta1, true_edges_delta));
			}
		}
	}
}

void tsp_graph_update_delta_cache_for_node(const struct tsp_graph *graph, struct tsp_delta_cache *cache, size_t node_idx)
{
	const struct sp_stack *const vacant = graph->nodes_vacant;
	const struct sp_stack *const active = graph->nodes_active;
	const size_t node_id = ((struct tsp_node*)sp_stack_get(active, node_idx))->id;

	/* Update inter_swap deltas between the target node and all vacant nodes */
	for (size_t vacant_idx = 0; vacant_idx < vacant->size; vacant_idx++) {
		const size_t vacant_id = ((struct tsp_node*)sp_stack_get(vacant, vacant_idx))->id;
		cache->inter_swap[node_id * cache->size + vacant_id] = tsp_graph_evaluate_inter_swap(graph, vacant_idx, node_idx);
	}

	/* Update swap_nodes/edges deltas between the target node and all active nodes */
	for (size_t active_idx = 0; active_idx < active->size; active_idx++) {
		const size_t active_id = ((struct tsp_node*)sp_stack_get(active, active_idx))->id;
		const long swap_nodes_delta = tsp_nodes_evaluate_swap_nodes(active, &graph->dist_matrix, active_idx, node_idx);
		const long swap_edges_delta = tsp_nodes_evaluate_swap_edges(active, &graph->dist_matrix, active_idx, node_idx);
		cache->swap_nodes[node_id * cache->size + active_id] = swap_nodes_delta;
		cache->swap_nodes[active_id * cache->size + node_id] = swap_nodes_delta;
		cache->swap_edges[node_id * cache->size + active_id] = swap_edges_delta;
		cache->swap_edges[active_id * cache->size + node_id] = swap_edges_delta;
	}
}
