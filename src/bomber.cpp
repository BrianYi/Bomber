#include <asm-generic/errno.h>
#include <climits>
#include <cstring>
#include <sys/poll.h>
#include <unistd.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h> 
#include <vector>
#include <algorithm>
#include <string>
#include <queue>
#include "unp.h"
using namespace std;

// 服务器端口号
#define SERVER_PORT 34567

/*
 * 游戏设定宽高
 */
#define WIDTH       60
#define HEIGHT      31
#define INFO_WIDTH  WIDTH
#define INFO_HEIGHT 6
#define GAME_WIDTH  WIDTH//43
#define GAME_HEIGHT 25
#define CHAT_WIDTH  30 //(INFO_WIDTH - GAME_WIDTH)
#define CHAT_HEIGHT HEIGHT

/*
 * 游戏运行时间间隔以10ms为单位
 */
#define TIME_TICKS_MS 10

/*
 * 消息最大长度
 */
#define MAX_MSG_LEN     4096
#define MAX_CHATMSG_LEN 1024

/*
 * 怪物移动速率单位ms
 */
#define MONSTER0_SPEED 1000
#define MONSTER1_SPEED 800
#define MONSTER2_SPEED 600
#define MONSTER3_SPEED 400
#define MONSTER4_SPEED 200
#define MONSTER5_SPEED 100

/*
 * 击败怪物后获得的分数
 */
#define MONSTER0_SCORE  10
#define MONSTER1_SCORE  20
#define MONSTER2_SCORE  30
#define MONSTER3_SCORE  40
#define MONSTER4_SCORE  50
#define MONSTER5_SCORE  60

/*
 * superman是游戏中的S图标,吃掉后可以穿墙,但有时间限制
 * 下述宏用于定义superman的持续时间
 */
#define SUPERMAN_TIME   30000

/*
 * 炸弹引爆前持续时间(也就是o或O这个状态持续时间)
 */
#define BOMB_BOMB_TIME  3000

/*
 * 炸弹引爆后持续时间(也就是x或X这个状态持续时间)
 * BOMB_CHANGE_TIME是形状变化时间(即x->X,X->x,o->O,O->o)
 */
#define BOMB_BOMBING_TIME   200
#define BOMB_CHANGE_TIME    100

// Linux下没有专用的毫秒休眠,因此使用msleep来达到效果
#define msleep(ms) usleep(ms * 1000)
 
/**
 * @brief 游戏菜单枚举
 */
enum
{
    START_GAME,
    JOIN_GAME,
    ABOUT,
    EXIT,
};

/**
 * @brief 障碍物,怪物,物品,玩家等种类枚举
 */
enum
{
    EMPTY,          // ' ' 空地
    STONE,          // '@' 石头(无法通过,若为superman则可以)
    BOMB_SMALL,     // 'o' 炸弹引爆前图标
    BOMB_BIG,       // 'O' 炸弹引爆前图标
    MONSTER0,       // '0' 怪物0
    MONSTER1,       // '1' 怪物1
    MONSTER2,       // '2' 怪物2
    MONSTER3,       // '3' 怪物3
    MONSTER4,       // '4' 怪物4
    MONSTER5,       // '5' 怪物5
    PLAYER0,        // 'a' 玩家0(用字母a表示)
    PLAYER1,        // 'b' 玩家1(用字母b表示)
    PLAYER2,        // 'c' 玩家2(用字母c表示)
    PLAYER3,        // 'd' 玩家3(用字母d表示)
    DOOR,           // '$' 门,去往下一关
    BOMB_POWER_UP,  // 'P' 炸弹威力+1
    LIFEUP,         // 'L' 生命+1
    BOMB_NUM_UP,    // 'B' 炸弹数量+1
    SUPERMAN,       // 'S' superman
    TIMER_BOMB,     // 'T' 定时炸弹
    BLOCK,                  // '#' 墙(可被炸毁)
    BLOCK_DOOR,             // '#' 墙(可被炸毁,含有$)
    BLOCK_BOMB_POWER_UP,    // '#' 墙(可被炸毁,含有P)
    BLOCK_LIFEUP,           // '#' 墙(可被炸毁,含有L) 
    BLOCK_BOMB_NUM_UP,      // '#' 墙(可被炸毁,含有B)
    BLOCK_SUPERMAN,         // '#' 墙(可被炸毁,含有S) 
    BLOCK_TIMER_BOMB,       // '#' 墙(可被炸毁,含有T)
    BOMBING0,       // 炸弹爆炸阶段形状0(即'X') 
    BOMBING1,       // 炸弹爆炸阶段形状1(即'x')
};

