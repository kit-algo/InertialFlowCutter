#include <routingkit/customizable_contraction_hierarchy.h>
#include <routingkit/vector_io.h>
#include <routingkit/timer.h>
#include <routingkit/inverse_vector.h>
#include <iostream>

int main(int argc, char **argv) {
	if (argc != 7) {
		std::cout << "Usage: " << argv[0] << " first_out head order metric query_sources query_tails" << std::endl;
		return 1;
	}

	std::string first_out_file = argv[1];
	std::string head_file = argv[2];
	std::string order_file = argv[3];
	std::string weight_file = argv[4];
	std::string query_sources_file = argv[5];
	std::string query_targets_file = argv[6];

	std::vector<unsigned> first_out = RoutingKit::load_vector<unsigned>(first_out_file);
	std::vector<unsigned> tail = RoutingKit::invert_inverse_vector(first_out);
	std::vector<unsigned> head = RoutingKit::load_vector<unsigned>(head_file);
	std::vector<unsigned> node_order = RoutingKit::load_vector<unsigned>(order_file);
	std::vector<unsigned> weight = RoutingKit::load_vector<unsigned>(weight_file);
	std::vector<unsigned> query_sources = RoutingKit::load_vector<unsigned>(query_sources_file);
	std::vector<unsigned> query_targets = RoutingKit::load_vector<unsigned>(query_targets_file);

	RoutingKit::CustomizableContractionHierarchy cch(node_order, tail, head);
	RoutingKit::CustomizableContractionHierarchyMetric metric(cch, weight);
	metric.customize();
	RoutingKit::CustomizableContractionHierarchyQuery query(metric);
	double time = -RoutingKit::get_micro_time();
	for (int i = 0; i < (int) query_sources.size(); ++i) {
		unsigned s = query_sources[i]; unsigned t = query_targets[i];
		query.reset().add_source(s).add_target(t).run();
	}
	time += RoutingKit::get_micro_time();
	time /= query_sources.size();
	std::cout << time << std::endl;

	return 0;

}
