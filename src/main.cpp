#include <unistd.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h> 
#include <vector>
#include <algorithm>
using namespace std;

#define WIDTH       60
#define HEIGHT      30
#define INFO_WIDTH  60
#define INFO_HEIGHT 6
#define GAME_WIDTH  43
#define GAME_HEIGHT 23
#define CHAT_WIDTH  (INFO_WIDTH - GAME_WIDTH)
#define CHAT_HEIGHT GAME_HEIGHT
#define TIME_TICKS_MS 10

#define MONSTER0_SPEED 1000
#define MONSTER1_SPEED 800
#define MONSTER2_SPEED 600
#define MONSTER3_SPEED 400
#define MONSTER4_SPEED 200
#define MONSTER5_SPEED 100

#define msleep(ms) usleep(ms * 1000)

enum
{
    START_GAME,
    JOIN_GAME,
    ABOUT,
    EXIT,
};

enum
{
    EMPTY,          // ' '                    
    STONE,          // '@' stone
    BOMB_SMALL,     // 'o' bomb
    BOMB_BIG,       // 'O' 
    MONSTER0,       // '0' monster
    MONSTER1,       // '1' monster
    MONSTER2,       // '2' monster
    MONSTER3,       // '3' monster
    MONSTER4,       // '4' monster
    MONSTER5,       // '5' monster
    PLAYER0,        // 'a' 
    PLAYER1,        // 'b'
    PLAYER2,        // 'c'
    PLAYER3,        // 'd'
    DOOR,           // '$' door to next level
    POWERUP,        // 'P' power up
    LIFEUP,         // 'L' life up
    BOMBUP,         // 'B' bomb up
    SUPERMAN,       // 'S' superman
    TIMER,          // 'T' timer
    BLOCK,          // '#' block with nothing
    BLOCK_DOOR,     // '$' door 
    BLOCK_POWERUP,  // '#' block(power up)
    BLOCK_LIFEUP,   // '#' block(life up)
    BLOCK_BOMBUP,   // '#' block(bomb up)
    BLOCK_SUPERMAN,  // '#' block(superman)
    BLOCK_TIMER,    // '#' block(timer)
    BOMBING0,       // 'X' bombing fire shape
    BOMBING1,       // 'x' bombing fire shape
};

struct Elem
{
    Elem(int px, int py, int ic):
        x(px),y(py),icon(ic){};
    int x,y;
    int icon;
};

struct Bomb
{
    Bomb(int px, int py, int bbtime, int ic, int pw):
        x(px),y(py),bombtime(bbtime),timetobomb(bbtime),changetime(100),timetochange(100),
        bombingtime(200),timetobombingfinish(200),bombingicon(BOMBING0),
        icon(ic),power(pw),isfirstbombing(true){};
    int x,y;
    int bombtime;
    int timetobomb;  
    int changetime;
    int timetochange;
    int bombingtime;
    int timetobombingfinish;
    int bombingicon;
    int icon;
    int power;
    bool isfirstbombing;
    vector<Elem> dieds;
};

struct Player
{
    Player(int px, int py, int ic):
        x(px),y(py),icon(ic),life(1),bombnum(10),bombtime(3000),bombpower(3),timer(false),superman(false){};
    int x,y;
    int icon;
    int life;
    size_t bombnum;
    int bombtime;
    int bombpower;
    bool timer;
    bool superman;
    vector<Bomb> bomb;
};

struct Monster
{
    Monster(int px, int py, int ic, int at):
        x(px),y(py),icon(ic),actiontime(at),timetoaction(at){};
    int x,y;
    int icon;
    const int actiontime;
    int timetoaction;
//    bool isdead;
//    int deadtime;
};

