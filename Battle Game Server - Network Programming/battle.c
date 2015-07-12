/*
 * A4 Network Programming Game Server:
 * This is the server for a text-based battle game, communicating over 
 * the network with clients and listening either for chatter from a client
 * _or_ for a new connection.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "battle.h"

/* Initializes the pseudo-random number generator */
void initialize_number_generator(void){
  srand((unsigned) time(NULL));
}

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q; //socket address structure
    fd_set allset;
    fd_set rset; //read set

    int i;

    initialize_number_generator(); 
    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;

        //waiting until one or more of the file descriptors become "ready"
        //The select function blocks the calling process until there is activity on 
        //any of the specified sets of file descriptors
        /* Timeout set to NULL: wait forever until new signal*/
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (nready == -1) {
            perror("select");
            continue;
        }
        if (FD_ISSET(listenfd, &rset)){
            printf("a new client is connecting\n");
            len = sizeof(q);
            //blocks waiting for a connection from the queue
            //returns new fd which refers to TCP connection with client
            //reads & writes happen on this new fd returned by accept
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            //This macro adds filedes to the file descriptor set allset.
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }

            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            cwrite(clientfd, "What is your name? ", 19); //prompt for their name
            addclient(&head, clientfd, q.sin_addr);
        }

        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        int result = handleclient(p, &head);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

/**
 * Displays for each player their game stats, as well as the remaining 
 * hitpoints of the opposing player.
 **/
void show_player_stats(struct client *p1, struct client *p2){
    char buf[BUFFER_SIZE] = {0}; 
    //print stats to buffer 
    snprintf(buf, (51 + sizeof p1->hitpoints + sizeof p1->powermoves + strlen(p2->name) + sizeof p2->hitpoints), 
            "Your hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n", 
            p1->hitpoints, p1->powermoves, p2->name, p2->hitpoints);               
    cwrite(p1->fd, buf, strlen(buf));    //write the buf to client
    snprintf(buf, (51 + sizeof p2->hitpoints + sizeof p2->powermoves + strlen(p1->name) + sizeof p1->hitpoints), 
            "Your hitpoints: %d\nYour powermoves: %d\n\n%s's hitpoints: %d\n", 
            p2->hitpoints, p2->powermoves, p1->name, p1->hitpoints);    
    cwrite(p2->fd, buf, strlen(buf));    //write the buf to client
}

/**
 * If the player p is currently active display a menu of valid commands 
 * for them, otherwise send them a waiting message.
 **/
void show_menu(struct client *p){
    char buf[BUFFER_SIZE] = {0}; 
    if (!p->active){
        snprintf(buf, 27 + strlen(p->last_opponent->name), 
            "Waiting for %s to strike...\n", p->last_opponent->name);
        cwrite(p->fd, buf, strlen(buf));
    } else {
        if (p->powermoves > 0)
            cwrite(p->fd, "\n(a)ttack\n(p)owermove\n(s)peak something\n", 41);
        else
            cwrite(p->fd, "\n(a)ttack\n(s)peak something\n", 29);
    }
}

/**
 * Matches a player by searching for a suitable partner starting from 
 * the beginning of the client list. If found, sets all the initial 
 * parameters of the match. 
 **/
int match_player(struct client *top, struct client *p){
  //searching for suitable opponent
  struct client *opp = top;
  while (opp) {
    //Checks if the player is not currently matched (engaged) and did not battle
    //player p most recently (last_opponent).
    if (p->fd != opp->fd && !opp->engaged && (!p->last_opponent || !opp->last_opponent || p->last_opponent->fd != opp->fd)){
      //There is a player available
      p->last_opponent = opp;
      opp->last_opponent = p;
      //Changes both players' 'engaged' variable to 1 to indicate they are currently in a match.
      p->engaged = 1;
      opp->engaged = 1;
      /* Each player starts a match with between 20 and 30 hitpoints */
      p->hitpoints = rand() % 11 + 20;
      opp->hitpoints = rand() % 11 + 20;
      /* Each player starts a match with between 1 and 3 powermoves */
      p->powermoves = rand() % 3 + 1;
      opp->powermoves = rand() % 3 + 1;

      char buf[BUFFER_SIZE] = {0}; 
      snprintf(buf, 14 + strlen(p->name), "You engage %s!\n", p->name);
      cwrite(opp->fd, buf, strlen(buf));
      snprintf(buf, 14 + strlen(opp->name), "You engage %s!\n", opp->name);
      cwrite(p->fd, buf, strlen(buf));

      show_player_stats(opp, p);
      int pactive = rand() % 2;
      if (pactive){ //changes one player to active randomly.
        p->active = 1;
      } else {
        opp->active = 1;
        
      }
      show_menu(p);
      show_menu(opp);
      break;
    }
    opp = opp->next;
  }
  return 0;
}

