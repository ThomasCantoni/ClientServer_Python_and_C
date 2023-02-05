#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <stdio.h>
#include <float.h>
#include "byte_converter.h"



#define SDL_MAIN_HANDLED
//#include <SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "bomberman.h"

#define FLT_EXPO_SIZE 5
#define MSG_SIZE 50
#define NETWORK_PERIOD 0.2f

WSADATA wsa;
SOCKET Server_socket,self_socket;
struct sockaddr_in Server_addr;
unsigned char* message, server_reply[2000];
int recv_size,running;
player_t player;
SDL_Window *window;
SDL_Renderer *renderer;
Uint64 currentTime, previousTime;
float delta_time,FPS,secondAccumulator=0;
fd_set read_fds;
int server_socket,ret;
int bind_error;
player_t* online_players[2];

SDL_Texture* Get_Texture(char* path)
{
    int width;
    int height;
    int channels;
    //"materials\\green_goblin.png"
    unsigned char *pixels = stbi_load(path, &width, &height, &channels, 4);
    if (!pixels)
    {
        SDL_Log("Unable to open image");
       
        return NULL;
    }

    SDL_Log("Image width: %d height: %d channels: %d", width, height, channels);
    SDL_Texture* tex;
    tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    if (!tex)    
    {
        SDL_Log("Unable to create texture: %s", SDL_GetError());
        free(pixels);
        
        return NULL;
    }

    SDL_UpdateTexture(tex, NULL, pixels, width * 4);
    SDL_SetTextureAlphaMod(tex, 255);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    free(pixels);
    return tex;
}
player_t* InstantiatePlayer(int ID)
{
    player_t* player = (player_t*)calloc(1,sizeof(player_t));
    player->position.x = 0;
    player->position.y = 0;
    player->number_of_lifes = 1;
    player->number_of_bombs = 1;
    player->score = 0;
    player->speed = 100;
    player->ID = ID;
    player->texture = Get_Texture("materials\\paperella.png");
    puts("GETTING TEX");


    return player;
}
void DestroyPlayer(player_t* to_destroy)
{
    SDL_free(to_destroy->texture);
    free(to_destroy);
}
void UpdatePlayer(player_t* player_to_update)
{
    //SDL_Rect player_rect = player_to_update->SDL_Rect;
    //vec2_t velocity = Vec2_towards(player_to_update->old_position,player_to_update->latest_position);
    //printf("\n TOWARDS:::: SPEED X %f  SPEED Y %f\n", velocity.x,velocity.y);

    //Vec2_Normalize(&velocity);
   // printf("\n NORMALIZE::: SPEED X %f  SPEED Y %f\n", velocity.x,velocity.y);
    //velocity = Vec2_multiply(velocity,player_to_update->speed*delta_time);
    player_to_update->lerp_accumulator += delta_time * (1/NETWORK_PERIOD);
    player_to_update->position = vector2_lerp(player_to_update->old_position,player_to_update->latest_position,player_to_update->lerp_accumulator);
    SDL_Rect player_rect = {player_to_update->position.x, player_to_update->position.y, 64, 64};
    SDL_RenderCopy(renderer, player_to_update->texture, NULL, &player_rect);
}
float message_to_float(char* buffer, size_t position)
{
    char buf[sizeof(float) + 1];
    size_t j = position;
    for (size_t i = 0; i < sizeof(buf); ++i)
    {
       buf[i] = buffer[j];
       ++j;
    }
    buf[sizeof(buf) - 1] = '\0';
    return strtof(buf, NULL);
}

static void bomberman_game_mode_init(game_mode_t *game_mode)
{
    game_mode->timer = 60;
}

static void bomberman_map_init(cell_t *map)
{
}

static void bomberman_player_init(player_t *player)
{
    player->position.x = 0;
    player->position.y = 0;
    player->number_of_lifes = 1;
    player->number_of_bombs = 1;
    player->score = 0;
    player->speed = 100;
   
}

