#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

#define SERVER_PORT 1234
#define QUEUE_SIZE 5
#define GAME_SIZE 10

pthread_mutex_t players = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wait_for_another_player[GAME_SIZE];
pthread_mutex_t room_mut[GAME_SIZE];

struct thread_data_t {
    int nr_deskryptora1;            // deskryptor powiązany z danym wątkiem
    int nr_deskryptora2;            // deskryptor przeciwnika z pary
    char data[6];                   // tablica do przesyładnia danych
    int pokoj;                      // numer pokoju 
    int numer;                      // identyfikator (ten, który jest przypisywany na samym początku)
    char * color;                   // kolor gracza
};

int all_rooms[GAME_SIZE][2];
pthread_t thread[2*GAME_SIZE];
struct thread_data_t t_data[2*GAME_SIZE];
int all_boards[GAME_SIZE][8][8];


// wypełnanie macierzy gry: id pionka lub 0
void create_board(int game[8][8]){
    int x, y;
    int man_index = 1;

    for(y=0; y<8; y++){
        for(x=0; x<8; x++){
            if ( ((x+y)%2 == 0) & (y<=2) ){
                game[y][x] = man_index;
                man_index++;
            }
            else if ( ((x+y)%2 == 0) & (y>=5) ){
                game[y][x] = man_index;
                man_index++;
            }
            else{
                game[y][x] = 0;
            }
        }
    }
}


// funkcja sprawdza rezultat rozgrywki
int * find_result(int game[8][8]) {
    int y, x, z;
    static int  res[2];

    int num_black = 0;
    int num_white = 0;

    // sprawdzanie ilosci pionkow
    for (y=0;y<8;y++){
        for(x=0;x<8;x++){
            if ( (game[y][x] >= 1) & (game[y][x] <= 12) ) {num_black++;}
            else if ( (game[y][x] >= 13) & (game[y][x] <= 24) ) {num_white++;}
        }
    }

    // jesli oboje zawodnicy maja pionki to sprawdz czy ktos doszedl na druga strone
    if (num_white * num_black != 0){
        for (z=0; z<8; z++){
            if ( (game[7][z] >= 1) & (game[7][z] <= 12) ) {num_white = 0;}
            else if ( (game[0][z] >= 13) & (game[0][z] <= 24) ) {num_black = 0;}
        }
    }

    res[0] = num_black;
    res[1] = num_white;

    return res;
}


// funkcja sprawdza porawnosc ruchu, zwraca 0 lub 1
int check_move( int game[8][8], char *color, char *move ){
    int row_from = move[0] - 65;
    int col_from = move[1] - 49;
    int row_to = move[3] - 65;
    int col_to = move[4] - 49;
    int active_man = game[row_from][col_from];
    int destination_field = game[row_to][col_to];
    int captured; 

    // "try catch" na wartosci poza zakresem 1-8 i A-H
    if (((row_from<0)|(row_from>7)) |
        ((row_to<0)|(row_to>7)) |
        ((col_from<0)|(col_from>7)) |
        ((col_to<0)|(col_to>7))){return 0;}

    // jesli zbijamy to zapisujemy id zbijanego pionka
    if( ((row_from - row_to == 2) | (row_from - row_to == -2)) &
        ((col_from - col_to == 2) | (col_from - col_to == -2)) ){ 
            captured = game[(row_from+row_to)/2][(col_from+col_to)/2]; 
    }
    else{captured = 0;}

    if ( strcmp(color, "black") == 0 ){
        if ( (row_from - row_to == -1) & (destination_field == 0) &
            ((col_from-col_to == -1)|(col_from-col_to == 1)) & 
            (active_man>=1) & (active_man<=12) ) {
                game[row_to][col_to] = active_man;
                game[row_from][col_from] = 0;
                return 1;}

        else if ( (row_from - row_to == -2) & (destination_field == 0) &
                ((col_from-col_to == -2)|(col_from-col_to == 2)) & 
                (active_man>=1) & (active_man<=12) & 
                (captured<=24) & (captured>=13) ){
                    game[row_to][col_to] = active_man;
                    game[row_from][col_from] = 0;
                    game[(row_from+row_to)/2][(col_from+col_to)/2] = 0;
                    return 1;}

        else{return 0;}
    }

    else if ( strcmp(color, "white") == 0 ){
        if ( (row_from - row_to == 1) & (destination_field == 0) &
            ((col_from-col_to == -1)|(col_from-col_to == 1)) & 
            (active_man<=24) & (active_man>=13) ) {
                game[row_to][col_to] = active_man;
                game[row_from][col_from] = 0;
                return 1;}

        else if ( (row_from - row_to == 2) & (destination_field == 0) &
                ((col_from-col_to == -2)|(col_from-col_to == 2)) & 
                (captured>=1) & (captured<=12) & 
                (active_man<=24) & (active_man>=13) ){
                    game[row_to][col_to] = active_man;                      
                    game[row_from][col_from] = 0;
                    game[(row_from+row_to)/2][(col_from+col_to)/2] = 0;                  
                    return 1;}

        else{return 0;}
    }

    else{return 0;}
}

