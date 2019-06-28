#include <routingkit/customizable_contraction_hierarchy.h>
#include <routingkit/vector_io.h>
#include <routingkit/timer.h>
#include <routingkit/inverse_vector.h>
#include <iostream>


int main(int argc, char **argv) {
    if (argc != 6) {
        std::cout << "Usage: " << argv[0] << " first_out head order metric cores" << std::endl;
        return 1;
    }

    std::string first_out_file = argv[1];
    std::string head_file = argv[2];
    std::string order_file = argv[3];
    std::string weight_file = argv[4];
    int cores = std::stoi(argv[5]);

    std::vector<unsigned> first_out = RoutingKit::load_vector<unsigned>(first_out_file);
    std::vector<unsigned> tail = RoutingKit::invert_inverse_vector(first_out);
    std::vector<unsigned> head = RoutingKit::load_vector<unsigned>(head_file);
    std::vector<unsigned> node_order = RoutingKit::load_vector<unsigned>(order_file);
    std::vector<unsigned> weight = RoutingKit::load_vector<unsigned>(weight_file);

    //std::cout << "read input " << first_out.size() << " " << tail.size() << " " << head.size() << " " << node_order.size() << " " << weight.size() << std::endl;
	RoutingKit::CustomizableContractionHierarchy cch(node_order, tail, head);
    //std::cout << "built CCH" << std::endl;
	RoutingKit::CustomizableContractionHierarchyMetric metric(cch, weight);
    //std::cout << "built metric" << std::endl;
	double time = -RoutingKit::get_micro_time();
	if (cores > 1) {
		RoutingKit::CustomizableContractionHierarchyParallelization parallel_custom(cch);
		parallel_custom.customize(metric, static_cast<unsigned int>(cores));
	}
	else {
        metric.customize();
	}
	time += RoutingKit::get_micro_time();
	std::cout << time << std::endl;
    return 0;
}