/**
 * @brief 通用结构
 */
struct Elem
{
    Elem(int px, int py, int ic):
        x(px),y(py),icon(ic){};
    int x,y;
    int icon;
};

/**
 * @brief 炸弹
 */
struct Bomb
{
    Bomb(int px, int py, int bbtime, int ic, int pw, bool tmbomb):
        x(px),y(py),bombtime(bbtime),timetobomb(bbtime),changetime(BOMB_CHANGE_TIME),timetochange(BOMB_CHANGE_TIME),
        bombingtime(BOMB_BOMBING_TIME),timetobombingfinish(BOMB_BOMBING_TIME),bombingicon(BOMBING0),
        icon(ic),power(pw),isfirstbombing(1),timerbomb(tmbomb){};
    int x,y;                // 炸弹位置
    int bombtime;           // 炸弹引爆时间
    int timetobomb;         // 还剩多少时间将爆炸
    int changetime;         // 炸弹形状变化时间(形状由O->o)
    int timetochange;       // 还剩多少时间将变化形状
    int bombingtime;        // 炸弹爆炸阶段持续时间
    int timetobombingfinish;// 还剩多少时间炸弹爆炸阶段结束
    int bombingicon;        // 炸弹爆炸阶段的图标(X或x)
    int icon;               // 炸弹图标(O或o)
    int power;              // 炸弹威力(1表示周围1格都被被炸到)
    bool isfirstbombing;    // 用于标记是否进入爆炸阶段
    bool timerbomb;         // 标记是否是定时炸弹
    vector<Elem> dieds;     // 记录炸毁的物体
};

/**
 * @brief 网络通信模块
 */
struct Network
{
    Network():
        isopen(0){}
    bool isopen;
    vector<int> fdplayers;
};

/**
 * @brief 数据包结构
 */
struct DataPacket
{
    DataPacket(char* dat, int n)
    {
        len = n;
        data = new char[n];
        memcpy(data,dat,n); 
    }
    ~DataPacket()
    {
        if (data)
            delete data;
    }
    int len;
    char *data;
};

/**
 * @brief 消息数据结构
 */
struct ChatMsg
{
    ChatMsg(const char* mesg, int n)
    {
        len = n+1;
        msg = new char[n+2];
        memcpy(msg,mesg,n);
        msg[n] = '\n';
        msg[len]='\0';
    }
    ~ChatMsg()
    {
        if (msg)
            delete msg;
    }
    int len;
    char *msg;
};

/**
 * @brief 玩家,实际整个游戏只有这一个对象,可以认为这应该是管理全局数据的一个结构体
 */
struct Player
{
    Player(int px, int py, int ic):
        x(px),y(py),icon(ic),score(0),life(1),bombnum(1),bombtime(BOMB_BOMB_TIME),bombpower(1),timerbomb(0),superman(0),
        supermantime(SUPERMAN_TIME),timetosupermanfinish(0),savedicon(EMPTY){};
    int x,y;                    // 玩家坐标
    int icon;                   // 玩家图标
    int score;                  // 玩家分数
    int life;                   // 玩家生命
    int bombnum;                // 玩家炸弹数量
    int bombtime;               // 玩家炸弹引爆时间
    int bombpower;              // 玩家炸弹威力
    bool timerbomb;             // 玩家是否有定时器炸弹
    bool superman;              // 玩家是否为superman
    int supermantime;           // 玩家superman默认时间
    int timetosupermanfinish;   // 还剩多少时间结束superman阶段
    int savedicon;              // 玩家当前位置原来的图标(玩家每走一个位置,都会保存那个位置图标,并将玩家自身图标覆盖那个位置)
    Network net;                // 网络模块
    vector<Bomb> bomb;          // 玩家炸弹记录(即玩家已经安置了几个炸弹,每个炸弹的信息)
    queue<DataPacket*> packetQue;   // TODO: 用于以后的多人联机
    queue<ChatMsg*> msgQue;     // 本地显示消息
} player(0,0,PLAYER0);  // 玩家本人

/**
 * @brief 怪物基类
 */
struct Monster
{
    Monster(int px, int py, int ic, int at, int sco):
        x(px),y(py),icon(ic),actiontime(at),timetoaction(at),score(sco){};
    int x,y;    // 怪物位置
    int icon;   // 怪物图标
    const int actiontime;   // 怪物移动时间间隔ms
    int timetoaction;       // 还剩多少时间进行下一次移动
    int score;              // 怪物包含的分数
};

/**
 * @brief 怪物0
 */