//funkcja opisującą zachowanie wątku - musi przyjmować argument typu (void *) i zwracać (void *)
void *ThreadBehavior(void *t_data) {
    // pobranie struktury i konwersja
    struct thread_data_t *th_data = (struct thread_data_t*)t_data;
    int *result;
    int check_move_return, read_int, send_int;
    bool wakeup = true;
    bool run = true;
    int n;

    pthread_mutex_lock(&room_mut[(*th_data).pokoj]);

    while(wakeup){
        // czekamy na drugiego gracza i zapisujemy jedo deskryptor
        if (all_rooms[(*th_data).pokoj][0] * all_rooms[(*th_data).pokoj][1] == 0){
            pthread_cond_wait(&wait_for_another_player[(*th_data).pokoj], &room_mut[(*th_data).pokoj]);
            (*th_data).nr_deskryptora2 = all_rooms[(*th_data).pokoj][1];
        }
        else{wakeup=false;}
    }

    // uwalniamy pierwszego gracza i zapisujemy jedo deskryptor
    if(all_rooms[(*th_data).pokoj][1] == (*th_data).nr_deskryptora1){
        pthread_cond_signal(&wait_for_another_player[(*th_data).pokoj]);
        (*th_data).nr_deskryptora2 = all_rooms[(*th_data).pokoj][0];
    }

    pthread_mutex_unlock(&room_mut[(*th_data).pokoj]);

    create_board(all_boards[(*th_data).pokoj]);

    // rozdzielanie kolorow
    n = 0;
    if ((*th_data).nr_deskryptora1 == all_rooms[(*th_data).pokoj][0]){
        char buf[5] = {'b', 'l', 'a', 'c', 'k'};
        while(n < 5){
            send_int = write((*th_data).nr_deskryptora1, &buf[n], strlen(buf)-n);
            if(send_int == -1){
                write((*th_data).nr_deskryptora1, ".", 1); 
                printf("server has an unexpected problem\n"); 
                pthread_exit(NULL);
            }
            n += send_int;
        }
        (*th_data).color = "black";
    }
    else if ((*th_data).nr_deskryptora1 == all_rooms[(*th_data).pokoj][1]){
        char buf[5] = {'w', 'h', 'i', 't', 'e'};
        while(n < 5){        
            send_int = write((*th_data).nr_deskryptora1, &buf[n], strlen(buf)-n);
            if(send_int == -1){
                write((*th_data).nr_deskryptora1, ".", 1); 
                printf("server has an unexpected problem\n"); 
                pthread_exit(NULL);
            }            
            n += send_int;
        }
        (*th_data).color = "white";        
    }
    
    
    // glowna petla odbierania i wysylania
    while(1){

        n = 0;
        read_int = read((*th_data).nr_deskryptora1, &(*th_data).data[n], 6*sizeof(char));
        n += read_int;
        // jeśli uda się odczytac wszytsko na raz to nie wchodzimy w pętle;
        while(n != 6){
            if(read_int == 0){
                n=0;
                char buf[6] = {'o', 'v', ';', 'e', 'r', '\n'};                  // koniec gry
                while(n < 6){        
                    send_int = write((*th_data).nr_deskryptora2, &buf[n], strlen(buf)-n);
                    n += send_int;
                    if(send_int == -1){
                        write((*th_data).nr_deskryptora2, ".", 1); 
                        printf("server has an unexpected problem\n"); 
                        pthread_exit(NULL);
                    }                    
                }
                run = false;
            }
            else if(read_int == -1){
                char buf[6] = {'e', 'r', ';', '0', '2', '\n'};                  // problem serwera w odczytnaiu danych
                n=0;
                while(n < 6){        
                    send_int = write((*th_data).nr_deskryptora1, &buf[n], strlen(buf)-n);
                    n += send_int;
                    if(send_int == -1){
                        write((*th_data).nr_deskryptora1, ".", 1); 
                        printf("server has an unexpected problem\n"); 
                        pthread_exit(NULL);
                    }
                }      
                n=0;
                while(n < 6){        
                    send_int = write((*th_data).nr_deskryptora2, &buf[n], strlen(buf)-n);
                    n += send_int;
                    if(send_int == -1){
                        write((*th_data).nr_deskryptora2, ".", 1); 
                        printf("server has an unexpected problem\n"); 
                        pthread_exit(NULL);
                    }                    
                }
                run = false;                                                
            }
            read_int = read((*th_data).nr_deskryptora1, &(*th_data).data[n], 6*sizeof(char));
            n += read_int;
        }
        if(run == false){break;}

        // sprawdzanie ruchu i wysyłanie wiadomosci do obydwóch klientów
        check_move_return = check_move(all_boards[(*th_data).pokoj], (*th_data).color, (*th_data).data);
        if( check_move_return == 1){
            n = 0;
            while(n < 6){        
                send_int = write((*th_data).nr_deskryptora1, &(*th_data).data[n], strlen((*th_data).data)-n);
                n += send_int;
                if(send_int == -1){
                    write((*th_data).nr_deskryptora1, ".", 1); 
                    printf("server has an unexpected problem\n"); 
                    pthread_exit(NULL);
                }                
            }         
            n = 0;
            while(n < 6){        
                send_int = write((*th_data).nr_deskryptora2, &(*th_data).data[n], strlen((*th_data).data)-n);
                n += send_int;
                if(send_int == -1){
                    write((*th_data).nr_deskryptora2, ".", 1); 
                    printf("server has an unexpected problem\n"); 
                    pthread_exit(NULL);
                }                
            }             
        }              
        else{
            n=0;
            char buf[6] = {'e', 'r', ';', '0', '0', '\n'};                  // zły ruch
            while(n < 6){        
                send_int = write((*th_data).nr_deskryptora1, &buf[n], strlen(buf)-n);
                n += send_int;
                if(send_int == -1){
                    write((*th_data).nr_deskryptora1, ".", 1); 
                    printf("server has an unexpected problem\n"); 
                    pthread_exit(NULL);
                }                
            }          
        }
        
        // sprawdzanie resulatu i ewentualne wysyłanie informacji o wygranym
        result = find_result(all_boards[(*th_data).pokoj]);
        if(result[0] == 0){
            char buf[6] = {'w', 'h', ';', 'b', 'l', '\n'};                  // biały wygrał, czarny przegrał
            n = 0;
            while(n < 6){        
                send_int = write((*th_data).nr_deskryptora1, &buf[n], strlen(buf)-n);
                n += send_int;
                if(send_int == -1){
                    write((*th_data).nr_deskryptora1, ".", 1); 
                    printf("server has an unexpected problem\n"); 
                    pthread_exit(NULL);
                }                
            }         
            n = 0;
            while(n < 6){        
                send_int = write((*th_data).nr_deskryptora2, &buf[n], strlen(buf)-n);
                n += send_int;
                if(send_int == -1){
                    write((*th_data).nr_deskryptora2, ".", 1); 
                    printf("server has an unexpected problem\n"); 
                    pthread_exit(NULL);
                }                
            }
            break;        
        }
        else if(result[1] == 0){
            char buf[6] = {'b', 'l', ';', 'w', 'h', '\n'};                  // czarny wygrał, biały przegrał
            n = 0;
            while(n < 6){        
                send_int = write((*th_data).nr_deskryptora1, &buf[n], strlen(buf)-n);
                n += send_int;
                if(send_int == -1){
                    write((*th_data).nr_deskryptora1, ".", 1); 
                    printf("server has an unexpected problem\n"); 
                    pthread_exit(NULL);
                }                       
            }         
            n = 0;
            while(n < 6){        
                send_int = write((*th_data).nr_deskryptora2, &buf[n], strlen(buf)-n);
                n += send_int;
                if(send_int == -1){
                    write((*th_data).nr_deskryptora2, ".", 1); 
                    printf("server has an unexpected problem\n"); 
                    pthread_exit(NULL);
                }                       
            }  
            break;        
        }
    }

    pthread_exit(NULL);
}