int bomberman_graphics_init(SDL_Window **window, SDL_Renderer **renderer, SDL_Texture** tex)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
    {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return -1;
    }

    *window = SDL_CreateWindow("SDL is active!", 100, 100, 512, 512, 0);
    if (!*window)
    {
        SDL_Log("Unable to create window: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*renderer){
        SDL_Log("Unable to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(*window);
        SDL_Quit();
        return -1;
    }
    

    int width;
    int height;
    int channels;
    unsigned char *pixels = stbi_load("materials\\paperella.png", &width, &height, &channels, 4);
    if (!pixels)
    {
        SDL_Log("Unable to open image");
        SDL_DestroyRenderer(*renderer);
        SDL_DestroyWindow(*window);
        SDL_Quit();
        return -1;
    }

    SDL_Log("Image width: %d height: %d channels: %d", width, height, channels);
    //SDL_Texture* tex = player->texture;
    *tex = SDL_CreateTexture(*renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    if (!*tex)    
    {
        SDL_Log("Unable to create texture: %s", SDL_GetError());
        free(pixels);
        SDL_DestroyRenderer(*renderer);
        SDL_DestroyWindow(*window);
        SDL_Quit();
        return -1;
    }

    SDL_UpdateTexture(*tex, NULL, pixels, width * 4);
    SDL_SetTextureAlphaMod(*tex, 255);
    SDL_SetTextureBlendMode(*tex, SDL_BLENDMODE_BLEND);
    free(pixels);
    return 0;
}

void bomberman_init_network(char* ip, unsigned int port)
{
    
    // Initialize Winsock
    printf("\nInitializing Winsock...");
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
    {
        printf("Failed. Error Code : %d", WSAGetLastError());
        exit(-1);
    }
    printf("Initialized.\n");

    // Create a socket
    Server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    //unsigned long l;
    //ioctlsocket(Server_socket,FIONREAD,&l);
    if (Server_socket < 0)
    {
        printf("Could not create socket : %d", WSAGetLastError());
    }
    printf("Socket created.\n");
    
    // Prepare the sockaddr_in structure
    memset(&Server_addr,0,sizeof(Server_addr));
    Server_addr.sin_family = AF_INET;
    
    printf("\n%d\n",port);
    Server_addr.sin_port = htons((int)port);
    inet_pton (AF_INET, ip, &Server_addr.sin_addr);

    // Connect to server
    bind_error =  connect(Server_socket, (struct sockaddr *)&Server_addr, sizeof(struct sockaddr_in));
    if (bind_error != 0 )
    {
        printf("Connect failed with error code : %d", WSAGetLastError());
        exit(-1);
    }
    printf("Connected.\n");
    char data [] = {'0','0','0','0'};
    int sent_bytes = sendto(Server_socket,data, 4, 0,(struct sockaddr *)&Server_addr, sizeof(Server_addr));
    int server_size = sizeof(Server_addr);
    sent_bytes = recvfrom(Server_socket, data, 4, 0, (struct sockaddr *)&Server_addr , &server_size);
    int id = bytes_to_int(data);
    printf(" MY PLAYER-ID = %i\n",id);
    player.ID = id;
   
   message = calloc(1,50);
}
int send_to_server(unsigned char* data )
{
    int sent_bytes = sendto(Server_socket,(char*)data, 50, 0,(struct sockaddr *)&Server_addr, sizeof(Server_addr));
    if (sent_bytes < 0)
    {
        puts("Send failed");
        return 1;
    }
    return 0;
}
const Uint8* manage_input()
{
     SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = 0;
            }
        }
        SDL_PumpEvents();
        const Uint8* ret = SDL_GetKeyboardState(NULL);
        return ret;
}

