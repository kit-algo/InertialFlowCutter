#!/bin/sh
G=$1
echo -n metis,$G,
./../build/console load_routingkit_unweighted_graph $G/first_out $G/head add_back_arcs remove_loops remove_multi_arcs assign_constant_arc_weights 1 save_metis_graph tmp
./ndmetis tmp | grep "Ordering" | sed -E 's/^[ \t]*Ordering:[ \t]+([0-9.]+)[ \t]+sec[ \t]+\(METIS time\)[ \t]*$/\1/'
./../build/console load_routingkit_unweighted_graph $G/first_out $G/head permutate_nodes tmp.iperm save_routingkit_node_permutation_since_last_load $G.metis.order
rm tmp tmp.iperm
