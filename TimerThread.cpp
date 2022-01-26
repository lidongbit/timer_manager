#include "TimerThread.h"
#include <stdio.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
void *TimerThreadBackRunning(void *arg)  
{ 
    int nfds, i;  
    uint64_t u64;  
    char buf[128];  
    struct epoll_event event;  
    timer_thread_item_t *timer_node = NULL;  
	
	TimerThread* tmp = (TimerThread*)(arg);
    timer_thread_obj_t tt = tmp->tt;
    struct epoll_event events[tt.timer_max];  
	
    pthread_detach(pthread_self());
    event.events = EPOLLIN | EPOLLET;  
    printf("Info: timer thread is running.\n");  
    while(tt.timer_active_flag)  
    {  
        nfds = epoll_wait(tt.timer_epoll_fd, events, tt.timer_max, -1);  
        if(nfds <= 0)  
            continue;  
        for(i = 0; i < nfds; i++)  
        {  
            timer_node = (timer_thread_item_t *)events[i].data.ptr;  
            if(NULL == timer_node)  
            {  
                read(tt.timer_event_fd, &u64, sizeof(uint64_t));  
                continue;  
            }  
			
            while(read(timer_node->timer_fd, buf, sizeof(buf)) > 0);  
            if(NULL == timer_node->timer_cb)  
                continue;  
			
            if(-1 == timer_node->timer_cnt)  
                timer_node->timer_cb(timer_node->timer_data);  
            else  
            {  
                if(timer_node->timer_cnt)  
                {  
                    timer_node->timer_cb(timer_node->timer_data);  
                    timer_node->timer_cnt--;  
                }  
                else  
                {  
                    event.data.ptr = (void *)timer_node;  
                    epoll_ctl(tt.timer_epoll_fd, EPOLL_CTL_DEL, timer_node->timer_fd, &event);  
                    pthread_rwlock_wrlock(&tt.timer_rwlock);  
                    //HASH_DEL(tt.timer_head, timer_node);  
                    for(std::list<timer_thread_item_t*>::iterator it=tt.timer_head.begin();it!=tt.timer_head.end();it++)
                    {
                        if((*it)->timer_fd==timer_node->timer_fd){
                            tt.timer_head.erase(it);
                        }
                    }
                    pthread_rwlock_unlock(&tt.timer_rwlock);  
                    close(timer_node->timer_fd);  
                    if(timer_node->release_cb)  
                        timer_node->release_cb(timer_node->timer_data);  
                    free(timer_node);  
                }  
            }  
        }  
    }  
    printf("Info: timer thread is exit.\n");  
    pthread_exit(NULL);  
}

TimerThread::TimerThread()
{
	tt.timer_max = 0;
    tt.timer_epoll_fd = -1;
    tt.timer_event_fd = -1;
    tt.timer_active_flag= false;
    
    tt.timer_thread_id = 0;
    tt.timer_rwlock = PTHREAD_RWLOCK_INITIALIZER;
}
TimerThread::~TimerThread()
{
	uint64_t quit = 0x51;
    if(false == tt.timer_active_flag)
        return ;
    tt.timer_active_flag = false;
    usleep(100);
    write(tt.timer_event_fd, &quit, sizeof(uint64_t)); /* wakeup thread */
    //pthread_join(tt.timer_thread_id, NULL);
    printf(">>>>>>>>>>>>>>>>>>>>>5<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    TimerThreadClear();
    close(tt.timer_epoll_fd);
    close(tt.timer_event_fd);
    tt.timer_epoll_fd = -1;
    tt.timer_event_fd = -1;
}

int TimerThread::TimerThreadInit(int max_num)
{
	struct epoll_event event;
  
    if(true == tt.timer_active_flag)
        return 0;

    tt.timer_event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if(-1 == tt.timer_event_fd)
    {
        printf("error: eventfd() failed, %s!\n", strerror(errno));
        return -1;
    }
    tt.timer_max = max_num;
    tt.timer_active_flag = true;
    tt.timer_epoll_fd = epoll_create(max_num);
    if(tt.timer_epoll_fd < 0)
    {
        printf("error: epoll_create failed %s.\n", strerror(errno));
        close(tt.timer_event_fd);
        tt.timer_event_fd = -1;
        return -1;
    }

    event.data.ptr = NULL;
    event.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(tt.timer_epoll_fd, EPOLL_CTL_ADD, tt.timer_event_fd, &event) < 0)
    {
        fprintf(stderr, "epoll: epoll_ctl ADD failed, %s\n", strerror(errno));
        close(tt.timer_epoll_fd);
        close(tt.timer_event_fd);
        tt.timer_epoll_fd = -1;
        tt.timer_event_fd = -1;
        return -1;
    }

    if(pthread_create(&tt.timer_thread_id, NULL, TimerThreadBackRunning, this) != 0)
    {
        printf("error: pthread_create failed %s.\n", strerror(errno));
        close(tt.timer_epoll_fd);
        close(tt.timer_event_fd);
        tt.timer_epoll_fd = -1;
        tt.timer_event_fd = -1;
        return -1;
    }
    
    return 0;
}

