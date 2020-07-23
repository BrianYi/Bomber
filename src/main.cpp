#include <unistd.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h> 
#include <vector>
#include <algorithm>
using namespace std;

#define msleep(ms) usleep(ms * 1000)

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
    MONSTER_DEAD,   // 'x' monster dead
    PLAYER0,        // 'a' 
    PLAYER1,        // 'b'
    PLAYER2,        // 'c'
    PLAYER3,        // 'd'
    DOOR,           // '$' door to next level
    POWERUP,        // 'P' power up
    LIFEUP,         // 'L' life up
    BOMBUP,         // 'B' bomb up
    SPEEDUP,        // 'S' speed up
    TIMER,          // 'T' timer
    BLOCK,          // '#' block with nothing
    BLOCK_DOOR,     // '$' door 
    BLOCK_POWERUP,  // '#' block(power up)
    BLOCK_LIFEUP,   // '#' block(life up)
    BLOCK_BOMBUP,   // '#' block(bomb up)
    BLOCK_SPEEDUP,  // '#' block(speed up)
    BLOCK_TIMER,    // '#' block(timer)
};

struct Bomb
{
    Bomb(int px, int py, int bbtime, int ic, int pw):
        x(px),y(py),bombtime(bbtime),timetobomb(bbtime),changetime(100),timetochange(100),icon(ic),power(pw){};
    int x,y;
    int bombtime;
    int timetobomb;  
    int changetime;
    int timetochange;
    int icon;
    int power;
};

struct Player
{
    Player(int px, int py, int ic):
        x(px),y(py),icon(ic),life(1),bombnum(10),bombtime(3000),bombpower(3),timer(false){};
    int x,y;
    int icon;
    int life;
    size_t bombnum;
    int bombtime;
    int bombpower;
    bool timer;
    vector<Bomb> bomb;
};

struct Monster
{
    Monster(int px, int py, int ic, int at):
        x(px),y(py),icon(ic),actiontime(at),timetoaction(at),isdead(false),deadtime(1000){};
    int x,y;
    int icon;
    const int actiontime;
    int timetoaction;
    bool isdead;
    int deadtime;
};

