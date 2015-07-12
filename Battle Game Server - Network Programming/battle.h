#ifndef _BATTLE_H
#define _BATTLE_H

#ifndef PORT
    #define PORT 30100
#endif

/* Maximum buffer size */
#define BUFFER_SIZE 512

struct client {
    char *name; 
    int fd; //file descriptor
    
    char buf[300];       // buffer to hold data being read from client
    int inbuf;            // how many bytes currently in buf?

    char last_move;
    struct in_addr ipaddr; //internet address
    struct client *next; //link/pointer to next client
    struct client *last_opponent; //latest opponent
    int engaged; //1 for currently engaged in match with other player
    int active;
    int hitpoints;
    int powermoves;
};

/* Add client to list of fds to listen for */
static void addclient(struct client **top, int fd, struct in_addr addr);

/* Remove client from list of clients */
static struct client *removeclient(struct client *top, struct client *p);

/* Broadcast a message to all connected clients */
static void broadcast(struct client *top, int eventfd, char *s, int size);

/* Handle commands or written lines from the client */
int handleclient(struct client *p, struct client **top);

/* Update client buffer with new character, return full message when available */
char * readline(struct client *p, char w);

/* Search buffer for presence of a network newline */
int find_network_newline(char *buf, int inbuf);

/* Match player with available LIFO client */
int match_player(struct client *top, struct client *p);

/* Returns FD of listening socket */
int bindandlisten(void);

/* Prints game statistics of each player match, hiding the other's powermoves */
void show_player_stats(struct client *p1, struct client *p2);

/* Displays a menu of available commands for the player */
void show_menu(struct client *p);

/* Based on player's move, updates points and handles winning/losing end of the match */
struct client *execute_strike(struct client *p, char move, struct client *top);

/* Wrapper function for write system call used for error checking */
int cwrite(int clientfd, char *buf, int nbytes);

#endif