struct Monster0: public Monster
{
    Monster0(int px, int py):
        Monster(px,py,MONSTER0,MONSTER0_SPEED){}
};
struct Monster1: public Monster
{
    Monster1(int px, int py):
        Monster(px,py,MONSTER1,MONSTER1_SPEED){}
};
struct Monster2: public Monster
{
    Monster2(int px, int py):
        Monster(px,py,MONSTER2,600){}
};
struct Monster3: public Monster
{
    Monster3(int px, int py):
        Monster(px,py,MONSTER3,500){}
};
struct Monster4: public Monster
{
    Monster4(int px, int py):
        Monster(px,py,MONSTER4,400){}
};
struct Monster5: public Monster
{
    Monster5(int px, int py):
        Monster(px,py,MONSTER5,300){}
};


void generate_data(int w, int h, int nblock, int nMonster0,int nMonster1,int nMonster2,int nMonster3,int nMonster4, int nMonster5,
        int npowerup, int nlifeup, int nbombup, int nSUPERMAN, int ntimer, int ndoor,
        vector<vector<int>>& outMapArry, vector<Monster*>& outMonsters, Player& player)
{
    int x, y;
    outMapArry.resize(h,vector<int>(w,EMPTY));
    outMapArry[player.y][player.x] = player.icon;
    for (y = 0; y < h; ++y)
    {
        for (x = 0; x < w; ++x)
        {
            if (x%2&&y%2)
                outMapArry[y][x]=STONE;
        }
    }
    srand(time(0));
    while (nblock > 0)
    {
        y = rand() % h;
        x = rand() % w;
        /*
         * block, fake door, power up, life up, bomb up, superman, timer
         */
        if (outMapArry[y][x] != EMPTY) continue;
        if (ndoor > 0 && ndoor--)
            outMapArry[y][x] = BLOCK_DOOR;
        else if (npowerup > 0 && npowerup--)
            outMapArry[y][x] = BLOCK_POWERUP;
        else if (nlifeup > 0 && nlifeup--)
            outMapArry[y][x] = BLOCK_LIFEUP;
        else if (nbombup > 0 && nbombup--)
            outMapArry[y][x] = BLOCK_BOMBUP;
        else if (nSUPERMAN > 0 && nSUPERMAN--)
            outMapArry[y][x] = BLOCK_SUPERMAN;
        else if (ntimer > 0 && ntimer--)
            outMapArry[y][x] = BLOCK_TIMER;
        else if (nblock > 0 && nblock--)
            outMapArry[y][x] = BLOCK;
        else
        {
            perror("error!");
            exit(0);
        }
    }
    while (nMonster5 > 0)
    {
        x = rand() % w;
        y = rand() % h;
        if ((outMapArry[y][x]) != EMPTY) continue;
        /*
         * keep track of monster's move
         */
        if (nMonster0 > 0 && nMonster0--)
            outMonsters.push_back(new struct Monster0(x,y));
        else if (nMonster1 > 0 && nMonster1--)
            outMonsters.push_back(new struct Monster1(x, y));
        else if (nMonster2 > 0 && nMonster2--)
            outMonsters.push_back(new struct Monster2(x, y));
        else if (nMonster3 > 0 && nMonster3--)
            outMonsters.push_back(new struct Monster3(x, y));
        else if (nMonster4 > 0 && nMonster4--)
            outMonsters.push_back(new struct Monster4(x, y));
        else if (nMonster5 > 0 && nMonster5--)
            outMonsters.push_back(new struct Monster5(x, y));
        else
        {
            perror("error!");
            exit(0);
        }
        outMapArry[y][x] = outMonsters.back()->icon;
    }
}


void refresh_info_win(WINDOW *ptrInfoWin)
{
    touchwin(ptrInfoWin);
    wrefresh(ptrInfoWin);
}

