//
// Created by Victoria on 2018-04-04.
//

#include "MergerOperator.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

typedef struct node_thread_params {
    MergerOperator* inst; // the MergerOperator instance to operate on
    int port; // port that the leaf node will send packets to
    int node_id; // the id of the runtime node for this thread
} NODE_THREAD_PARAMS;


MergerOperator::MergerOperator() {
    printf("MergerOperator::MergerOperator \n");

    this->action_table = new ActionTable();
    this->merger_info = MergerInfo::get_dummy_merger_info();
    this->num_nodes = this->merger_info->get_port_to_node_map().size();

    // set up mutexes
    packet_map_mutex = PTHREAD_MUTEX_INITIALIZER;
}


typedef struct merge_thread_params {
    MergerOperator* inst; // the MergerOperator instance to operate on
    int packet_id; // the packet_id of the packets to merge
} MERGE_THREAD_PARAMS;


void* MergerOperator::merge_packet_wrapper(void *arg) {
    auto *tp = (MERGE_THREAD_PARAMS*) arg;
    MergerOperator *this_mo = tp->inst;
    this_mo->merge_packet(tp->packet_id);

    return nullptr;
}


void* MergerOperator::run_node_thread_wrapper(void *arg) {
    auto *tp = (NODE_THREAD_PARAMS*) arg;
    MergerOperator *this_mo = tp->inst;
    this_mo->run_node_thread(tp->port, tp->node_id);

    return nullptr;
}


/**
 * Listens on the given port for packets from the given runtime_id, merging as necessary
 *
 * @param port          The port to listen for packets from runtime_id
 * @param node_id    The id of the runtime node leaf
 */
void MergerOperator::run_node_thread(int port, int node_id) {
    printf("Calling run_node_thread with port: %d, node_id: %d\n", port, node_id);

    // opens a datagram socket and returns the fd or -1 */
    int sockfd = open_socket();
    if (sockfd < 0) {
        fprintf(stderr, "Cannot open socket: %s", strerror(errno));
        exit(-1);
    }
    printf("opened socket on port: %d\n", port);

    // binds socket with given fd to given port */
    bind_socket(sockfd, port);
    printf("binded socket\n");

    std::map<int, packet *>* this_pkt_map;
    while (true) {
        // listen for packets
        printf("\nlistening for data...\n");
        sockdata *pkt_data = receive_data(sockfd);
        packet* p = packet_from_data(pkt_data);

        printf("Echo: [%s] (%d bytes)\n", p->data, p->data_size);

        // add packet to packet_map
        pthread_mutex_lock(&packet_map_mutex);
        if (packet_map.count(p->ip_header->ip_id) == 0) {
            std::map<int, packet *>* new_node_map = new std::map<int, packet *>();
            packet_map.insert(std::make_pair(p->ip_header->ip_id, new_node_map));
        }
        this_pkt_map = packet_map.at(p->ip_header->ip_id);
        this_pkt_map->insert(std::make_pair(node_id, p));
        printf("Printing this_node_map\n");

        packet_map.insert(std::make_pair(p->ip_header->ip_id, this_pkt_map));
        pthread_mutex_unlock(&packet_map_mutex);

        print_packet_map();

        // if all packets have been received for this packet_id, begin merging process
        if ((int) this_pkt_map->size() == num_nodes) {
            pthread_t merge_thread;
            auto * tp = (MERGE_THREAD_PARAMS*) malloc(sizeof(MERGE_THREAD_PARAMS));
            tp->inst = this;
            tp->packet_id = p->ip_header->ip_id;
            pthread_create(&merge_thread, nullptr, MergerOperator::merge_packet_wrapper, tp);
            pthread_detach(merge_thread);
        }
    }
}


/**
 * Retrieves all packets for the given pkt_id stored in packet_map and outputs a merged packet
 * based on the conflict items in merger_info
 *
 * @param pkt_id    The id of the packet to merge
 * @return Pointer to a packet with all changes merged
 */