void manage_network_operations()
{
    unsigned char* ID = calloc(1,sizeof(float));
    unsigned char* command = calloc(1,sizeof(float));

    unsigned char* buffer_x;
    unsigned char* buffer_y;
    
   
    secondAccumulator += delta_time;
    if(secondAccumulator > NETWORK_PERIOD)
    {
        //  return;

    
        //printf("%f",secondAccumulator);
        secondAccumulator =0;
    
        puts("\nSTARTING NETWORK SEND");
    
    // send position to server

   
   
    
    //printf("\n %i PLAYER ID",player.ID);
    int_to_bytes(player.ID,ID);
    
    //memcpy(message,ID,sizeof(int));
    //send_to_server(message);
    //printf("\n %f PLAYER X",player.position.x);
    //printf("\n %f PLAYER Y",player.position.y);
    buffer_x = float_to_bytes(player.position.x);
    buffer_y = float_to_bytes(player.position.y);

    

   //printf("\nX LENGTH:%llu  X IS:\n",sizeof(float));
   //for (int i = 0; i < sizeof(float); i++)
   //{
   //    printf("|%02x",buffer_x[i]);
   //   
   //}
   //printf("\nY LENGTH:%llu Y IS:\n",sizeof(float));
   //for (int i = 0; i <  sizeof(float); i++)
   //{
   //    printf("|%02x",buffer_y[i]);
   //   
   //}
   //
   //puts("\ncommand is");
   //for (int i = 0; i < sizeof(float); i++)
   //{
   //  printf("|%02x",command[i]);
   //}
    
    
    memcpy(message,ID,sizeof(int));
    bytes_append(message,50,4,command,  sizeof(int) );

    bytes_append(message,50,8,buffer_x, sizeof(float));
    bytes_append(message,50,12,buffer_y,sizeof(float));
  
    //dest_size = strlen(message)+ strlen(buffer_y) +1;
   // puts("\n MESSAGE IS:");
   // for (int i = 0; i < 50; i++)
   // {
   //   printf("|%02x",message[i]);
   // }

   
   
    

   
    send_to_server(message);
    }
    // get info from server
    unsigned long msg_length;
    int error = ioctlsocket(Server_socket,FIONREAD,&msg_length);
    if(msg_length == 0 || error != 0)
        return;

    int server_size = sizeof(Server_addr);
    int n = recvfrom(Server_socket, message, 50, 0, (struct sockaddr *)&Server_addr , &server_size);
    if(n == 0)
        return;
    printf("Received %d bytesfrom %s : %d : %.*s \n",n,inet_ntoa(Server_addr.sin_addr),ntohs(Server_addr.sin_port),n,message);
        for(int i = 0;i<n;++i)
        {
            printf("%x",message[i]);
        }
    
    int commandCode = bytes_to_int2(message,0);
    printf("\n RECEIVED COMMAND %i \n",commandCode);

    int player_id = bytes_to_int2(message,4);
    printf("\n RECEIVED PLAYERID %i \n",player_id);

    if(commandCode == 0)
    { // command 0 updates player

        float printX,printY;
        printX = bytes_to_float(message,8);
        printY = bytes_to_float(message,12);
       

        printf("\n PLAYER %i X IS %.2f\n",player_id,printX);
        printf("\n PLAYER %i Y IS %.2f\n",player_id,printY);

        online_players[player_id-1]->old_position.x= online_players[player_id-1]->latest_position.x;
        online_players[player_id-1]->old_position.y= online_players[player_id-1]->latest_position.y;
        online_players[player_id-1]->latest_position.x = printX;
        online_players[player_id-1]->latest_position.y = printY;
        online_players[player_id-1]->lerp_accumulator=0;

    }
    if(commandCode == 1)
    { // command 1 is new player
        puts("#################################################");
        puts("NEW PLAYER");
        puts("#################################################");
        printf("\n New Player ID = %i \n",player_id);
        player_t* newPlayer = InstantiatePlayer(player_id);
        online_players[newPlayer->ID-1] = newPlayer; 
        
    }
    if(commandCode == 2)
    {
        puts("#################################################");
        puts("PLAYER DIED");
        puts("#################################################");
        DestroyPlayer(online_players[player_id-1]);
        online_players[player_id] = NULL;
    }

  
}
float DeltaTimeUpdate()
{
   
   currentTime = SDL_GetPerformanceCounter();
   delta_time = (float)(currentTime - previousTime) / (float)SDL_GetPerformanceFrequency();

   previousTime = currentTime;
   float fps = (1.f / delta_time);
   FPS = fps;
   char* windowTitle = calloc(1,50);
   sprintf(windowTitle,"DeltaTime : %f  FPS: %f",delta_time,fps);
   SDL_SetWindowTitle(window,windowTitle);
   return fps;
}
int main(int argc, char **argv)
{
    char* server_ip = argv[1];
    char* test = argv[2] ;
    int port= atoi(test);
    
    puts(server_ip);
    printf("SHORT IS %hu\n",port);
   
    game_mode_t game_mode;
    cell_t map[64 * 64];
    

    bomberman_game_mode_init(&game_mode);
    bomberman_map_init(map);
    bomberman_player_init(&player);
    bomberman_init_network(server_ip,port);

    //online_players[player.ID] = &player;
    

    if (bomberman_graphics_init(&window, &renderer, &player.texture))
    {
        return -1;
    }

    running = 1;

    
    
    
    puts("INIT SUCCESFUL");
    // game loop
        
    while(running)
    {
       
        DeltaTimeUpdate();
        const Uint8 *keys = manage_input();
        manage_network_operations();
       
        

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        for (int i = 0; i < 2; i++)
        {
            player_t* current_player_to_update = online_players[i];
            if(current_player_to_update != NULL)
            {
                UpdatePlayer(current_player_to_update);
                //SDL_Rect target_rect = {current_player_to_update->position.x, current_player_to_update->position.y, 100, 100};
                //SDL_RenderCopy(renderer, current_player_to_update->texture, NULL, &target_rect);
                

            }

        }
    
       SDL_Rect target_rect = {player.position.x, player.position.y, 64, 64};
       SDL_RenderCopy(renderer, player.texture, NULL, &target_rect);
        player.position.x += (keys[SDL_SCANCODE_RIGHT]-keys[SDL_SCANCODE_LEFT])   * delta_time*player.speed;
       
        player.position.y += (keys[SDL_SCANCODE_DOWN]- keys[SDL_SCANCODE_UP])   *   delta_time *player.speed;
        

        
        SDL_RenderPresent(renderer);
        

        
    }

    // Add a NULL terminating character to make it a proper string before printing
    server_reply[recv_size] = '\0';
    //puts(server_reply);
    // Cleanup and exit
    closesocket(Server_socket);
    WSACleanup();

    return 0;
}