/* Adds the client with the socket fd to the list of clients. */
static void addclient(struct client **top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));
    p->fd = fd;
    p->ipaddr = addr;
    p->name = NULL;
    p->inbuf = 0;
    p->last_move = 'n'; //Client still need to provide a name.
    p->last_opponent = NULL;
    p->engaged = 1; //Sets 'engaged' by default because until the client is named, they cannot be matched.
    p->active = 0;
    p->next = NULL;

    struct client **nav;

    /* Navigates through the linked list to get the address of the NULL node at the end 
    of the list, and sets it to the new client node. Takes care of the case of an empty 
    list (top is the address of the pointer to the head node).*/
    for (nav = top; *nav; nav = &(*nav)->next);
    *nav = p;
}


/** 
 * Appends character w to the client's buffer. Returns a string
 * if a network newline is found (end of text message), or if the
 * buffer is at full capacity.
 **/
char *readline(struct client *p, char w){
    int where; // location of network newline
    p->buf[p->inbuf] = w; // Receive message
    p->inbuf++; //bytes currently in buffer, and index to position after data in buf
    where = find_network_newline(p->buf, p->inbuf);
    if (where >= 0){
        p->buf[where] = '\0';
        char * result = malloc(p->inbuf - 1); //number of data bytes currently in buffer, counting up to newline.
        strncpy(result, p->buf, where + 1);  
        memset(p->buf, 0, sizeof(p->buf) - 1);
        p->inbuf = 0;
        return result;
    } else if ((sizeof(p->buf) - 1) == p->inbuf){ //in case of buffer overflow.
        p->buf[p->inbuf] = '\0';
        char * result = malloc(p->inbuf + 1);
        strncpy(result, p->buf, p->inbuf + 1); 
        p->inbuf = 0;
        return result;
    }
    return NULL; //full line has not been read yet.
}

/*Search the first inbuf characters of buf for a network newline ("\r\n").
  Return the location of the '\r' if the network newline is found,
  or -1 otherwise.
*/
int find_network_newline(char *buf, int inbuf) {
  // Step 1: write this function
  // return the location of '\r' if found
  int i;
  for (i = 0; i < inbuf; i++){
    if (buf[i] == '\n'){
        return i;
    }
  }
  
  return -1; 
}

/**
 * Executes an in-game command (attack or powermove). Uses the same point/score parameters
 * as outlined in the handout. Decreases the opposing player's hitpoints, and resets and restores
 * the player's variables on the event of a losing or winning match.
 **/
struct client *execute_strike(struct client *p, char move, struct client *top) {

    char buf[BUFFER_SIZE] = {0}; 
    //print stats to buffer 
    int damage = rand() % 5 + 2; //regular attack damage between 2-6.    
    if (move == 'a'){ //REGULAR ATTACK
        
        snprintf(buf, 29 + strlen(p->last_opponent->name) + sizeof damage, "\nYou hit %s for %d damage!\n", p->last_opponent->name, damage);
        cwrite(p->fd, buf, strlen(buf));
        snprintf(buf, 29 + strlen(p->name) + sizeof damage, "%s hits you for %d damage!\n", p->name, damage);
        cwrite(p->last_opponent->fd, buf, strlen(buf));
        p->last_opponent->hitpoints -= damage;

    } else if (p->powermoves){ //POWERMOVE

        if ((rand() % 2)){ //successful hit (50% chance)
            damage *= 3; //three times the damage of a regular attack
            snprintf(buf, 29 + strlen(p->last_opponent->name) + sizeof damage, "\nYou hit %s for %d damage!\n", p->last_opponent->name, damage);
            cwrite(p->fd, buf, strlen(buf));
            snprintf(buf, 35 + strlen(p->name) + sizeof damage, "%s powermoves you for %d damage!\n", p->name, damage);
            cwrite(p->last_opponent->fd, buf, strlen(buf));
            p->last_opponent->hitpoints -= damage;
        } else { //target missed, change nothing
            cwrite(p->fd, "\nYou missed!\n", 13);
            snprintf(buf, 14 + strlen(p->name), "%s missed you!\n", p->name);
            cwrite(p->last_opponent->fd, buf, strlen(buf));
        }
        p->powermoves -= 1;
    } else {
        //no more powermoves, discard powermove command.
        return top;
    }
      