void refresh_game_win(WINDOW *ptrGameWin, vector<vector<int>>& mapArry, vector<Monster*>& monsters, Player& player, int passedms)
{
    /*
     * bombing action
     */
    int dx[] = {-1,1,0,0};
    int dy[] = {0,0,-1,1};
    int x, y, h, w, ch;
    h = mapArry.size();
    w = mapArry[0].size();
    for (auto it = player.bomb.begin(); it != player.bomb.end(); )
    {
        if (it->timetobomb > 0)
        {
            /*
             * not boming
             */
            it->timetochange -= passedms;
            it->timetobomb -= passedms;
            if (it->timetochange < 0)
            {
                if (it->icon == BOMB_SMALL)
                    it->icon = BOMB_BIG;
                else
                    it->icon = BOMB_SMALL;
                it->timetochange = it->changetime;
                mapArry[it->y][it->x] = it->icon;
            }
        }
        else
        {
            /*
             * start bombing
             */
            it->timetobombingfinish -= passedms;
            if (it->timetobombingfinish < 0) // bombing finished
            {
                for (auto& deadelem : it->dieds)
                {
                    switch (deadelem.icon)
                    {
                        case EMPTY:          // ' '                    
                        case STONE:          // '@' stone
                        case BOMB_SMALL:     // 'o' bomb
                        case BOMB_BIG:       // 'O' 
                            mapArry[deadelem.y][deadelem.x] = deadelem.icon;
                            break;
                        case MONSTER0:       // '0' monster
                        case MONSTER1:       // '1' monster
                        case MONSTER2:       // '2' monster
                        case MONSTER3:       // '3' monster
                        case MONSTER4:       // '4' monster
                        case MONSTER5:       // '5' monster
                            mapArry[deadelem.y][deadelem.x] = EMPTY;
                            break;
                        case PLAYER0:        // 'a' 
                        case PLAYER1:        // 'b'
                        case PLAYER2:        // 'c'
                        case PLAYER3:        // 'd'
                            // TODO:  player died
                            break;
                        case DOOR:           // '$' door to next level
                            // TODO: spawn monsters
                            break;
                        case POWERUP:        // 'P' power up
                        case LIFEUP:         // 'L' life up
                        case BOMBUP:         // 'B' bomb up
                        case SUPERMAN:        // 'S' superman
                        case TIMER:          // 'T' timer
                            mapArry[deadelem.y][deadelem.x] = deadelem.icon;
                            break;
                        case BLOCK:          // '#' block with nothing
                            mapArry[deadelem.y][deadelem.x] = EMPTY;
                            break;
                        case BLOCK_DOOR:     // '$' door 
                            mapArry[deadelem.y][deadelem.x]=DOOR;
                            break;
                        case BLOCK_POWERUP:  // '#' block(power up)
                            mapArry[deadelem.y][deadelem.x]=POWERUP;
                            break;
                        case BLOCK_LIFEUP:   // '#' block(life up)
                            mapArry[deadelem.y][deadelem.x]=LIFEUP;
                            break;
                        case BLOCK_BOMBUP:   // '#' block(bomb up)
                            mapArry[deadelem.y][deadelem.x]=BOMBUP;
                            break;
                        case BLOCK_SUPERMAN:  // '#' block(superman)
                            mapArry[deadelem.y][deadelem.x]=SUPERMAN;
                            break;
                        case BLOCK_TIMER:    // '#' block(timer)
                            mapArry[deadelem.y][deadelem.x]=TIMER;
                            break;
                        case BOMBING0:       // 'X' bombing fire shape
                        case BOMBING1:       // 'x' bombing fire shape
                            mapArry[deadelem.y][deadelem.x] = deadelem.icon;
                            break;
                        default:
                            printf("error: %d", __LINE__);
                            exit(0);
                            break;
                    }
                }
                it=player.bomb.erase(it);
                continue;
            }
            else // during the bombing
            {
                if (it->timetobombingfinish < 100 && it->bombingicon != BOMBING1) // change bombing shape
                {
                    it->bombingicon = BOMBING1;
                    for (auto& deadelem : it->dieds)
                        mapArry[deadelem.y][deadelem.x] = it->bombingicon;
                }

                if (it->isfirstbombing)
                {
                    it->isfirstbombing=false;
                    it->dieds.push_back(Elem(it->x,it->y,EMPTY));
                    mapArry[it->y][it->x] = it->bombingicon;
                    for (int i=0;i<4;++i)
                    {
                        bool flag=false;    // has bombed in this direction
                        for (int k=1;k<=it->power;++k)
                        {
                            int nx=it->x+k*dx[i],ny=it->y+k*dy[i];
                            if (nx<0||nx>=w||ny<0||ny>=h) break;
                            switch (mapArry[ny][nx])
                            {
                                case EMPTY:          // ' '                    
                                    it->dieds.push_back(Elem(nx,ny,mapArry[ny][nx]));
                                    mapArry[ny][nx] = it->bombingicon;
                                    break;
                                case STONE:          // '@' stone
                                    flag=true;
                                    break;
                                case BOMB_SMALL:     // 'o' bomb
                                case BOMB_BIG:       // 'O' 
                                    flag=true;
                                    break;
                                case MONSTER0:       // '0' monster
                                case MONSTER1:       // '1' monster
                                case MONSTER2:       // '2' monster
                                case MONSTER3:       // '3' monster
                                case MONSTER4:       // '4' monster
                                case MONSTER5:       // '5' monster
                                    monsters.erase(find_if(monsters.begin(),monsters.end(),[&](Monster* ptrMon){
                                                    if (ptrMon->x==nx&&ptrMon->y==ny)
                                                        return true;
                                                    return false;
                                                }));
                                    it->dieds.push_back(Elem(nx,ny,mapArry[ny][nx]));
                                    mapArry[ny][nx] = it->bombingicon;
                                    break;
                                case PLAYER0:        // 'a' 
                                case PLAYER1:        // 'b'
                                case PLAYER2:        // 'c'
                                case PLAYER3:        // 'd'
                                    // TODO:  player died
                                    break;
                                case DOOR:           // '$' door to next level
                                    // TODO: spawn monsters
                                    flag=true;
                                    break;
                                case POWERUP:        // 'P' power up
                                case LIFEUP:         // 'L' life up
                                case BOMBUP:         // 'B' bomb up
                                case SUPERMAN:        // 'S' superman
                                case TIMER:          // 'T' timer
                                    flag=true;
                                    break;
                                case BLOCK:          // '#' block with nothing
                                case BLOCK_DOOR:     // '$' door 
                                case BLOCK_POWERUP:  // '#' block(power up)
                                case BLOCK_LIFEUP:   // '#' block(life up)
                                case BLOCK_BOMBUP:   // '#' block(bomb up)
                                case BLOCK_SUPERMAN:  // '#' block(superman)
                                case BLOCK_TIMER:    // '#' block(timer)
                                    it->dieds.push_back(Elem(nx,ny,mapArry[ny][nx]));
                                    mapArry[ny][nx] = it->bombingicon;
                                    flag=true;
                                    break;
                                case BOMBING0:       // 'X' bombing fire shape
                                case BOMBING1:       // 'x' bombing fire shape
                                    it->dieds.push_back(Elem(nx,ny,mapArry[ny][nx]));
                                    mapArry[ny][nx] = it->bombingicon;
                                    break;
                                default:
                                    perror("error");
                                    exit(0);
                                    break;
                            }
                            if (flag) break;
                        }
                    }

                }
            }
        }
        it++;
    }

    /*
     * monster move
     */
    for (auto it = monsters.begin(); it != monsters.end(); )
    {
        auto ptrMon = *it;
        ptrMon->timetoaction -= passedms;
        if (ptrMon->timetoaction < 0)
        {
            ptrMon->timetoaction = ptrMon->actiontime;
            mapArry[ptrMon->y][ptrMon->x] = EMPTY;
            int nx = ptrMon->x, ny = ptrMon->y;
            for (;;)
            {
                int idx = rand() % 4;
                nx = ptrMon->x + dx[idx];
                ny = ptrMon->y + dy[idx];
                if (nx >= 0 && nx < w && ny >= 0 && ny < h)
                    break;
            }
            switch (mapArry[ny][nx])
            {
                case EMPTY:
                    ptrMon->x = nx, ptrMon->y = ny;
                    mapArry[ptrMon->y][ptrMon->x] = ptrMon->icon;
                    break;
                case PLAYER0:
                case PLAYER1:
                case PLAYER2:
                case PLAYER3:
                    mapArry[ptrMon->y][ptrMon->x] = ptrMon->icon;
                    break;
                case BOMBING0:
                case BOMBING1:
                    it=monsters.erase(it);
                    delete ptrMon;
                    continue;
                    break;
                default:
                    mapArry[ptrMon->y][ptrMon->x] = ptrMon->icon;
                    break;
            }
        }
        it++;
    }


    /*
     * map
     */
    for (y = 0; y < h; ++y)
    {
        for (x = 0; x < w; ++x)
        {
            switch (mapArry[y][x])
            {
                case EMPTY:         ch = ' '; break;// ' '                    
                case STONE:         ch = '@'; break;// '@' stone
                case BOMB_SMALL:    ch = 'o'; break;// 'o' bomb
                case BOMB_BIG:      ch = 'O'; break;// 'O' bomb
                case MONSTER0:      ch = '0'; break;// 'M' monster
                case MONSTER1:      ch = '1'; break;// 'M' monster
                case MONSTER2:      ch = '2'; break;// 'M' monster
                case MONSTER3:      ch = '3'; break;// 'M' monster
                case MONSTER4:      ch = '4'; break;// 'M' monster
                case MONSTER5:      ch = '5'; break;// 'M' monster
                case PLAYER0:       ch = 'a'; break;// 'a' me
                case PLAYER1:       ch = 'b'; break;// 'b' me
                case PLAYER2:       ch = 'c'; break;// 'c' me
                case PLAYER3:       ch = 'd'; break;// 'd' me
                case DOOR:          ch = '$'; break;// '$' door to next level
                case POWERUP:       ch = 'P'; break;// 'P' power up
                case LIFEUP:        ch = 'L'; break;// 'L' life up
                case BOMBUP:        ch = 'B'; break;// 'B' bomb up
                case SUPERMAN:       ch = 'S'; break;// 'S' superman
                case TIMER:         ch = 'T'; break;// 'T' timer
                case BLOCK:         // '#' block with nothing
                case BLOCK_DOOR:    // '$' door 
                case BLOCK_POWERUP: // '#' block(power up)
                case BLOCK_LIFEUP:  // '#' block(life up)
                case BLOCK_BOMBUP:  // '#' block(bomb up)
                case BLOCK_SUPERMAN: // '#' block(superman)
                case BLOCK_TIMER:   ch = '#'; break;// '#' block(timer)
                case BOMBING0:      ch = 'X'; break;       // 'X' bombing fire shape
                case BOMBING1:      ch = 'x'; break;       // 'x' bombing fire shape
                default:
                                    perror("erro!");
                                    exit(0);
                                    break;
            }
            mvwaddch(ptrGameWin, y+1, x+1, ch);
        }
    }
    touchwin(ptrGameWin);
    wrefresh(ptrGameWin);
}



