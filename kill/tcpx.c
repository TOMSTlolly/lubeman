//
// Created by krata on 4/7/25.
//
#include <net/if.h>
#include <linux/if.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

//#define INTERFACE "enP4p65s0"
//#define INTERFACE "wlp0s20f3"
//#define INTERFACE "wlan0"
#define INTERFACE "eth0"
//
// MAC address is stored in ifr_hwaddr.sa_data
struct ifreq ifr;
unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
char interface[24]; // Buffer to store the MAC address string]


// Function to test connectivity using connect with timeout
int connect_with_timeout(int sock, struct sockaddr *server_address, socklen_t address_len, int timeout) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        perror("Failed to get socket flags");
        return -1;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("Failed to set socket to non-blocking mode");
        return -1;
    }

    int result = connect(sock, server_address, address_len);
    if (result < 0 && errno != EINPROGRESS) {
        perror("Immediate connection attempt failed");
        return -1;
    }

    fd_set write_fds;
    struct timeval tv;
    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    result = select(sock + 1, NULL, &write_fds, NULL, &tv);
    if (result <= 0) {
        if (result == 0) {
            fprintf(stderr, "Connection timed out\n");
        } else {
            perror("Select error");
        }
        return -1;
    }

    int so_error;
    socklen_t len = sizeof(so_error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
        perror("Getsockopt error");
        return -1;
    }

    if (so_error != 0) {
        fprintf(stderr, "Connection error: %s\n", strerror(so_error));
        return -1;
    }

    if (fcntl(sock, F_SETFL, flags) == -1) {
        perror("Failed to restore socket to blocking mode");
        return -1;
    }

    return 0;
}


bool get_mac_address(const char *interface){
    int sock;

    // Create a socket for communication
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        //printf("socket createtion failed \r\n");
	perror("Socket creation failed");
        return false;
    }

    // Specify the network interface
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    //printf("interface %s\r\n",ifr.ifr_name);
    // Fetch the MAC address using ioctl
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
        //printf("ioctl failed \r\n");
	perror("ioctl failed");
        close(sock);
        return false;
    }

    //printf("get_mac_address went fine \r\n");
    close(sock);
    return true;
}

// 127.0.0.1 , port = 5000, cmd = "ADS", par = "ERROR 18", response = NULL
int writetcp(char *ip, int port, char *cmd, char *par,char *response) {
    int sockfd;
    struct sockaddr_in server_addr;
    char mac_address[] = "00:1A:2B:3C:4D:5E"; // Replace with the ADS MAC address you want to send
    char buffer[1024];
    char line[128];

    // POZOR, INTERFACE MUSIM NASTAVIT NAPEVNO, zatim neumim zjistit, ktery z interfejsu ma pripojeni k internetu
    //printf("INTERFACE %s\r\n",INTERFACE);
    if (!get_mac_address(INTERFACE))
    {
	    printf("cannot get mac address from interface\r\n");
	    exit(0);
    }
    line[0]=0;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return(EXIT_FAILURE);
    }

    // Define server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip); // Replace with your server's IP address

    // Connect to the server with a timeout
    if (connect_with_timeout(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr), 5) < 0) {
        perror("connect_with_timeout failed");
        close(sockfd);
        return(EXIT_FAILURE);
    }

    // simuluj chybu adapteru a odesli na server
    // ADS mac_address ERROR 18
    // GDA 00:1A:2B:3C:4D:5E USERNAME
    snprintf(line,sizeof(line),"%s %02x:%02x:%02x:%02x:%02x:%02x %s \r\n",cmd, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],par);
    //printf("packet: %s", line);
    if (send(sockfd, line, strlen(line), 0) < 0) {
        perror("Failed to send message");
        close(sockfd);
        return(EXIT_FAILURE);
    }

    // prijmi zpravu ze serveru
    int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        perror("Failed to receive response");
    } else {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        //printf("Server response: %s\n", buffer);
    }
    printf("%s  %s \n",line,buffer);
    close(sockfd);
    strcpy(response,buffer);
    return 0;
}