struct Monster0: public Monster
{
    Monster0(int px, int py):
        Monster(px,py,MONSTER0,MONSTER0_SPEED,MONSTER0_SCORE){}
};

/**
 * @brief 怪物1
 */
struct Monster1: public Monster
{
    Monster1(int px, int py):
        Monster(px,py,MONSTER1,MONSTER1_SPEED,MONSTER1_SCORE){}
};

/**
 * @brief 怪物2
 */
struct Monster2: public Monster
{
    Monster2(int px, int py):
        Monster(px,py,MONSTER2,MONSTER2_SPEED,MONSTER2_SCORE){}
};

/**
 * @brief 怪物3
 */
struct Monster3: public Monster
{
    Monster3(int px, int py):
        Monster(px,py,MONSTER3,MONSTER3_SPEED,MONSTER3_SCORE){}
};


/**
 * @brief 怪物4
 */
struct Monster4: public Monster
{
    Monster4(int px, int py):
        Monster(px,py,MONSTER4,MONSTER4_SPEED,MONSTER4_SCORE){}
};

/**
 * @brief 怪物5
 */
struct Monster5: public Monster
{
    Monster5(int px, int py):
        Monster(px,py,MONSTER5,MONSTER5_SPEED,MONSTER5_SCORE){}
};

/*
 * DESC: function macro
 *  功能宏,此宏用于管理全局的消息(即显示在画面右边的文字消息,包含用户聊天消息,网络消息,系统消息等等)
 */
#define MSGQUE_PUSH(msg,n)  player.msgQue.push(new ChatMsg(msg,n))
#define MSGQUE_FRONT()      player.msgQue.front()
#define MSGQUE_POP()        player.msgQue.pop()
#define MSGQUE_EMPTY()      player.msgQue.empty()


/**
 * @brief 将本机作为服务器,监听本地端口,等待其他玩家连接
 *
 * @param arg
 *
 * @return 
 */
void* th_receiver(void *arg)
{
    struct pollfd client[_POSIX_OPEN_MAX];
    struct sockaddr servaddr,cliaddr;
    bzero(&servaddr,sizeof(servaddr));
    int i,maxi,nready,connfd,sockfd;
    int n;
    char buf[MAX_MSG_LEN];
    socklen_t clilen,servlen;
    
    int listenfd = Tcp_listen("localhost",nullptr,&servlen);
    Getsockname(listenfd, &servaddr, &servlen);
    char *p = Sock_ntop(&servaddr, servlen);
    snprintf(buf,sizeof(buf),"your game is now opened to network, and address is %s", p);
    MSGQUE_PUSH(buf,sizeof(buf));

    client[0].fd = listenfd; 
    client[0].events = POLLRDNORM;
    
    for (i=1;i<_POSIX_OPEN_MAX;++i)
        client[i].fd = -1;
    
    maxi=0;
    
    for(;;)
    {
        nready = Poll(client, maxi+1, INFTIM);
        if (client[0].revents & POLLRDNORM)
        {
            clilen = sizeof(cliaddr);
            connfd = Accept(listenfd, (SA *)&cliaddr, &clilen);
            for (i = 1; i < _POSIX_OPEN_MAX; ++i)
            {
                if (client[i].fd < 0)
                {
                    client[i].fd = connfd;
                    player.net.fdplayers.push_back(connfd);
                    break;
                }
            }
            if (i == _POSIX_OPEN_MAX)
                err_quit("too many clients");
            client[i].events = POLLRDNORM;
            if (i > maxi)
                maxi = i;
            if (--nready <= 0)
                continue;
        }
        for (i = 1; i <= maxi; ++i)
        {
            if ((sockfd = client[i].fd) < 0)
                continue;
            if (client[i].revents & (POLLRDNORM | POLLERR))
            {
                if ((n = read(sockfd, buf, MAX_MSG_LEN)) < 0)
                {
                    if (errno == ECONNRESET)
                    {
                        Close(sockfd);
                        client[i].fd = -1;
                    } else
                        err_sys("read error");
                } else if (n == 0) {
                    Close(sockfd);
                    client[i].fd = -1;
                } else { // received data
                    if (buf[n-1] == '\n')
                    {
                        player.packetQue.push(new DataPacket(buf, n));
                    }
                    else
                        err_sys("read data incomplete");
                }
                if (--nready <= 0)
                    break;
            }
        }
    }
}