    if (p->last_opponent->hitpoints > 0){ //game is still on, hand turn over to opposing player.
        p->active = 0;
        p->last_opponent->active = 1;
        show_player_stats(p->last_opponent, p);
        show_menu(p);
        show_menu(p->last_opponent);
    }
    else { /* Handles the case when the opposing player loses the match */
        snprintf(buf, 48 + strlen(p->last_opponent->name), "%s gives up. You win!\n\nAwaiting next opponent...\n", p->last_opponent->name);
        cwrite(p->fd, buf, strlen(buf));
        
        snprintf(buf, 71 + strlen(p->name), "You are no match for %s. You scurry away...\n\nAwaiting next opponent...\n", p->name);
        cwrite(p->last_opponent->fd, buf, strlen(buf));
        
        //clears player's match statuses
        p->engaged = 0;
        p->active = 0;
        p->last_opponent->engaged = 0;
        p->last_opponent->active = 0;


        struct client **nav;
        struct client *t;
        struct client *opp = p->last_opponent;

        /* MOVES both clients to the END of the client list */
        //searching for one of p and opp clients and removing them.
        for (nav = &top; *nav && ((*nav)->fd != p->fd) && ((*nav)->fd != opp->fd); nav = &(*nav)->next);
        //change 
        if (*nav){
            t = (*nav)->next;
            *nav = t;
        } 
        //searching for the other client and removing them
        for (; *nav && ((*nav)->fd != p->fd) && ((*nav)->fd != opp->fd); nav = &(*nav)->next);

        if (*nav){
            t = (*nav)->next; 
            *nav = t;
        }
        //get the address of the NULL client node at the end of the list 
        for(; *nav; nav = &(*nav)->next);

        //adds current client and their opponent to the end of the list            
        *nav = p;
        p->next = opp;
        opp->next = NULL;

        //match single clients with new players if possible.
        match_player(top, p);
        match_player(top, opp);
    }
    return top;

}

/** 
 * Handles any character printed by a client, including all commands,
 * written speech or the client name. Disregards any irrelevant text
 * (such as when the client is not in a match, or it is not their turn).
 * Returns 0 on success, or -1 on death of a client (client quits). 
 **/