packet* MergerOperator::merge_packet(int pkt_id) {
    printf("MergerOperator::merge_packet\n");
    std::map<int, packet*>* this_pkt_map = packet_map.at(pkt_id);

    // convert this_pkt_map to a map from node ids to packet_infos
    std::map<int, MergerOperator::PACKET_INFO*>* pkt_info_map = packet_map_to_packet_info_map(this_pkt_map);
    printf("Printing pkt_info_map:\n");
    for (auto it = pkt_info_map->begin(); it != pkt_info_map->end(); ++it) {
        printf("%d -> {", it->first);
        for (auto it2 = it->second->written_fields.begin(); it2 != it->second->written_fields.end(); ++it2) {
            printf("%s, ", field::field_to_string(*it2).c_str());
        }
        printf("}\n");
    }
    printf("\n");
    if ((int) this_pkt_map->size() != num_nodes) {
        fprintf(stderr, "Called merge_packet on an invalid pkt_id\n");
    }
    std::vector<ConflictItem*> conflicts_list = merger_info->get_conflicts_list();

    bool was_changed = true; // has at least one merge conflict been resolved in this iteration?

    for (int i = 0; i < num_nodes; i++) {
        printf("Iterating through conflicts list: %d\n", i);
    }

    return nullptr;
}


/**
 * Converts a map from node ids to packets to a map from node ids to equivalent packet_infos
 * @return A map of runtime node ids to a packet_info struct equivalent to the node's
 *         packet in packet_map
 */
std::map<int, MergerOperator::PACKET_INFO*>* MergerOperator::packet_map_to_packet_info_map(std::map<int, packet*>* packet_map) {

    auto ret_map = new std::map<int, MergerOperator::PACKET_INFO*>();

    for (auto it = packet_map->begin(); it != packet_map->end(); ++it) {
        int node_id = it->first;
        packet* pkt = it->second;

        ret_map->insert(std::make_pair(node_id, packet_to_packet_info(pkt, node_id)));
    }

    return ret_map;
};


/**
 * Encapsulates the given packet into a packet_info struct
 *
 * @param packet    The packet to encapsulate
 * @param node_id   The id of the runtime node that send the input packet
 * @return Packet_info struct encapsulating the input packet instance
 */
MergerOperator::PACKET_INFO* MergerOperator::packet_to_packet_info(packet* pkt, int node_id) {
    auto* pi = (PACKET_INFO*) malloc(sizeof(PACKET_INFO));

    if (this->merger_info->get_node_map().count(node_id) == 0) {
        fprintf(stderr, "Called packet_to_packet_info on invalid node_id: %d", node_id);
        exit(-1);
    }
    RuntimeNode* rn = this->merger_info->get_node_map().at(node_id);

    pi->pkt = pkt;
    pi->written_fields = this->action_table->get_write_fields(rn->get_nf());
    return pi;
}


/**
 * Setup MergerOperator to start listening and merging packets
 */
void MergerOperator::run() {
    printf("MergerOperator::run() \n");

    // send up one thread to handle each leaf node
    std::map<int, int> port_to_node_map = this->merger_info->get_port_to_node_map();

    pthread_t threads[num_nodes];
    int thread_i = 0;
    for (auto it = port_to_node_map.begin(); it != port_to_node_map.end(); ++it) {
        auto * tp = (NODE_THREAD_PARAMS*) malloc(sizeof(NODE_THREAD_PARAMS));
        tp->inst = this;
        tp->port = it->first;
        tp->node_id = it->second;
        pthread_create(&threads[thread_i++], nullptr, MergerOperator::run_node_thread_wrapper, tp);
    }

    void *status;
    for (int i = 0; i < num_nodes; i++) {
        pthread_join(threads[i], &status);
    }
}

/**
 * Helper function that prints the contents of packet_map to stdout
 */
void MergerOperator::print_packet_map() {
    for (auto it = packet_map.begin(); it != packet_map.end(); ++it) {
        printf("id: %d -> {", it->first);
        std::map<int, packet*>* this_node_map2 = it->second;
        for (auto it2 = this_node_map2->begin(); it2 != this_node_map2->end(); ++it2) {
            printf("%d: %s, ", it2->first, it2->second->data);
        }
        printf("}\n");
    }
}
#pragma clang diagnostic pop