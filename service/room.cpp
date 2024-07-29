#include "unpthread.h"
#include "msg.h"
#include "unp.h"
#include <map>
#define SENDTHREADSIZE 5
SEND_QUEUE sendqueue; //save data

enum USER_TYPE
{
    GUEST=2,
    OWNER
};
static volatile int maxfd;
STATUS volatile roomstatus = ON;

typedef struct pool
{
    fd_set fdset;
    pthread_mutex_t lock;
    int owner;
    int num;
    int status[1024 + 10];
    std::map<int, uint32_t> fdToIp;
    pool()
    {
        memset(status, 0, sizeof(status));
        owner = 0;
        FD_ZERO(&fdset);
        lock = PTHREAD_MUTEX_INITIALIZER;
        num = 0;
    }

    void clear_room()
    {
        Pthread_mutex_lock(&lock);
        roomstatus = CLOSE;
        for(int i = 0; i <= maxfd; i++)
        {
            if(status[i] == ON)
            {
                Close(i);
            }
        }
        memset(status, 0, sizeof(status));
        num = 0;
        owner = 0;
        FD_ZERO(&fdset);
        fdToIp.clear();
        sendqueue.clear();
        Pthread_mutex_unlock(&lock);
    }
}Pool;

Pool * user_pool = new Pool();

// room process
void process_main(int i, int fd) // room start
{
    //create accpet fd thread
    MSG_INFO("room %d starting", getpid());
    Signal(SIGPIPE, SIG_IGN);
    pthread_t pfd1;
    void* accept_fd(void *);
    void* send_func(void *);
    void  fdclose(int, int);

    int *ptr = (int *)malloc(4);
    *ptr = fd;
    Pthread_create(&pfd1, NULL, accept_fd, ptr); // accept fd
    for(int i = 0; i < SENDTHREADSIZE; i++)
    {
        Pthread_create(&pfd1, NULL, send_func, NULL);
    }

    //listen read data from fds
    for(;;)
    {
        fd_set rset = user_pool->fdset;
        int nsel;
        struct timeval time;
        memset(&time, 0, sizeof(struct timeval));
        while((nsel = Select(maxfd + 1, &rset, NULL, NULL, &time))== 0)
        {
            rset = user_pool->fdset; // make sure rset update
        }
        for(int i = 0; i <= maxfd; i++)
        {
            //check data arrive
            if(FD_ISSET(i, &rset))
            {
                char head[15] = {0};
                int ret = Readn(i, head, 11); // head size = 11
                if(ret <= 0)
                {
                    MSG_INFO("peer close");
                    fdclose(i, fd);
                }
                else if(ret == 11)
                {
                    // PRTNT_CONTEXT_HEX(head,11)
                    if(head[0] == '$')
                    {
                        //solve datatype
                        MSG_TYPE msgtype;
                        memcpy(&msgtype, head + 1, 2);
                        msgtype = (MSG_TYPE)ntohs(msgtype);

                        MSG msg;
                        memset(&msg, 0, sizeof(MSG));
						msg.targetfd = i;
						memcpy(&msg.ip, head + 3, 4);
						int msglen;
						memcpy(&msglen, head + 7, 4);
						msg.len = ntohl(msglen);

                        if(msgtype == IMG_SEND || msgtype == AUDIO_SEND || msgtype == TEXT_SEND)
                        {
                            msg.msgType = (msgtype == IMG_SEND) ? IMG_RECV : ((msgtype == AUDIO_SEND)? AUDIO_RECV : TEXT_RECV);
                            msg.ptr = (char *)malloc(msg.len);
                            msg.ip = user_pool->fdToIp[i];

                            // PRTNT_CONTEXT_HEX(head,11)
                            // PRTNT_IP_STR(msg.ip)
                            if((ret = Readn(i, msg.ptr, msg.len)) < msg.len)
                            {
                                MSG_ERR("3 msg format error");
                            }
                            else
                            {
                                int tail;
                                Readn(i, &tail, 1);
                                if(tail != '#')
                                {
                                    MSG_ERR("4 msg format error");
                                }
                                else
                                {
                                    sendqueue.push_msg(msg);
                                }
                            }
                        }
						else if(msgtype == CLOSE_CAMERA)
						{
							char tail;
							Readn(i, &tail, 1);
							if(tail == '#' && msg.len == 0)
							{
								msg.msgType = CLOSE_CAMERA;
								sendqueue.push_msg(msg);
							}
							else
							{
								MSG_ERR("camera data error ");
							}
						}
                    }
                    else
                    {
                        MSG_ERR("1 msg format error");
                    }
                }
                else
                {
                    MSG_ERR("2 msg format error");
                }
                if(--nsel <= 0) break;
            }
        }
    }
}