void refresh_chat_win(WINDOW *ptrChatWin)
{
    touchwin(ptrChatWin);
    wrefresh(ptrChatWin);
}

void refresh_welcome_win(WINDOW* ptrWelcomeWin, int choose)
{
    char buf[COLS];
    int len;
    int x,y;
    int w,h;
    getmaxyx(ptrWelcomeWin,h,w);
    int attr = A_BOLD;

    len = snprintf(buf,sizeof(buf),"Start Game");
    x = (w-len)/2;
    y = h/5;
    wattroff(ptrWelcomeWin,attr);
    if (choose == START_GAME) wattrset(ptrWelcomeWin,attr);
    if (mvwprintw(ptrWelcomeWin,y,x,"%s",buf)==ERR)
    {
        perror("error");exit(0);
    }

    len = snprintf(buf,sizeof(buf),"Join Game");
    x = (w-len)/2;
    y += h/5;
    wattroff(ptrWelcomeWin,attr);
    if (choose == JOIN_GAME) wattrset(ptrWelcomeWin,attr);
    if (mvwprintw(ptrWelcomeWin,y,x,"%s",buf)==ERR)
    {
        perror("error");exit(0);
    }

    len = snprintf(buf,sizeof(buf),"About");
    x = (w-len)/2;
    y += h/5;
    wattroff(ptrWelcomeWin,attr);
    if (choose == ABOUT) wattrset(ptrWelcomeWin,attr);
    if (mvwprintw(ptrWelcomeWin,y,x,"%s",buf)==ERR)
    {
        perror("error");exit(0);
    }
    
    len = snprintf(buf,sizeof(buf),"Exit");
    x = (w-len)/2;
    y += h/5;
    wattroff(ptrWelcomeWin,attr);
    if (choose == EXIT) wattrset(ptrWelcomeWin,attr);
    if (mvwprintw(ptrWelcomeWin,y,x,"%s",buf)==ERR)
    {
        perror("error");exit(0);
    }

    touchwin(ptrWelcomeWin);
    wrefresh(ptrWelcomeWin);        
}
void start_game()
{
    int ch;
    WINDOW *ptrInfoWin = newwin(INFO_HEIGHT,INFO_WIDTH,0,0);
    box(ptrInfoWin, ACS_VLINE, ACS_HLINE);
    WINDOW *ptrGameWin = newwin(GAME_HEIGHT, GAME_WIDTH, INFO_HEIGHT, 0);
    box(ptrGameWin, ACS_VLINE, ACS_HLINE);
    WINDOW *ptrChatWin = newwin(CHAT_HEIGHT, CHAT_WIDTH, INFO_HEIGHT, GAME_WIDTH);
    box(ptrChatWin, ACS_VLINE, ACS_HLINE);
    vector<Monster*> monsters;
    vector<vector<int>> mapArry;
    Player player(0,0,PLAYER0);
    generate_data(GAME_WIDTH-2, GAME_HEIGHT-2, 200, 
            3,3,3,3,3,3, 1, 3, 1, 1, 1, 1, mapArry, monsters,player);
    for(;;)
    {
        if ((ch = tolower(getch())) != ERR)
        {
            if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')
            {
                int h = mapArry.size();
                int w = mapArry[0].size();
                mapArry[player.y][player.x] = EMPTY;
                int nx=player.x,ny=player.y;
                switch (tolower(ch))
                {
                    case KEY_UP:    
                        ny--; 
                        break;
                    case KEY_DOWN:  
                        ny++; 
                        break;
                    case KEY_LEFT:  
                        nx--; 
                        break;
                    case KEY_RIGHT: 
                        nx++; 
                        break;
                    case ' ':
                        if (player.bombnum > player.bomb.size())
                            player.bomb.push_back(Bomb(nx,ny,player.bombtime, BOMB_SMALL, player.bombpower));
                        break;
                    default:
                        break;
                }
                if (nx>=0&&nx<w&&ny>=0&&ny<h)
                {
                    switch (mapArry[ny][nx])
                    {
                        case EMPTY:          // ' '                    
                            player.x = nx, player.y = ny;
                            break;
                        case STONE:          // '@' stone
                        case BOMB_SMALL:     // 'o' bomb
                        case BOMB_BIG:       // 'O' 
                            break;
                        case MONSTER0:       // '0' monster
                        case MONSTER1:       // '1' monster
                        case MONSTER2:       // '2' monster
                        case MONSTER3:       // '3' monster
                        case MONSTER4:       // '4' monster
                        case MONSTER5:       // '5' monster
                            // TODO: died
                            break;
                        case PLAYER0:        // 'a' 
                        case PLAYER1:        // 'b'
                        case PLAYER2:        // 'c'
                        case PLAYER3:        // 'd'
                            // TODO: multiplayer
                            break;
                        case DOOR:           // '$' door to next level
                            // TODO: next level
                            player.x = nx, player.y = ny;
                            break;
                        case POWERUP:        // 'P' power up
                            player.x = nx, player.y = ny;
                            player.bombpower++;
                            break;
                        case LIFEUP:         // 'L' life up
                            player.x = nx, player.y = ny;
                            player.life++;
                            break;
                        case BOMBUP:         // 'B' bomb up
                            player.x = nx, player.y = ny;
                            player.bombnum++;
                            break;
                        case SUPERMAN:        // 'S' superman
                            player.x = nx, player.y = ny;
                            player.superman=true;
                            break;
                        case TIMER:          // 'T' timer
                            player.x = nx, player.y = ny;
                            player.timer=true;
                            break;
                        case BLOCK:          // '#' block with nothing
                        case BLOCK_DOOR:     // '$' door 
                        case BLOCK_POWERUP:  // '#' block(power up)
                        case BLOCK_LIFEUP:   // '#' block(life up)
                        case BLOCK_BOMBUP:   // '#' block(bomb up)
                        case BLOCK_SUPERMAN:  // '#' block(superman)
                        case BLOCK_TIMER:    // '#' block(timer)
                            break;
                        case BOMBING0:       // 'X' bombing fire shape
                        case BOMBING1:       // 'x' bombing fire shape
                            // TODO: died
                            break;
                        default:
                            break;
                    }
                }
                mapArry[player.y][player.x] = player.icon;
            }
            else
            {
                switch (tolower(ch))
                {
                    case 'q':
                        goto exit;
                        break;
                    default:
                        break;
                } 
            }
            
        }
        refresh_info_win(ptrInfoWin);
        refresh_game_win(ptrGameWin, mapArry, monsters, player, TIME_TICKS_MS);
        refresh_chat_win(ptrChatWin);
        msleep(TIME_TICKS_MS);
    }
exit:
    for (auto& ptrMon : monsters)
        delete ptrMon;
    delwin(ptrInfoWin);
    delwin(ptrGameWin);
    delwin(ptrChatWin);
}