//funkcja obsługująca połączenie z nowym klientem
void handleConnection(int connection_socket_descriptor, int id, int gdzie) {
    int identyfikator = id;
    int room = gdzie;
    pthread_mutex_unlock(&players);

    t_data[identyfikator].nr_deskryptora1 = connection_socket_descriptor;
    t_data[identyfikator].pokoj = room;
    t_data[identyfikator].numer = identyfikator;

    pthread_create(&thread[identyfikator], NULL, ThreadBehavior, (void *)&t_data[identyfikator]);
}


int main(int argc, char* argv[]) {
    int server_socket_descriptor;
    int connection_socket_descriptor;
    int bind_result;
    int listen_result;
    char reuse_addr_val = 1;
    struct sockaddr_in server_address;
    int i;
    int id = 0;

    //inicjalizacja gniazda serwera
    memset(&server_address, 0, sizeof(struct sockaddr));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(SERVER_PORT);

    // tworzenie socketu serwera
    server_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_descriptor < 0)
    {
        fprintf(stderr, "%s: Błąd przy próbie utworzenia gniazda..\n", argv[0]);
        exit(1);
    }
    setsockopt(server_socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse_addr_val, sizeof(reuse_addr_val));

    bind_result = bind(server_socket_descriptor, (struct sockaddr*)&server_address, sizeof(struct sockaddr));
    if (bind_result < 0)
    {
        fprintf(stderr, "%s: Błąd przy próbie dowiązania adresu IP i numeru portu do gniazda.\n", argv[0]);
        exit(1);
    }

    listen_result = listen(server_socket_descriptor, QUEUE_SIZE);
    if (listen_result < 0) {
        fprintf(stderr, "%s: Błąd przy próbie ustawienia wielkości kolejki.\n", argv[0]);
        exit(1);
    }
        
    // wszytskie pokoje sa puste, inicjalizacja mutexow i zmiennych war    
    int k;    
    for(k=0; k<GAME_SIZE; k++){                                
        all_rooms[k][0] = 0;
        all_rooms[k][1] = 0;
        pthread_mutex_init(&room_mut[k], NULL);
        pthread_cond_init(&wait_for_another_player[k], NULL);
    }

    while(1){
        connection_socket_descriptor = accept(server_socket_descriptor, NULL, NULL);
        if (connection_socket_descriptor < 0)
        {
            fprintf(stderr, "%s: Błąd przy próbie utworzenia gniazda dla połączenia.\n", argv[0]);
            exit(1);
        }

        // szukamy pierwszego wolnego pokoju i wpisujemy nasz deskryptor
        pthread_mutex_lock(&players);
        for(i=0; i<16; i++){                            
            if ((all_rooms[i][0] * all_rooms[i][1]) == 0){
                break;
            }
        }
        if (all_rooms[i][0] == 0){
            all_rooms[i][0] = connection_socket_descriptor;
        }
        else{
            all_rooms[i][1] = connection_socket_descriptor;   
        }

        int identy = id;
        id = id + 1;
        handleConnection(connection_socket_descriptor, identy, i);
    }

    close(server_socket_descriptor);
    return(0);
}
