#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "lib/msg.h"
#include "lib/map.h"
#include "lib/ship.h"
#include "lib/socket.h"
#include "lib/util.h"

char opponent_nickname[30];

int main(int argc, char *argv[])
{
    Map *m = init_map_matrix(10, 10);

    int i, ship, x, y, orientation; // ship insertion
    char my_nickname[30];

    // Socket stuff
    int server_socket, client_socket;
    struct addrinfo hints, *server_info;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    char client_ip[INET6_ADDRSTRLEN];
    int status;

    // Messages
    int end_game;
    introducion_message *i_msg;
    attack_message *a_msg;
    response_message *r_msg;

    /* Welcome screen
     */
    welcome();

    // just for test!
    insert_ship(m, 0, 0, 0, 2);
    insert_ship(m, 1, 6, 0, 2);
    insert_ship(m, 2, 0, 2, 1);
    insert_ship(m, 2, 0, 6, 1);
    insert_ship(m, 3, 1, 9, 2);
    insert_ship(m, 3, 4, 9, 2);

    /* Get nickname
     */
    printf("Insert your nickname: ");
    fgets(my_nickname, 30, stdin);
    if (my_nickname[strlen(my_nickname) - 1] == '\n')
        my_nickname[strlen(my_nickname) - 1] = '\0';

    system("clear");

    /* Insert ships until its done
     */
    while((i = check_used_ships(m)) > 0)
    {
        // Show the map and the ships
        show_map(m);
        show_ships(m);

        printf("You still have %i ship(s) to organize\n", i);
        printf("Choose one to put in the map: ");
        scanf("%i", &ship);
        while(ship > 3 || ship < 0)
        {
            printf("Invalid choice\n");
            printf("Please, choose another one: ");
            scanf("%i", &ship);
        }

        printf("Ship head position\n");
        printf("x: ");
        scanf("%i", &x);
        while(x > m->width || x < 0)
        {
            printf("Invalid choice\n");
            printf("Please, choose another x: ");
            scanf("%i", &x);
        }

        printf("y: ");
        scanf("%i", &y);
        while(y > m->height || y < 0)
        {
            printf("Invalid choice\n");
            printf("Please, choose another y: ");
            scanf("%i", &y);
        }

        printf("Orientation (1-vert/2-hori): ");
        scanf("%i", &orientation);
        while(orientation != 1 && orientation != 2)
        {
            printf("Invalid choice\n");
            printf("Please, choose another option: ");
            scanf("%i", &orientation);
        }

        insert_ship(m, ship, x, y, orientation);

        system("clear");
    }

    show_map(m);

    printf("ships ready!\n");


    /* Connection
     */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((status = getaddrinfo(NULL, PORT, &hints, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // loop through all the results and bind to the first we can

    if ((server_socket = socket(server_info->ai_family,
         server_info->ai_socktype, server_info->ai_protocol)) == -1) {
        perror("server: socket");
    }

    if (bind(server_socket, server_info->ai_addr,
             server_info->ai_addrlen) == -1) {
        close(server_socket);
        perror("server: bind");
    }

    if (server_info == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(server_info);

    if (listen(server_socket, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Waiting for connections...\n");

    sin_size = sizeof client_addr;
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr,
                           &sin_size);
    if (client_socket == -1) {
        perror("accept");
    }

    inet_ntop(client_addr.ss_family,
        get_in_addr((struct sockaddr *)&client_addr),
        client_ip, sizeof client_ip);
    printf("server: got connection from %s\n", client_ip);

    // ------------------------------------------------------------------------
    // Send and receive introduction message

    i_msg = malloc(sizeof(introducion_message));
    i_msg->type = 1;
    strcpy(i_msg->nickname, my_nickname);

    if (send(client_socket, i_msg, sizeof(introducion_message), 0) == -1)
    {
        perror("send");
        exit(1);
    }

    if ((status = recv(client_socket, i_msg,
                       sizeof(introducion_message), 0)) == -1) {
        perror("recv");
        exit(1);
    }
    strcpy(opponent_nickname, i_msg->nickname);

    free(i_msg);
    system("clear");

    // ------------------------------------------------------------------------

    Map *client_map = init_map_matrix(10, 10);

    while(1)
    {
        // Server attacks -----------------------------------------------------
        printf("you: %s vs %s\n", my_nickname, opponent_nickname);
        printf("Your map:\n");
        show_map(m);
        printf("%s's map:\n", opponent_nickname);
        show_map(client_map);

        printf("Your turn to attack\n");
        printf("x: ");
        scanf("%i", &x);
        printf("y: ");
        scanf("%i", &y);

        // Send attack
        a_msg = malloc(sizeof(attack_message));
        a_msg->type = 2;
        a_msg->x = x;
        a_msg->y = y;
        send(client_socket, a_msg, sizeof(attack_message), 0);
        free(a_msg);

        // Get response
        r_msg = malloc(sizeof(response_message));
        recv(client_socket, r_msg, sizeof(response_message), 0);
        if (r_msg->response == 1)
        {
            client_map->map[y][x] = HIT;
            if (client_map->total == 1)
            {
                end_game = 1;
                break;
            }
            else
                client_map->total--;
        }
        else {
            client_map->map[y][x] = MISS;
        }
        free(r_msg);

        // Send ok
        // r_msg->response = OK;
        // send(client_socket, r_msg, sizeof(response_message), 0);

        system("clear");

        // Client attacks -----------------------------------------------------
        printf("you: %s vs %s\n", my_nickname, opponent_nickname);
        printf("Your map:\n");
        show_map(m);
        printf("%s's map:\n", opponent_nickname);
        show_map(client_map);

        // Receive attack
        printf("Waiting for an attack\n");
        a_msg = malloc(sizeof(attack_message));
        recv(client_socket, a_msg, sizeof(attack_message), 0);

        // Send response
        r_msg = malloc(sizeof(response_message));
        r_msg->type = 3;
        r_msg->response = attack_ship(m, a_msg->x, a_msg->y);

        if (r_msg->response == 1)
        {
            if (m->total == 1)
            {
                end_game = 0;
                break;
            }
            else
                m->total--;
        }
        send(client_socket, r_msg, sizeof(response_message), 0);
        free(a_msg);
        free(r_msg);

        // // Receive attack
        // r_msg = malloc(sizeof(response_message));
        // recv(client_socket, r_msg, sizeof(response_message), 0);
        // free(r_msg);

        // if (r_msg->response == OK)
        //     continue;

        system("clear");
    }

    if (!fork()) { // this is the child process
        close(server_socket); // child doesn't need the listener
        if (send(client_socket, "Hello, world!", 13, 0) == -1)
            perror("send");
        close(client_socket);
        exit(0);
    } else {
        close(client_socket);  // parent doesn't need this
    }

    return 0;

    }