void join_game()
{
    /*
     * for multiplayer
     */ 
}

void about_game()
{
    /*
     * game introduction
     */
}

void exit_game()
{
    /*
     * exit game
     */
    clear();
    refresh();
    exit(0);
}

void game_loop()
{
    int ch;
    WINDOW *ptrWelcomeWin = newwin(HEIGHT,WIDTH,0,0);
    box(ptrWelcomeWin,ACS_VLINE,ACS_HLINE);
    int choose = START_GAME;
    for(;;)
    {
        if ((ch = getch()) != ERR)
        {
            switch (tolower(ch))
            {
                case KEY_UP:
                    if (choose>START_GAME) choose--;
                    break;
                case KEY_DOWN:
                    if (choose<EXIT) choose++;
                    break;
                case '\n':
                    switch (choose)
                    {
                        case START_GAME:
                            start_game();
                            break;
                        case JOIN_GAME:
                            join_game();
                            break;
                        case ABOUT:
                            about_game();
                            break;
                        case EXIT:
                            exit_game();
                            break;
                        default:
                            break;
                    }
                    break;
                case 'q':
                    exit_game();
                    break;
                default:
                    break;
            }
        }
        refresh_welcome_win(ptrWelcomeWin, choose);
        msleep(TIME_TICKS_MS);
    }
}

int main(int argc, char** argv)
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr,TRUE);
    nodelay(stdscr,true);
    curs_set(0);
    if (!has_colors())
    {
        exit(1);    
    }
    if (start_color()!=OK)
    {
        exit(1);
    }
    game_loop();
    endwin();
    exit(0);
}