int TimerThread::TimerThreadAdd(int ms, int repeat, timer_callback_t cb, void *data, timer_release_t rb)
{
	uint64_t quit = 0x51;
    struct epoll_event event;
    timer_thread_item_t *handler = NULL;
  	struct itimerspec itimespec;
	
    itimespec.it_value.tv_sec = 0;
    itimespec.it_value.tv_nsec = ms*1000000;
    itimespec.it_interval.tv_sec = 0;
    itimespec.it_interval.tv_nsec = ms*1000000;
    handler = new timer_thread_item_t;
    if(NULL == handler)
    {
        printf("error: malloc failed %s.\n", strerror(errno));
        return -1;
    }
    
    handler->timer_cb = cb;
    handler->release_cb = rb;
    handler->timer_cnt = repeat;
    handler->timer_data = &tt.timer_epoll_fd;
    handler->timer_fd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC|TFD_NONBLOCK);
    if(handler->timer_fd < 0)
    {
        printf("error: timerfd_create failed %s.\n", strerror(errno));
        free(handler);
        return -2;
    }
    if(timerfd_settime(handler->timer_fd, 0, &itimespec, NULL) == -1)
    {
        printf("error: timerfd_settime failed %s.\n", strerror(errno));
        close(handler->timer_fd);
        free(handler);
        return -3;
    }

    event.events = EPOLLIN | EPOLLET;
    event.data.ptr = handler;
    if(epoll_ctl(tt.timer_epoll_fd, EPOLL_CTL_ADD, handler->timer_fd, &event) < 0)
    {
        printf("epoll_ctl ADD failed %s.\n", strerror(errno));
        close(handler->timer_fd);
        free(handler);
        return -4;
    }
    write(tt.timer_event_fd, &quit, sizeof(uint64_t)); /* wakeup thread */
    pthread_rwlock_wrlock(&tt.timer_rwlock);
    //HASH_ADD_INT(tt.timer_head, timer_fd, handler);
    tt.timer_head.push_back(handler);
    pthread_rwlock_unlock(&tt.timer_rwlock);
    
    return handler->timer_fd;
}

int TimerThread::TimerThreadDel(int timerfd)
{
	struct epoll_event event;
    timer_thread_item_t *handler = NULL;

    pthread_rwlock_rdlock(&tt.timer_rwlock);
    //HASH_FIND_INT(tt.timer_head, &timerfd, handler);
    for (std::list<timer_thread_item_t*>::iterator it = tt.timer_head.begin(); it != tt.timer_head.end(); ++it) 
    {
        if((*it)->timer_fd == timerfd)
        {
            handler = *it;
            printf(">>>>>>>>>>>>>>>>>>>>>1<<<<<<<<<<<<<<<<<<<<<<<<<\n");
        }
    }
    pthread_rwlock_unlock(&tt.timer_rwlock);
    if(NULL == handler)
        return 0;
    
    event.data.ptr = (void *)handler;
    event.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(tt.timer_epoll_fd, EPOLL_CTL_DEL, handler->timer_fd, &event) < 0)
    {
        printf("error: epoll_ctl DEL failed %s.\n", strerror(errno));
        return -1;
    }

    close(handler->timer_fd);
    if(handler->release_cb)
        handler->release_cb(handler->timer_data);
        printf(">>>>>>>>>>>>>>>>>>>>>2<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    pthread_rwlock_wrlock(&tt.timer_rwlock);
    //HASH_DEL(tt.timer_head, handler);
    for (std::list<timer_thread_item_t*>::iterator it = tt.timer_head.begin(); it != tt.timer_head.end(); ++it) 
    {
        if((*it)->timer_fd == timerfd)
        {
            delete (*it);
            tt.timer_head.erase(it);
            printf(">>>>>>>>>>>>>>>>>>>>>3<<<<<<<<<<<<<<<<<<<<<<<<<\n");
            break;
        }
    }
    printf(">>>>>>>>>>>>>>>>>>>>>4<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    pthread_rwlock_unlock(&tt.timer_rwlock);

    return 0;
}

int TimerThread::TimerThreadClear()
{
	struct epoll_event event;
    //timer_thread_item_t *handler = NULL;
    
    event.events = EPOLLIN | EPOLLET;
    for (std::list<timer_thread_item_t*>::iterator it = tt.timer_head.begin(); it != tt.timer_head.end(); ++it) 
    {
        event.data.ptr = (void *)(*it);
        if(epoll_ctl(tt.timer_epoll_fd, EPOLL_CTL_DEL, (*it)->timer_fd, &event) < 0)
        {
            printf("error: epoll_ctl CLEAR failed %s.\n", strerror(errno));
            return -1;
        }
        close((*it)->timer_fd);
        if((*it)->release_cb)
            (*it)->release_cb((*it)->timer_data);
        pthread_rwlock_wrlock(&tt.timer_rwlock);
        //HASH_DEL(tt.timer_head, handler);
        tt.timer_head.erase(it);
        //printf(">>>>>>>>>>>>>>>>>>>>>5<<<<<<<<<<<<<<<<<<<<<<<<<\n");
        pthread_rwlock_unlock(&tt.timer_rwlock);
    }
    return 0;
}

int TimerThread::TimerThreadCount(void)
{
	return tt.timer_head.size();
}

int TimerThread::TimerThreadEventfd_Get()
{
    return tt.timer_event_fd;
}