//file description close
void fdclose(int fd, int pipefd)
{

    if(user_pool->owner == fd) // room close
    {
        //room close
        user_pool->clear_room();
		MSG_INFO("clear room");
        //write to father process
        char cmd = 'E';
        if(writen(pipefd, &cmd, 1) < 1)
        {
            MSG_ERR("writen error");
        }
    }
    else
    {
        uint32_t getpeerip(int);
        uint32_t ip;
        //delete fd from pool
        Pthread_mutex_lock(&user_pool->lock);
        ip = user_pool->fdToIp[fd];
        FD_CLR(fd, &user_pool->fdset);
        user_pool->num--;
        user_pool->status[fd] = CLOSE;
        if(fd == maxfd) maxfd--;
        Pthread_mutex_unlock(&user_pool->lock);

        char cmd = 'Q';
        if(writen(pipefd, &cmd, 1) < 1)
        {
            MSG_ERR("write error");
        }

        // msg ipv4

        MSG msg;
        memset(&msg, 0, sizeof(MSG));
        msg.msgType = PARTNER_EXIT;
        msg.targetfd = -1;
        msg.ip = ip; // network order
        Close(fd);
        sendqueue.push_msg(msg);
    }
}

void* accept_fd(void *arg) //accept fd from father
{
    uint32_t getpeerip(int);
    Pthread_detach(pthread_self());
    int fd = *(int *)arg, tfd = -1;
    free(arg);
    while(1)
    {
        int n, c;
        if((n = read_fd(fd, &c, 1, &tfd)) <= 0)
        {
            QUIT_ERR("read_fd error");
        }
        if(tfd < 0)
        {
            MSG_INFO("c = %c", c);
            QUIT_ERR("no descriptor from read_fd");
        }

        //add to poll

        if(c == 'C') // create
        {
            Pthread_mutex_lock(&user_pool->lock); //lock

            FD_SET(tfd, &user_pool->fdset);
            user_pool->owner = tfd;
            user_pool->fdToIp[tfd] = getpeerip(tfd);
            user_pool->num++;
//            user_pool->fds[user_pool->num++] = tfd;
            user_pool->status[tfd] = ON;
            maxfd = MAX(maxfd, tfd);
            //ERR_MSG("c %d", maxfd);
            //write room No to  tfd
            roomstatus = ON; // set on

            Pthread_mutex_unlock(&user_pool->lock); //unlock

            MSG msg;
            msg.msgType = CREATE_MEETING_RESPONSE;
            msg.targetfd = tfd;
            int roomNo = htonl(getpid());
            msg.ptr = (char *) malloc(sizeof(int));
            memcpy(msg.ptr, &roomNo, sizeof(int));
            msg.len = sizeof(int);
            MSG_INFO("roomNo %u", ntohl(roomNo));
            sendqueue.push_msg(msg);

        }
        else if(c == 'J') // join
        {
            Pthread_mutex_lock(&user_pool->lock); //lock
            if(roomstatus == CLOSE) // meeting close (owner close)
            {
                close(tfd);
                Pthread_mutex_unlock(&user_pool->lock); //unlock
                continue;
            }
            else
            {

                FD_SET(tfd, &user_pool->fdset);
                user_pool->num++;
//                user_pool->fds[user_pool->num++] = tfd;
                user_pool->status[tfd] = ON;
                maxfd = MAX(maxfd, tfd);
                user_pool->fdToIp[tfd] = getpeerip(tfd);

                // PRTNT_IP_STR(getpeerip(tfd));
                Pthread_mutex_unlock(&user_pool->lock); //unlock

                //broadcast to others
                MSG msg;
                memset(&msg, 0, sizeof(MSG));
                msg.msgType = PARTNER_JOIN;
                msg.ptr = NULL;
                msg.len = 0;
                msg.targetfd = tfd;
                msg.ip = user_pool->fdToIp[tfd];
                sendqueue.push_msg(msg);

                //broadcast to others
                MSG msg1;
                memset(&msg1, 0, sizeof(MSG));
                msg1.msgType = PARTNER_JOIN2;
                msg1.targetfd = tfd;
                int size = user_pool->num * sizeof(uint32_t);

                msg1.ptr = (char *)malloc(size);
                int pos = 0;

                for(int i = 0; i <= maxfd; i++)
                {
                    if(user_pool->status[i] == ON && i != tfd)
                    {
                        uint32_t ip = user_pool->fdToIp[i];
                        memcpy(msg1.ptr + pos, &ip, sizeof(uint32_t));
                        pos += sizeof(uint32_t);
                        msg1.len += sizeof(uint32_t);
                    }
                }
                PRTNT_IP_STR(msg.ip);
                sendqueue.push_msg(msg1);
            }
        }
    }
    return NULL;
}