struct Monster0: public Monster
{
    Monster0(int px, int py):
        Monster(px,py,MONSTER0,1000){}
};
struct Monster1: public Monster
{
    Monster1(int px, int py):
        Monster(px,py,MONSTER1,800){}
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
        int npowerup, int nlifeup, int nbombup, int nspeedup, int ntimer, int ndoor,
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
         * block, fake door, power up, life up, bomb up, speed up, timer
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
        else if (nspeedup > 0 && nspeedup--)
            outMapArry[y][x] = BLOCK_SPEEDUP;
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

#define INFO_WIDTH  60
#define INFO_HEIGHT 6
#define GAME_WIDTH  43
#define GAME_HEIGHT 23
#define CHAT_WIDTH  (INFO_WIDTH - GAME_WIDTH)
#define CHAT_HEIGHT GAME_HEIGHT
#define TIME_TICKS_MS 10

void refresh_info_win(WINDOW *ptrInfoWin)
{
    touchwin(ptrInfoWin);
    wrefresh(ptrInfoWin);
}

void refresh_game_win(WINDOW *ptrGameWin, vector<vector<int>>& mapArry, vector<Monster*>& monsters, Player& player, int passedms)
{
    /*
     * monster move
     */
    int dx[] = {-1,1,0,0};
    int dy[] = {0,0,-1,1};
    int x, y, h, w, ch;
    h = mapArry.size();
    w = mapArry[0].size();
    for (auto it = monsters.begin(); it != monsters.end(); )
    {
        auto ptrMon = *it;
        if (ptrMon->isdead) // dead
        {
            ptrMon->deadtime -= passedms;
            ptrMon->icon = MONSTER_DEAD;
            if (ptrMon->deadtime < 0)
            {
                if (mapArry[ptrMon->y][ptrMon->x] == MONSTER_DEAD)
                    mapArry[ptrMon->y][ptrMon->x] = EMPTY;
                delete ptrMon;
                it = monsters.erase(it);
                continue;
            }
            else
            {
                if (mapArry[ptrMon->y][ptrMon->x] != PLAYER0)
                    mapArry[ptrMon->y][ptrMon->x] = ptrMon->icon;
            }
        }
        else // alive
        {
            ptrMon->timetoaction -= passedms;
            if (ptrMon->timetoaction < 0)
            {
                ptrMon->timetoaction = ptrMon->actiontime;
                mapArry[ptrMon->y][ptrMon->x] = EMPTY;
                x = ptrMon->x, y = ptrMon->y;
                for (;;)
                {
                    int idx = rand() % 4;
                    x = ptrMon->x + dx[idx];
                    y = ptrMon->y + dy[idx];
                    if (x >= 0 && x < w && y >= 0 && y < h)
                        break;
                }
                switch (mapArry[y][x])
                {
                    case EMPTY:
                        ptrMon->x = x, ptrMon->y = y;
                        break;
                    case PLAYER0:
                    case PLAYER1:
                    case PLAYER2:
                    case PLAYER3:
                        break;
                }
                mapArry[ptrMon->y][ptrMon->x] = ptrMon->icon;
            }
        }
        it++;
    }

    for (auto it = player.bomb.begin(); it != player.bomb.end(); )
    {
        it->timetobomb -= passedms;
        it->timetochange -= passedms;
        if (it->timetochange < 0)
        {
            if (it->icon == BOMB_SMALL)
                it->icon = BOMB_BIG;
            else
                it->icon = BOMB_SMALL;
            it->timetochange = it->changetime;
        }
        mapArry[it->y][it->x] = it->icon;
        if (it->timetobomb < 0)
        {
            mapArry[it->y][it->x] = EMPTY;
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
                            break;
                        case STONE:          // '@' stone
                            flag = true;
                            break;
                        case BOMB_SMALL:     // 'o' bomb
                            flag = true;
                            break;
                        case BOMB_BIG:       // 'O' 
                            flag = true;
                            break;
                        case MONSTER0:       // '0' monster
                        case MONSTER1:       // '1' monster
                        case MONSTER2:       // '2' monster
                        case MONSTER3:       // '3' monster
                        case MONSTER4:       // '4' monster
                        case MONSTER5:       // '5' monster
                            find_if(monsters.begin(), monsters.end(), [&](Monster* ptrMon){
                                    if (ptrMon->x == nx && ptrMon->y == ny)
                                    {
                                    ptrMon->isdead = true;
                                    return true;
                                    }
                                    return false;
                                    });
                            break;
                        case MONSTER_DEAD:   // 'x' monster dead
                            break;
                        case PLAYER0:        // 'a' 
                        case PLAYER1:        // 'b'
                        case PLAYER2:        // 'c'
                        case PLAYER3:        // 'd'
                            // TODO:  player died
                            break;
                        case DOOR:           // '$' door to next level
                            // TODO: spawn monsters
                            flag = true;
                            break;
                        case POWERUP:        // 'P' power up
                        case LIFEUP:         // 'L' life up
                        case BOMBUP:         // 'B' bomb up
                        case SPEEDUP:        // 'S' speed up
                        case TIMER:          // 'T' timer
                            flag = true;
                            break;
                        case BLOCK:          // '#' block with nothing
                            mapArry[ny][nx]=EMPTY;
                            flag = true;
                            break;
                        case BLOCK_DOOR:     // '$' door 
                            mapArry[ny][nx]=DOOR;
                            flag = true;
                            break;
                        case BLOCK_POWERUP:  // '#' block(power up)
                            mapArry[ny][nx]=POWERUP;
                            flag = true;
                            break;
                        case BLOCK_LIFEUP:   // '#' block(life up)
                            mapArry[ny][nx]=LIFEUP;
                            flag = true;
                            break;
                        case BLOCK_BOMBUP:   // '#' block(bomb up)
                            mapArry[ny][nx]=BOMBUP;
                            flag = true;
                            break;
                        case BLOCK_SPEEDUP:  // '#' block(speed up)
                            mapArry[ny][nx]=SPEEDUP;
                            flag = true;
                            break;
                        case BLOCK_TIMER:    // '#' block(timer)
                            mapArry[ny][nx]=TIMER;
                            flag = true;
                            break;
                        default:
                            flag = false;
                            break;
                    }
                    if (flag) break;
                }
            }
            it=player.bomb.erase(it);
            continue;
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
                case MONSTER_DEAD:  ch = 'x'; break;
                case PLAYER0:       ch = 'a'; break;// 'a' me
                case PLAYER1:       ch = 'b'; break;// 'b' me
                case PLAYER2:       ch = 'c'; break;// 'c' me
                case PLAYER3:       ch = 'd'; break;// 'd' me
                case DOOR:          ch = '$'; break;// '$' door to next level
                case POWERUP:       ch = 'P'; break;// 'P' power up
                case LIFEUP:        ch = 'L'; break;// 'L' life up
                case BOMBUP:        ch = 'B'; break;// 'B' bomb up
                case SPEEDUP:       ch = 'S'; break;// 'S' speed up
                case TIMER:         ch = 'T'; break;// 'T' timer
                case BLOCK:         // '#' block with nothing
                case BLOCK_DOOR:    // '$' door 
                case BLOCK_POWERUP: // '#' block(power up)
                case BLOCK_LIFEUP:  // '#' block(life up)
                case BLOCK_BOMBUP:  // '#' block(bomb up)
                case BLOCK_SPEEDUP: // '#' block(speed up)
                case BLOCK_TIMER:   ch = '#'; break;// '#' block(timer)
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

int main(int argc, char** argv)
{
    int ch;
    initscr();
    cbreak();
    noecho();
    keypad(stdscr,TRUE);
    nodelay(stdscr,true);
    curs_set(0);
    WINDOW *ptrInfoWin = newwin(INFO_HEIGHT,INFO_WIDTH,0,0);
    box(ptrInfoWin, ACS_VLINE, ACS_HLINE);
    WINDOW *ptrGameWin = newwin(GAME_HEIGHT, GAME_WIDTH, INFO_HEIGHT, 0);
    box(ptrGameWin, ACS_VLINE, ACS_HLINE);
    WINDOW *ptrChatWin = newwin(CHAT_HEIGHT, CHAT_WIDTH, INFO_HEIGHT, GAME_WIDTH);
    box(ptrChatWin, ACS_VLINE, ACS_HLINE);
    vector<Monster*> monsters;
    vector<vector<int>> mapArry;
    Player player(0,0,PLAYER0);
    generate_data(GAME_WIDTH-2, GAME_HEIGHT-2, 200, 3,3,3,3,3,3, 1, 3, 1, 1, 1, 1, mapArry, monsters,player);
    for(;;)
    {
        if ((ch = tolower(getch())) != ERR)
        {
            if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')
            {
                int x = player.x;
                int y = player.y;
                int h = mapArry.size();
                int w = mapArry[0].size();
                mapArry[y][x] = EMPTY;
                switch (tolower(ch))
                {
                    case KEY_UP:    
                        y--; 
                        break;
                    case KEY_DOWN:  
                        y++; 
                        break;
                    case KEY_LEFT:  
                        x--; 
                        break;
                    case KEY_RIGHT: 
                        x++; 
                        break;
                    case ' ':
                        if (player.bombnum > player.bomb.size())
                            player.bomb.push_back(Bomb(x,y,player.bombtime, BOMB_SMALL, player.bombpower));
                        break;
                    default:
                        break;
                }
                if (x>=0&&x<w&&y>=0&&y<h)
                {
                    if (mapArry[y][x] == EMPTY || mapArry[y][x] == MONSTER_DEAD)
                        player.x = x, player.y = y;
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
    endwin();
    exit(0);
}