/**
 * @brief 生成地图数据 
 *
 * @param w                 地图宽
 * @param h                 地图高
 * @param nblock            墙数量
 * @param nbomb_power_up    物品P数量
 * @param nlifeup           物品L数量
 * @param nbomb_num_up      物品B数量
 * @param nsuperman         物品S数量
 * @param ntimer            物品T数量
 * @param ndoor             $数量
 * @param nMonster0         怪物0数量
 * @param nMonster1         怪物1数量
 * @param nMonster2         怪物2数量 
 * @param nMonster3         怪物3数量
 * @param nMonster4         怪物4数量
 * @param nMonster5         怪物5数量
 * @param outMapArry        返回地图数据
 * @param outMonsters       返回怪物数据
 */
void generate_data(int w, int h, int nblock, 
        int nbomb_power_up, int nlifeup, int nbomb_num_up, int nsuperman, int ntimer, int ndoor,
        int nMonster0,int nMonster1,int nMonster2,int nMonster3,int nMonster4, int nMonster5,
        vector<vector<int>>& outMapArry, vector<Monster*>& outMonsters)
{
    int x, y;

    // 初始地图大小为h*w
    outMapArry.resize(h,vector<int>(w,EMPTY));
    
    // 玩家位置(y,x),y表示行号,x表示列号
    outMapArry[player.y][player.x] = player.icon;

    // 生成石块
    for (y = 0; y < h; ++y)
    {
        for (x = 0; x < w; ++x)
        {
            if (x%2&&y%2)
                outMapArry[y][x]=STONE;
        }
    }

    // 随机数种子
    srand(time(0));

    // 生成随机物品
    while (ndoor>0||nbomb_power_up>0||nlifeup>0||nbomb_num_up>0||nsuperman>0||ntimer>0||nblock>0)
    {
        y = rand() % h;
        x = rand() % w;

        // 只在空地处生成
        if (outMapArry[y][x] != EMPTY) continue;
        if (ndoor > 0)
        {
            outMapArry[y][x] = BLOCK_DOOR;
            ndoor--;
        }
        else if (nbomb_power_up > 0)
        {
            outMapArry[y][x] = BLOCK_BOMB_POWER_UP;
            nbomb_power_up--;
        }
        else if (nlifeup > 0)
        {
            outMapArry[y][x] = BLOCK_LIFEUP;
            nlifeup--;
        }
        else if (nbomb_num_up > 0)
        {
            outMapArry[y][x] = BLOCK_BOMB_NUM_UP;
            nbomb_num_up--;
        }
        else if (nsuperman > 0)
        {
            outMapArry[y][x] = BLOCK_SUPERMAN;
            nsuperman--;
        }
        else if (ntimer > 0)
        {
            outMapArry[y][x] = BLOCK_TIMER_BOMB;
            ntimer--;
        }
        else if (nblock > 0)
        {
            outMapArry[y][x] = BLOCK;
            nblock--;
        }
        else
        {
            perror("error!");
            exit(0);
        }
    }

    // 生成怪物
    while (nMonster5 > 0||nMonster4 > 0||nMonster3 > 0||nMonster2 > 0||nMonster1 > 0||nMonster0 > 0)
    {
        x = rand() % w;
        y = rand() % h;
        
        // 只在空地处生成
        if ((outMapArry[y][x]) != EMPTY) continue;

        /*
         * 保持追踪每个怪物的坐标信息
         */
        if (nMonster0 > 0)
        {
            outMonsters.push_back(new struct Monster0(x,y));
            nMonster0--;
        }
        else if (nMonster1 > 0)
        {
            outMonsters.push_back(new struct Monster1(x, y));
            nMonster1--;
        }
        else if (nMonster2 > 0)
        {
            outMonsters.push_back(new struct Monster2(x, y));
            nMonster2--;
        }
        else if (nMonster3 > 0)
        {
            outMonsters.push_back(new struct Monster3(x, y));
            nMonster3--;
        }
        else if (nMonster4 > 0)
        {
            outMonsters.push_back(new struct Monster4(x, y));
            nMonster4--;
        }
        else if (nMonster5 > 0)
        {
            outMonsters.push_back(new struct Monster5(x, y));
            nMonster5--;
        }
        else
        {
            perror("error!");
            exit(0);
        }

        // 设置怪物图标
        outMapArry[y][x] = outMonsters.back()->icon;
    }
}


/**
 * @brief 刷新信息窗口(信息窗口是界面最上方的窗口,包含玩家分数等等信息)
 *
 * @param ptrInfoWin 信息窗口对象
 */