void *send_func(void *arg)
{
    Pthread_detach(pthread_self());
    char * sendbuf = (char *)malloc(4 * MB);
    /*
     * $_msgType_ip_size_data_#
    */

    for(;;)
    {
        memset(sendbuf, 0, 4 * MB);
        MSG msg = sendqueue.pop_msg();
        int len = 0;

        sendbuf[len++] = '$';
        short type = htons((short)msg.msgType);
        memcpy(sendbuf + len, &type, sizeof(short)); //msgtype
        len+=2;

        if(msg.msgType == CREATE_MEETING_RESPONSE || msg.msgType == PARTNER_JOIN2)
        {
            len += 4;
        }
        else if(msg.msgType == TEXT_RECV || msg.msgType == PARTNER_EXIT || msg.msgType == PARTNER_JOIN || msg.msgType == IMG_RECV || msg.msgType == AUDIO_RECV || msg.msgType == CLOSE_CAMERA)
        {
            memcpy(sendbuf + len, &msg.ip, sizeof(uint32_t));
            len+=4;
        }

        int msglen = htonl(msg.len);
        memcpy(sendbuf + len, &msglen, sizeof(int));
        len += 4;
        memcpy(sendbuf + len, msg.ptr, msg.len);
        len += msg.len;
        sendbuf[len++] = '#';
        PRTNT_CONTEXT_HEX(sendbuf,len)
		Pthread_mutex_lock(&user_pool->lock);

        if(msg.msgType == CREATE_MEETING_RESPONSE)
        {
            if(writen(msg.targetfd, sendbuf, len) < 0)
            {
                MSG_ERR("writen error");
            }
        }
        else if(msg.msgType == PARTNER_EXIT || msg.msgType == IMG_RECV || msg.msgType == AUDIO_RECV || msg.msgType == TEXT_RECV || msg.msgType == CLOSE_CAMERA)
        {
            for(int i = 0; i <= maxfd; i++)
            {
                if(user_pool->status[i] == ON && msg.targetfd != i)
                {
                    if(writen(i, sendbuf, len) < 0)
                    {
                        MSG_ERR("writen error");
                    }
                }
            }
        }
        else if(msg.msgType == PARTNER_JOIN)
        {
            for(int i = 0; i <= maxfd; i++)
            {
                if(user_pool->status[i] == ON && i != msg.targetfd)
                {
                    if(writen(i, sendbuf, len) < 0)
                    {
                        MSG_ERR("writen error");
                    }
                }
            }
        }
        else if(msg.msgType == PARTNER_JOIN2)
        {
            for(int i = 0; i <= maxfd; i++)
            {
                if(user_pool->status[i] == ON && i == msg.targetfd)
                {
                    if(writen(i, sendbuf, len) < 0)
                    {
                        MSG_ERR("writen error");
                    }
                }
            }
        }

		Pthread_mutex_unlock(&user_pool->lock);

        //free
        if(msg.ptr)
        {
            free(msg.ptr);
            msg.ptr = NULL;
        }
    }
	free(sendbuf);

    return NULL;
}