int handleclient(struct client *p, struct client **top) {
    char move;
    char outbuf[512];
    int len = read(p->fd, &move, 1);
    if (len > 0) {
        //Client is still in the state of typing their name.
        if (p->last_move == 'n'){ 
            char * name = readline(p, move);
            if (name) { //
                p->name = name;
                p->last_move = 0;
                p->engaged = 0;
                snprintf(outbuf, 9 + strlen(name) + 24, "Welcome, %s! Awaiting opponent...\n", name);
                cwrite(p->fd, outbuf, strlen(outbuf));
                snprintf(outbuf, 23 + strlen(name), "**%s enters the arena**\n", name);
                broadcast(*top, p->fd, outbuf, strlen(outbuf));
                match_player(*top, p);
            }
        //Client is speaking, so the character is interpreted as speech rather than an explicit command.
        } else if (p->last_move == 's') { 
            char * msg = readline(p, move);
            if (msg) { //the message has ended, or the client buffer is full
                p->last_move = 0;
                snprintf(outbuf, 14 + strlen(msg), "You speak: %s\n\n", msg);
                cwrite(p->fd, outbuf, strlen(outbuf));
                snprintf(outbuf, 30 + strlen(p->name) + strlen(msg), "%s takes a break to tell you:\n%s\n\n", p->name, msg);
                cwrite(p->last_opponent->fd, outbuf, strlen(outbuf));
                free(msg);
                //reiterate state of match for both players
                show_player_stats(p, p->last_opponent);
                show_menu(p);
                show_menu(p->last_opponent);
            }
        } else {
            if (!p->engaged | !p->active) //any text sent by the inactive player should be discarded
                return 0;
            if ((move == 'a') | (move == 'p')){
                 *top = execute_strike(p, move, *top);
            } else if (move == 's'){
                cwrite(p->fd, "\nSpeak: ", 8);
                p->last_move = 's';
            } else {
                //any invalid commands sent by the active player should be discarded.
            }
        }
        return 0;
    } else if (len == 0) { //Client has disconnected
        // socket is closed
        struct client *opp = p->last_opponent;
        if (p->engaged & (p->last_move != 'n')) { //If player p was previously in a match with another player
            snprintf(outbuf, 49 + strlen(p->name), "--%s dropped. You win!\n\nAwaiting next opponent...\n", p->name);
            cwrite(opp->fd, outbuf, strlen(outbuf));
            /* Clear all memory of the match for the opposing player */
            opp->last_opponent = NULL; 
            opp->active = 0;
            opp->engaged = 0;
            opp->last_move = 0;
            opp->inbuf = 0;
        } else if (opp) {
            //get rid of any lingering references to the disconnected client
            if (opp->last_opponent->fd == p->fd) 
                opp->last_opponent = NULL; //this client does not exist anymore, and their fd may be reused.
        }
        if (p->last_move != 'n'){
            snprintf(outbuf, 13 + strlen(p->name), "**%s leaves**\n", p->name);
            broadcast(*top, p->fd, outbuf, strlen(outbuf));
        }
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr)); //20
        return -1;
    } else { // shouldn't happen
        perror("read");
        return -1;
    }
}

 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;
    /* set up listening socket soc */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { //create socket
        perror("socket");
        exit(1);
    }
    int yes = 1;
    // Make sure we can reuse the port immediately after the
    // server terminates. Avoids the "address in use" error
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET; /*address family that is used for the socket you're creating 
                 *(in this case an Internet Protocol address).*/
    r.sin_addr.s_addr = INADDR_ANY; // listen on all network addresses
    r.sin_port = htons(PORT); // which port will we be listening on  (htons: Transforms to Network Byte Order, Big-Endian)

    // Associate the process with the address and a port
    // When a socket is created with socket(2), it exists in a name space 
    //(address family) but has no address assigned to it.
    // bind() assigns the address specified by r to the socket referred to by the file descriptor listenfd.
    if (bind(listenfd, (struct sockaddr *)&r, sizeof(r))) {
        perror("bind");
        exit(1);
    }
    // Sets up a queue in the kernel to hold pending connection
    //The argument n specifies the length of the queue for pending connections. 
    //When the queue fills, new clients attempting to connect fail with ECONNREFUSED 
    //until the server calls accept to accept a connection from the queue.
    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    //returns listening socket
    return listenfd;
}

/* Removes client from list, and matches any opponent left behind */ 
static struct client *removeclient(struct client *top, struct client *p) {

    struct client **nav;
    for (nav = &top; *nav && (*nav)->fd != p->fd; nav = &(*nav)->next);
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*nav) {
        struct client *t = (*nav)->next;
        printf("Removing client %d %s\n", p->fd, inet_ntoa((*nav)->ipaddr)); 
        int drop = (p->engaged & (p->last_move != 'n')) ? 1 : 0;
        struct client *opp = p->last_opponent;
        if ((*nav)->name){ //just in case the client exited without a name
            free((*nav)->name);
        }
        free(*nav);
        *nav = t;
        if (drop) {
            match_player(top, opp); //try to match the lone client with someone new
        }
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 p->fd);
    }
    return top;
}

/* Broadcasts a message 's' across all client fds */
static void broadcast(struct client *top, int eventfd, char *s, int size) {
    struct client *p;
    for (p = top; p; p = p->next) {
        if (p->fd == eventfd)
            continue;
        cwrite(p->fd, s, size);
    }
}

/* Write to client: wrapper function for the write call. Checks for errors on write. */
int cwrite (int clientfd, char *buf, int nbytes){
    if (write(clientfd, buf, nbytes) == -1) {
        //An error occurred while writing, problem with reading end of the client socket
        perror("write");
        return -1;
    }
    return 0;
}
