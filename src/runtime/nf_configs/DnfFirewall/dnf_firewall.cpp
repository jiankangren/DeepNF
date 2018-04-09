#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <vector>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>

#include "../../address_util.h"
#include "../../socket_util.h"



#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void int_handler(int signo);


int sockfd;
std::vector<address*> addresses;

int main(int argc,char **argv)
{
    signal(SIGINT, int_handler);

    if (argc < 3) {
        std::cerr << "./fw my_port destIP:port [destIP2:port2] .......\n";
        return -1;
    }

    // get this server's bind port
    int bind_port = atoi(argv[1]);
   
    for (int i = 2; i < argc; i++) {
        address *addr = address_from_string(argv[i]);
        if (addr != NULL) {
            addresses.push_back(addr);
        }
    }

    if (addresses.size() == 0) { 
        std::cerr << "No valid destination addresses\n";
        return -1; 
    }

    // setup log for this NF
    std::ofstream log;
    log.open("log.txt", std::ios::out);

    // create socket
    int sockfd = open_socket();

    // bind onto a port
    if (bind_socket(sockfd, bind_port) == -1) {
        std::cerr << "bind failure: " << strerror(errno) << std::endl;
        return -1;
    }
    printf("Firewall listening for packets on port: %d\n", bind_port);
    

    int count = 0;
    struct timeval tv;
    time_t nowtime;
    struct tm *nowtm;
    char tmbuf[64], buf[64];

    while (true) {
        count++;
        sockdata *pkt_data = receive_data(sockfd);
        if (pkt_data == NULL || pkt_data->size == 0) { 
            std::cerr << "packet receive error: " << strerror(errno) << std::endl;
            continue;
        }
        packet* p = packet_from_data(pkt_data);

        // process packet
        printf("\n-------------------------------------\n");
        printf("\nReceived packet:\n");
        p->print_info();

        // drops input packet
        p->nullify();

        printf("\nSending modified packet:\n");
        p->print_info();

        // forward packet
        for (address *addr : addresses) {
            if (send_packet(p, sockfd, addr) < 0) {
                fprintf(stderr, "Send packet error: %s", strerror(errno));
                exit(-1);
            }
        }

        gettimeofday(&tv, NULL);
        nowtime = tv.tv_sec;
        nowtm = localtime(&nowtime);
        strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
        snprintf(buf, sizeof buf, "%s.%06ld", tmbuf, tv.tv_usec);
        printf("Printing into log\n");
        log << tmbuf << "." << tv.tv_usec;
        if (count % 1 == 0) {
            count = 0;
            log.flush();
        }

        
    }

    return 0;
}

void int_handler(int signo)
{
    close(sockfd);
    exit(0);
}



// void run_firewall(struct packet *pkt_info)
// {
//     /* if dip = 80, send a null packet (drop packet) */
//     if (htons(pkt_info->tcp_header->dest) == 8000) {
//         pkt_info->nullify();
//     }
//     forward_packet(pkt_info->pkt, pkt_info->size);
// }


#pragma clang diagnostic pop