void refresh_info_win(WINDOW *ptrInfoWin)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "score:%04d life:%-2d bomb_num:%-2d", 
            player.score, player.life, player.bombnum);
    mvwprintw(ptrInfoWin, 1, 2, buf);
    snprintf(buf,sizeof(buf),"bomb_power:%-3d timerbomb:%d",
            player.bombpower, player.timerbomb);
    mvwprintw(ptrInfoWin, 2, 2, buf);
    snprintf(buf,sizeof(buf),"superman:%02ds", player.timetosupermanfinish/1000);
    mvwprintw(ptrInfoWin, 3, 2, buf);
    snprintf(buf,sizeof(buf),"bomb_left:%-2d", (int)player.bomb.size());
    mvwprintw(ptrInfoWin, 4, 2, buf);
    touchwin(ptrInfoWin);
    wrefresh(ptrInfoWin);
}

/**
 * @brief 刷新游戏窗口(游戏窗口为信息窗口下方的窗口)
 *
 * @param ptrGameWin 游戏窗口对象
 * @param mapArry 游戏地图数据
 * @param monsters 游戏怪物数据
 * @param passedms 游戏刷新间隔
 */
void refresh_game_win(WINDOW *ptrGameWin, vector<vector<int>>& mapArry, vector<Monster*>& monsters, int passedms)
{
    int dx[] = {-1,1,0,0};
    int dy[] = {0,0,-1,1};
    int x, y, h, w, ch;
    h = mapArry.size();
    w = mapArry[0].size();

    /*
     * 处理炸弹
     */
    for (auto it = player.bomb.begin(); it != player.bomb.end(); )
    {
        // 如果炸弹还处于引爆阶段,那么就让它一直变形(o->O,O->o)
        if (it->timetobomb > 0 || it->timerbomb)
        {
            /*
             * not boming
             */
            it->timetochange -= passedms;
            if (!it->timerbomb)
                it->timetobomb -= passedms;
            if (it->timetochange <= 0)
            {
                if (it->icon == BOMB_SMALL)
                    it->icon = BOMB_BIG;
                else
                    it->icon = BOMB_SMALL;
                it->timetochange = it->changetime;
                mapArry[it->y][it->x] = it->icon;
            }
        }

        // 如果炸弹已经到了爆炸阶段
        if (it->timetobomb <= 0)
        {
            /*
             * 炸弹已经处于爆炸阶段,持续时间为timetobombingfinish
             */
            it->timetobombingfinish -= passedms;

            /*
             * 炸弹爆炸结束,处理爆炸后受到炸弹影响的位置处的图标
             */
            if (it->timetobombingfinish <= 0) 
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
                        case BOMB_POWER_UP:        // 'P' power up
                        case LIFEUP:         // 'L' life up
                        case BOMB_NUM_UP:         // 'B' bomb up
                        case SUPERMAN:        // 'S' superman
                        case TIMER_BOMB:          // 'T' timer
                            mapArry[deadelem.y][deadelem.x] = deadelem.icon;
                            break;
                        case BLOCK:          // '#' block with nothing
                            mapArry[deadelem.y][deadelem.x] = EMPTY;
                            break;
                        case BLOCK_DOOR:     // '$' door 
                            mapArry[deadelem.y][deadelem.x]=DOOR;
                            break;
                        case BLOCK_BOMB_POWER_UP:  // '#' block(power up)
                            mapArry[deadelem.y][deadelem.x]=BOMB_POWER_UP;
                            break;
                        case BLOCK_LIFEUP:   // '#' block(life up)
                            mapArry[deadelem.y][deadelem.x]=LIFEUP;
                            break;
                        case BLOCK_BOMB_NUM_UP:   // '#' block(bomb up)
                            mapArry[deadelem.y][deadelem.x]=BOMB_NUM_UP;
                            break;
                        case BLOCK_SUPERMAN:  // '#' block(superman)
                            mapArry[deadelem.y][deadelem.x]=SUPERMAN;
                            break;
                        case BLOCK_TIMER_BOMB:    // '#' block(timer)
                            mapArry[deadelem.y][deadelem.x]=TIMER_BOMB;
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
                
                // 这颗炸弹已经完成了它的使命,可以销毁了
                it=player.bomb.erase(it);
                continue;
            }
            else  // 炸弹爆炸期间
            {
                // 离炸弹爆炸结束时间小于100ms时,变化爆炸图标由X->x,给人一种火焰变小的动态感
                if (it->timetobombingfinish <= 100 && it->bombingicon != BOMBING1) 
                {
                    it->bombingicon = BOMBING1;
                    for (auto& deadelem : it->dieds)
                        mapArry[deadelem.y][deadelem.x] = it->bombingicon;
                }

                // 是否是刚刚从引爆阶段到达爆炸阶段(即刚刚爆炸)
                if (it->isfirstbombing)
                {
                    // 刚刚爆炸,则需要记录爆炸时影响到的各个位置和怪物信息
                    // 且将炸弹爆炸期间炸到的位置信息和物品放入
                    it->isfirstbombing=false;
                    it->dieds.push_back(Elem(it->x,it->y,EMPTY));
                    mapArry[it->y][it->x] = it->bombingicon;

                    /*
                     * 从4个方向来进行处理
                     * 比如:    #
                     *          #
                     *         @o@
                     *
                     *          #
                     *
                     *  #墙,@石头,o炸弹,如果炸弹威力为2,则爆炸时如下
                     *          #
                     *          X
                     *         @X@
                     *          X
                     *          X
                     *  因为炸弹不能一次穿过两堵墙#,因此在某个方向上只要遇到第一堵墙#,则该方向上炸弹威力耗尽
                     *  炸弹是不能炸毁石头的,因此两边的石头@没变化
                     *  但遇到怪物,空地或者玩家,则不会对炸弹威力有影响
                     *  可以结合实际想象一下
                     */
                    for (int i=0;i<4;++i)
                    {
                        bool flag=false;    // 用于标记是否该方向上炸弹威力耗尽
                        // 在该方向上处理炸弹爆炸后的影响
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
                                                {
                                                player.score += ptrMon->score;
                                                delete ptrMon;
                                                return true;
                                                }
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
                                case BOMB_POWER_UP:        // 'P' power up
                                case LIFEUP:         // 'L' life up
                                case BOMB_NUM_UP:         // 'B' bomb up
                                case SUPERMAN:        // 'S' superman
                                case TIMER_BOMB:          // 'T' timer
                                    flag=true;
                                    break;
                                case BLOCK:          // '#' block with nothing
                                case BLOCK_DOOR:     // '$' door 
                                case BLOCK_BOMB_POWER_UP:  // '#' block(power up)
                                case BLOCK_LIFEUP:   // '#' block(life up)
                                case BLOCK_BOMB_NUM_UP:   // '#' block(bomb up)
                                case BLOCK_SUPERMAN:  // '#' block(superman)
                                case BLOCK_TIMER_BOMB:    // '#' block(timer)
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
     * 怪物移动处理
     */
    for (auto it = monsters.begin(); it != monsters.end(); )
    {
        Monster* ptrMon = *it;
        ptrMon->timetoaction -= passedms;
        if (ptrMon->timetoaction <= 0) // 怪物可以移动了
        {
            // 下一轮移动需要等待的时间
            ptrMon->timetoaction = ptrMon->actiontime;  

            // 先将怪物当前位置置空,再计算新的位置
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

            // 新位置更新
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
                    player.score += ptrMon->score;
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
     * 其他处理:
     *  玩家superman时间处理
     */
    if (player.superman)
    {
        player.timetosupermanfinish -= passedms;
        if (player.timetosupermanfinish <= 0)
        {
            player.timetosupermanfinish = 0;
            player.superman = false;
        }
    }

    /*
     * 显示新的地图
     */
    for (y = 0; y < h; ++y)
    {
        for (x = 0; x < w; ++x)
        {
            switch (mapArry[y][x])
            {
                case EMPTY:                 ch = ' '; break;// ' '                    
                case STONE:                 ch = '@'; break;// '@' stone
                case BOMB_SMALL:            ch = 'o'; break;// 'o' bomb
                case BOMB_BIG:              ch = 'O'; break;// 'O' bomb
                case MONSTER0:              ch = '0'; break;// 'M' monster
                case MONSTER1:              ch = '1'; break;// 'M' monster
                case MONSTER2:              ch = '2'; break;// 'M' monster
                case MONSTER3:              ch = '3'; break;// 'M' monster
                case MONSTER4:              ch = '4'; break;// 'M' monster
                case MONSTER5:              ch = '5'; break;// 'M' monster
                case PLAYER0:               ch = 'a'; break;// 'a' me
                case PLAYER1:               ch = 'b'; break;// 'b' me
                case PLAYER2:               ch = 'c'; break;// 'c' me
                case PLAYER3:               ch = 'd'; break;// 'd' me
                case DOOR:                  ch = '$'; break;// '$' door to next level
                case BOMB_POWER_UP:         ch = 'P'; break;// 'P' power up
                case LIFEUP:                ch = 'L'; break;// 'L' life up
                case BOMB_NUM_UP:           ch = 'B'; break;// 'B' bomb up
                case SUPERMAN:              ch = 'S'; break;// 'S' superman
                case TIMER_BOMB:            ch = 'T'; break;// 'T' timer
                case BLOCK:                                 // '#' block with nothing
                case BLOCK_DOOR:                            // '$' door 
                case BLOCK_BOMB_POWER_UP:                   // '#' block(power up)
                case BLOCK_LIFEUP:                          // '#' block(life up)
                case BLOCK_BOMB_NUM_UP:                     // '#' block(bomb up)
                case BLOCK_SUPERMAN:                        // '#' block(superman)
                case BLOCK_TIMER_BOMB:      ch = '#'; break;// '#' block(timer)
                case BOMBING0:              ch = 'X'; break;// 'X' bombing fire shape
                case BOMBING1:              ch = 'x'; break;// 'x' bombing fire shape
                default:
                                    perror("erro!");
                                    exit(0);
                                    break;
            }
            mvwaddch(ptrGameWin, y+1, x+1, ch);
        }
    }

    // 点亮窗口
    touchwin(ptrGameWin);

    // 刷新窗口
    wrefresh(ptrGameWin);
}



/**
 * @brief 刷新聊天窗口
 *
 * @param ptrChatWin 聊天窗口
 */
void refresh_chat_win(WINDOW *ptrChatWin)
{
    // 从消息队列中取出消息,加载到聊天窗口中
    while (!MSGQUE_EMPTY())
    {
        ChatMsg* ptrMsg = MSGQUE_FRONT();   
        MSGQUE_POP();
        wprintw(ptrChatWin, ptrMsg->msg);
        delete ptrMsg;
    }

    // 点亮窗口
    touchwin(ptrChatWin);

    // 刷新窗口
    wrefresh(ptrChatWin);
}

/**
 * @brief 刷新菜单界面
 *
 * @param ptrWelcomeWin 菜单窗口
 * @param choose 用户选项
 */
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

    // 点亮窗口
    touchwin(ptrWelcomeWin);

    // 刷新窗口
    wrefresh(ptrWelcomeWin);        
}


/**
 * @brief 开始游戏
 */
void start_game()
{
    int ch;
    /*
     * 创建3个窗口: 信息窗口(最上方),游戏窗口(下方),聊天窗口(右边)
     */
    WINDOW *ptrInfoWin = newwin(INFO_HEIGHT,INFO_WIDTH,0,0);
    box(ptrInfoWin, ACS_VLINE, ACS_HLINE);
    WINDOW *ptrGameWin = newwin(GAME_HEIGHT, GAME_WIDTH, INFO_HEIGHT, 0);
    box(ptrGameWin, ACS_VLINE, ACS_HLINE);
    WINDOW *ptrChatWin = newwin(CHAT_HEIGHT, CHAT_WIDTH, 1, WIDTH+1);
    //box(ptrChatWin, ACS_VLINE, ACS_HLINE);
    vector<Monster*> monsters;
    vector<vector<int>> mapArry;

    // 生成地图数据
    generate_data(GAME_WIDTH-2, GAME_HEIGHT-2, 
            100,
            // powerup,bombup,lifeup,superman,timer
            20, 20, 20, 20, 20, 
            1, 
            10,10,10,10,10,10, 
            mapArry, monsters);

    // 获取用户选项,并进行处理,刷新游戏地图
    for(;;)
    {
        if ((ch = tolower(getch())) != ERR)
        {
                int h = mapArry.size();
                int w = mapArry[0].size();
                int oldx = player.x, oldy=player.y;
                int nx=player.x,ny=player.y;
                int flag=false;
                switch (tolower(ch))
                {
                    case KEY_UP:    // 上   
                        ny--; 
                        flag=true;
                        break;
                    case KEY_DOWN:  // 下
                        ny++; 
                        flag=true;
                        break;
                    case KEY_LEFT:  // 左
                        nx--; 
                        flag=true;
                        break;
                    case KEY_RIGHT: // 右
                        nx++; 
                        flag=true;
                        break;
                    case ' ':   // 空格放置炸弹
                        // 放置炸弹数量不能超过玩家可放置数量
                        if (player.bombnum > (int)player.bomb.size())
                        {
                            if (player.savedicon == EMPTY)
                                player.bomb.push_back(Bomb(nx,ny,player.bombtime, BOMB_SMALL, player.bombpower, player.timerbomb));
                        }
                        break;
                    case 's': // s键定时炸弹爆炸
                        if (player.timerbomb)
                        {
                            for (auto& b : player.bomb)
                            {
                                if (b.timetobomb > 0)
                                {
                                    b.timerbomb = false;
                                    b.timetobomb = 0;
                                }
                                break;
                            }
                        }
                        break;
                    case 'o':   // o键开放网络,运行外部接入
                    {
                        if (!player.net.isopen)
                        {
                            player.net.isopen = true;
                            pthread_t t1;
                            pthread_create(&t1, nullptr, th_receiver, nullptr);
                        }
                        break;
                    }
                    case 'q':   // q键退出,返回到主菜单
                        goto exit;
                        break;
                    default:
                        break;
                }

                if (flag&&nx>=0&&nx<w&&ny>=0&&ny<h) // 有效移动
                {
                    mapArry[oldy][oldx]=player.savedicon;   // 恢复旧图标
                    switch (mapArry[ny][nx])
                    {
                        case EMPTY:          // ' '
                            player.savedicon=mapArry[ny][nx];
                            player.x = nx, player.y = ny;
                            break;
                        case STONE:          // '@' stone
                        case BOMB_SMALL:     // 'o' bomb
                        case BOMB_BIG:       // 'O' 
                            if (player.superman)
                            {
                                player.savedicon=mapArry[ny][nx];
                                player.x = nx, player.y = ny;
                            }
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
                        case BOMB_POWER_UP:        // 'P' power up
                            player.savedicon=EMPTY;
                            player.x = nx, player.y = ny;
                            player.bombpower++;
                            break;
                        case LIFEUP:         // 'L' life up
                            player.savedicon=EMPTY;
                            player.x = nx, player.y = ny;
                            player.life++;
                            break;
                        case BOMB_NUM_UP:         // 'B' bomb up
                            player.savedicon=EMPTY;
                            player.x = nx, player.y = ny;
                            player.bombnum++;
                            break;
                        case SUPERMAN:        // 'S' superman
                            player.savedicon=EMPTY;
                            player.x = nx, player.y = ny;
                            player.superman=true;
                            player.timetosupermanfinish+=player.supermantime;
                            break;
                        case TIMER_BOMB:          // 'T' timer
                            player.savedicon=EMPTY;
                            player.x = nx, player.y = ny;
                            player.timerbomb=true;
                            break;
                        case BLOCK:          // '#' block with nothing
                        case BLOCK_DOOR:     // '$' door 
                        case BLOCK_BOMB_POWER_UP:  // '#' block(power up)
                        case BLOCK_LIFEUP:   // '#' block(life up)
                        case BLOCK_BOMB_NUM_UP:   // '#' block(bomb up)
                        case BLOCK_SUPERMAN:  // '#' block(superman)
                        case BLOCK_TIMER_BOMB:    // '#' block(timer)
                            if (player.superman)
                            {
                                player.savedicon=mapArry[ny][nx];
                                player.x = nx, player.y = ny;
                            }
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

        // 刷新信息窗口
        refresh_info_win(ptrInfoWin);

        // 刷新游戏窗口
        refresh_game_win(ptrGameWin, mapArry, monsters, TIME_TICKS_MS);

        // 刷新聊天窗口
        refresh_chat_win(ptrChatWin);

        // 休眠游戏间隔ms
        msleep(TIME_TICKS_MS);
    }
exit:
    /*
     * 内存销毁
     */
    for (auto& ptrMon : monsters)
        delete ptrMon;
    delwin(ptrInfoWin);
    delwin(ptrGameWin);
    delwin(ptrChatWin);
}

/**
 * @brief 加入游戏
 */
void join_game()
{
    /*
     * for multiplayer
     */ 
}

/**
 * @brief 游戏介绍
 */
void about_game()
{
    /*
     * game introduction
     */
}

/**
 * @brief 退出游戏
 */
void exit_game()
{
    /*
     * exit game
     */
    clear();
    refresh();
    exit(0);
}

/**
 * @brief 游戏循环
 */
void game_loop()
{
    int ch;
    // 新建菜单窗口
    WINDOW *ptrWelcomeWin = newwin(HEIGHT,WIDTH,0,0);
    box(ptrWelcomeWin,ACS_VLINE,ACS_HLINE);
    int choose = START_GAME;

    // 游戏菜单界面,等待用户选项
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

/**
 * @brief 开始游戏
 *
 * @param argc
 * @param argv
 *
 * @return 
 */
int main(int argc, char** argv)
{
    initscr();              // 初始化curses库
    cbreak();               // 采用break模式
    noecho();               // 不进行echo显示
    keypad(stdscr,TRUE);    // 把功能键映射为一个值
    nodelay(stdscr,true);   // getch变为不需等待(即异步),用户没输入时自动返回ERR
    curs_set(0);            // 不显示光标
    if (!has_colors())      // 是否支持彩色显示
    {
        exit(1);    
    }
    if (start_color()!=OK)  // 开启彩色显示支持
    {
        exit(1);
    }
    game_loop(); // 开始游戏循环
    endwin();
    exit(0);
}
