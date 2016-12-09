#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sstream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstdio>
#include <sys/wait.h>
#include <cstdlib>
#include <pthread.h>

using namespace std;

string locks_shared_name("locks_shared");
string blockeds_shared_name("blockeds_shared");
string mutexes_shared_name("mutexes_shared");
string conds_shared_name("conds_shared");
string watches_shared_name("watches_shared");



key_t key_locks=1;
key_t key_blockeds=2;
key_t key_mutexes=3;
key_t key_conds=4;
key_t key_watches=5;

int sock;
int client;
int wait_status;
char buf[1024];
int read_count;

int lock_id=1;
int blocked_id=1;
int watch_id=1;

pthread_mutex_t locks_lock;


typedef struct mystruct1
{
  pid_t pid;
  int id;
  double xoff,yoff,width,height;
  char type ;
} lock;

typedef struct mystruct2
{
  pid_t pid;
  int id;
  double xoff,yoff,width,height;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} blocked;

typedef struct mystruct3
{
  pid_t pid;
  int index;
  int id;
  double xoff,yoff,width,height;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  char type;
  double e_xoff,e_yoff,e_width,e_height;
  char e_type;

} watch;

lock *locks_mem;
blocked *blockeds_mem;
watch *watches_mem;

pthread_mutex_t *mutexes_mem;
pthread_cond_t *conds_mem;

pthread_mutex_t *locks_mutex;
pthread_cond_t *locks_read;
pthread_cond_t *locks_write;

pthread_mutex_t *blockeds_mutex;
pthread_cond_t *blockeds_read;
pthread_cond_t *blockeds_write;


void create_shared_memorys()
{
  FILE *f;

  int locks_id;
  int blockeds_id;
  int mutexes_id;
  int conds_id;
  int watches_id;





  f = fopen(locks_shared_name.c_str(),"w");
  fclose(f);
  f = fopen(blockeds_shared_name.c_str(),"w");
  fclose(f);
  f = fopen(mutexes_shared_name.c_str(),"w");
  fclose(f);
  f = fopen(conds_shared_name.c_str(),"w");
  fclose(f);
  f = fopen(watches_shared_name.c_str(),"w");
  fclose(f);



  key_locks = ftok(locks_shared_name.c_str(), key_locks);
  key_blockeds = ftok(blockeds_shared_name.c_str(), key_blockeds);
  key_mutexes = ftok(mutexes_shared_name.c_str(), key_mutexes);
  key_conds = ftok(conds_shared_name.c_str(), key_conds);
  key_watches = ftok(watches_shared_name.c_str(), key_watches);


  locks_id= shmget(key_locks, sizeof(lock)*100000, 0777 | IPC_CREAT);
  locks_mem = (lock*)shmat(locks_id,NULL,0);
  memset(locks_mem,0,sizeof(lock)*100000);

  blockeds_id= shmget(key_blockeds, sizeof(blocked)*100000, 0777 | IPC_CREAT);
  blockeds_mem = (blocked*)shmat(blockeds_id,NULL,0);
  memset(blockeds_mem,0,sizeof(blocked)*100000);

  mutexes_id= shmget(key_mutexes, sizeof(pthread_mutex_t)*100000, 0777 | IPC_CREAT);
  mutexes_mem = (pthread_mutex_t*)shmat(mutexes_id,NULL,0);
  memset(mutexes_mem,0,sizeof(pthread_mutex_t)*100000);

  conds_id= shmget(key_conds, sizeof(pthread_cond_t)*100000, 0777 | IPC_CREAT);
  conds_mem = (pthread_cond_t*)shmat(conds_id,NULL,0);
  memset(conds_mem,0,sizeof(pthread_cond_t)*100000);

  watches_id = shmget(key_watches, sizeof(watch)*100000, 0777 | IPC_CREAT);
  watches_mem = (watch*)shmat(watches_id,NULL,0);
  memset(watches_mem,0,sizeof(watch)*100000);

}

void create_socket(char *path)
{
  struct sockaddr_un server;

  sock = socket(AF_UNIX, SOCK_STREAM, 0);
  server.sun_family = AF_UNIX;
  strcpy(server.sun_path, path);
  bind(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_un));
  listen(sock, 1);
}

void create_mutexes_and_conds()
{
  pthread_mutexattr_t locks_mutex_attr;
  pthread_mutexattr_t blockeds_mutex_attr;

  pthread_condattr_t locks_read_attr;
  pthread_condattr_t locks_write_attr;
  pthread_condattr_t blockeds_read_attr;
  pthread_condattr_t blockeds_write_attr;

  locks_mutex = &(mutexes_mem[0]);
  blockeds_mutex = &(mutexes_mem[1]);
  locks_read = &(conds_mem[0]);
  locks_write= &(conds_mem[1]);
  blockeds_read = &(conds_mem[2]);
  blockeds_write = &(conds_mem[3]);



  pthread_mutexattr_init(&locks_mutex_attr);
  pthread_mutexattr_setpshared(&locks_mutex_attr,PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(locks_mutex, &locks_mutex_attr);

  pthread_mutexattr_init(&blockeds_mutex_attr);
  pthread_mutexattr_setpshared(&blockeds_mutex_attr,PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(blockeds_mutex, &blockeds_mutex_attr);

  pthread_condattr_init(&locks_read_attr);
  pthread_condattr_setpshared(&locks_read_attr, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(locks_read, &locks_read_attr);

  pthread_condattr_init(&locks_write_attr);
  pthread_condattr_setpshared(&locks_write_attr, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(locks_write, &locks_write_attr);

  pthread_condattr_init(&blockeds_read_attr);
  pthread_condattr_setpshared(&blockeds_read_attr, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(blockeds_read, &blockeds_read_attr);

  pthread_condattr_init(&blockeds_write_attr);
  pthread_condattr_setpshared(&blockeds_write_attr, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(blockeds_write, &blockeds_write_attr);


}

bool intersects(lock l, double xoff,double yoff,double width,double height)
{
  return !(   (l.xoff+width<xoff)
          &&  (xoff+width<l.xoff)
          &&  (l.yoff+height<yoff)
          &&  (yoff+height<l.yoff));
}

void wait_blocked(double xoff,double yoff,double width,double height, char type,pid_t pid)
{
  for(int i=0;i<100000;i++)
  {
    if( !(blockeds_mem[i].id) )
    {
      pthread_mutexattr_t mutex_attr;
      pthread_condattr_t cond_attr;

      pthread_mutexattr_init(&mutex_attr);
      pthread_mutexattr_setpshared(&mutex_attr,PTHREAD_PROCESS_SHARED);
      pthread_mutex_init(&(blockeds_mem[i].mutex), &mutex_attr);

      pthread_condattr_init(&cond_attr);
      pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
      pthread_cond_init(&(blockeds_mem[i].cond), &cond_attr);

      blockeds_mem[i].id = blocked_id++;
      blockeds_mem[i].xoff = xoff;
      blockeds_mem[i].yoff = yoff;
      blockeds_mem[i].width = width;
      blockeds_mem[i].height = height;

      pthread_mutex_lock(&(blockeds_mem[i].mutex));
      pthread_mutex_unlock(locks_mutex);
      pthread_cond_wait(&(blockeds_mem[i].cond),&(blockeds_mem[i].mutex));
      blockeds_mem[i].id = blocked_id = 0;
      pthread_mutex_unlock(&(blockeds_mem[i].mutex));
      return;
    }
  }
}


int lock_it(double xoff,double yoff,double width,double height, char type,pid_t pid, bool try_lock)
{

  pthread_mutex_lock(locks_mutex);

  for(int i=0;i<100000;i++)
  {
    if(locks_mem[i].id && (type == 'W' || locks_mem[i].type=='W' )&& intersects(locks_mem[i],xoff,yoff,width,height) )
    {
      if(!try_lock )
      {
        wait_blocked(xoff,yoff,width,height,type,pid);
        return lock_it(xoff,yoff,width,height,type,pid,try_lock);
      }
      else
      {
        pthread_mutex_unlock(locks_mutex);
        return -1;
      }

    }
  }

  for(int i=0;i<100000;i++)
  {
    if( !(locks_mem[i].id) )
    {
      locks_mem[i].id = lock_id++;
      locks_mem[i].pid = pid;
      locks_mem[i].type = type;
      locks_mem[i].xoff = xoff;
      locks_mem[i].yoff = yoff;
      locks_mem[i].width = width;
      locks_mem[i].height = height;
      for(int j=0;j<100000;j++)
      {
        pthread_mutex_lock(&(watches_mem[j].mutex));
        if( intersects( locks_mem[i], watches_mem[j].xoff,watches_mem[j].yoff,watches_mem[j].width,watches_mem[j].height ) )
        {


          watches_mem[j].type = 'L';
          watches_mem[j].e_xoff = locks_mem[i].xoff;
          watches_mem[j].e_yoff = locks_mem[i].yoff;
          watches_mem[j].e_width = locks_mem[i].width;
          watches_mem[j].e_height = locks_mem[i].height;
          watches_mem[j].e_type = locks_mem[i].type;
          pthread_cond_signal(&(watches_mem[j].cond));

        }
        pthread_mutex_unlock(&(watches_mem[j].mutex));

      }
      break;
    }
  }

  pthread_mutex_unlock(locks_mutex);

  return lock_id-1;
}

bool unlock_it(int id,pid_t pid)
{
  pthread_mutex_lock(locks_mutex);
  for(int i=0;i<100000;i++)
  {
    if( locks_mem[i].id == id && locks_mem[i].pid == pid )
    {
      locks_mem[i].id = 0;

      for(int j=0;j<100000;j++)
      {
        if( !(blockeds_mem[j].id))
          continue;
        pthread_mutex_lock(&(blockeds_mem[j].mutex));
        if( intersects(locks_mem[i],blockeds_mem[j].xoff,blockeds_mem[j].yoff,blockeds_mem[j].width,blockeds_mem[j].height ) )
        {
          pthread_mutex_unlock(locks_mutex);
          pthread_cond_signal(&(blockeds_mem[j].cond));
          pthread_mutex_unlock(&(blockeds_mem[j].mutex));
        }
        pthread_mutex_unlock(&(blockeds_mem[j].mutex));
      }
      for(int j=0;j<100000;j++)
      {
        pthread_mutex_lock(&(watches_mem[j].mutex));
        if(watches_mem[j].id && intersects( locks_mem[i], watches_mem[j].xoff,watches_mem[j].yoff,watches_mem[j].width,watches_mem[j].height ) )
        {

          watches_mem[j].type = 'U';
          watches_mem[j].e_xoff = locks_mem[i].xoff;
          watches_mem[j].e_yoff = locks_mem[i].yoff;
          watches_mem[j].e_width = locks_mem[i].width;
          watches_mem[j].e_height = locks_mem[i].height;
          watches_mem[j].e_type = locks_mem[i].type;
          pthread_cond_signal(&(watches_mem[j].cond));

        }
        pthread_mutex_unlock(&(watches_mem[j].mutex));

      }

      return true;
    }
  }
  pthread_mutex_unlock(locks_mutex);
  return false;
}

void mylocks(pid_t pid)
{
  pthread_mutex_lock(locks_mutex);
  for(int i=0;i<100000;i++)
  {
    if( locks_mem[i].pid == pid )
    {
      ostringstream temp;
      temp  << locks_mem[i].id << " " << locks_mem[i].type << " " << locks_mem[i].xoff << " " << locks_mem[i].yoff << " "
            <<locks_mem[i].width << " " << locks_mem[i].height;
      write(client,(temp.str()+'\n').c_str(),(temp.str()+'\n').size());
    }
  }
  pthread_mutex_unlock(locks_mutex);
}
void getlocks(double xoff, double yoff, double width, double height)
{
  pthread_mutex_lock(locks_mutex);
  for(int i=0;i<100000;i++)
  {
    if( locks_mem[i].id && intersects(locks_mem[i],xoff,yoff,width,height) )
    {
      ostringstream temp;
      temp  << locks_mem[i].type << " " << locks_mem[i].xoff << " " << locks_mem[i].yoff << " "
            <<locks_mem[i].width << " " << locks_mem[i].height;
      write(client,(temp.str()+'\n').c_str(),(temp.str()+'\n').size());
    }
  }
  pthread_mutex_unlock(locks_mutex);
}

void *watch_thread(void *params)
{
  int my_watch_id;
  my_watch_id = *((int *)params);
  pthread_mutex_lock(&(watches_mem[my_watch_id].mutex));
  while(1)
  {
    pthread_cond_wait(&(watches_mem[my_watch_id].cond),&(watches_mem[my_watch_id].mutex));

    if( watches_mem[my_watch_id].type == 'D' )
    {
      watches_mem[my_watch_id].id = 0;
      break;
    }
    else if( watches_mem[my_watch_id].type != 'F' )
    {
      ostringstream temp;
      temp  << "Watch " << watches_mem[my_watch_id].id << " " << watches_mem[my_watch_id].e_xoff << " " << watches_mem[my_watch_id].e_yoff << " "
            << watches_mem[my_watch_id].e_width << " " << watches_mem[my_watch_id].e_height <<  " " << watches_mem[my_watch_id].e_type << " ";
      if( watches_mem[my_watch_id].type == 'L' )
        temp << "locked";
      else
        temp << "unlocked";
      write(client,(temp.str()+'\n').c_str(),(temp.str()+'\n').size());
      watches_mem[my_watch_id].type = 'F';
    }

  }
  pthread_mutex_unlock(&(watches_mem[my_watch_id].mutex));
  return NULL;
}

int create_watch_thread(pid_t pid, double xoff, double yoff, double width, double height)
{
  pthread_t tid;

  for(int i=0;i<100000;i++)
  {
    if( !(watches_mem[i].id) )
    {
      int my_watch_id = watch_id++;
      watches_mem[i].index = i;
      watches_mem[i].pid = pid;
      watches_mem[i].id = my_watch_id;
      watches_mem[i].xoff = xoff;
      watches_mem[i].yoff = yoff;
      watches_mem[i].width = width;
      watches_mem[i].height = height;
      watches_mem[i].type != 'F';

      pthread_create(&(tid), NULL, &watch_thread, (void*)(&(watches_mem[i].index)));
      return my_watch_id;
    }
  }

}

void print_watches(pid_t pid)
{
  for(int i=0;i<100000;i++)
  {
    if( watches_mem[i].pid == pid && watches_mem[i].id )
    {
      ostringstream temp;

      temp  << watches_mem[i].id << " " << watches_mem[i].xoff << " " << watches_mem[i].yoff << " "
            << watches_mem[i].width << " " << watches_mem[i].height ;
      write(client,(temp.str()+'\n').c_str(),(temp.str()+'\n').size());
    }

  }
}

bool unwatch_it(int id, pid_t pid)
{
  for(int i=0;i<100000;i++)
  {
    pthread_mutex_lock(&(watches_mem[i].mutex));
    if(  watches_mem[i].pid == pid &&  watches_mem[i].id == id )
    {
      watches_mem[i].type = 'D';
      pthread_cond_signal(&(watches_mem[i].cond));
      pthread_mutex_unlock(&(watches_mem[i].mutex));
      return true;
    }
    pthread_mutex_unlock(&(watches_mem[i].mutex));
  }
  return false;
}


void agent( pid_t pid)
{
  string command;
  double xoff;
  double yoff;
  double width;
  double height;

  int id;



  while((read_count=read(client, buf, 1024)))
  {
    buf[read_count] = 0;
    string sbuf(buf);
    istringstream iss(sbuf);
    iss >> command;
    if( command == "LOCKR" )
    {
      iss >> xoff >> yoff >> width >> height;

      ostringstream temp;
      temp << lock_it(xoff,yoff,width,height,'R',pid,false);
      write(client,(temp.str()+'\n').c_str(),(temp.str()+'\n').size());
    }
    else if( command == "LOCKW" )
    {
      iss >> xoff >> yoff >> width >> height;

      ostringstream temp;
      temp << lock_it(xoff,yoff,width,height,'W',pid,false);
      write(client,(temp.str()+'\n').c_str(),(temp.str()+'\n').size());
    }
    else if( command == "UNLOCK" )
    {
      iss >> id;
      string resp;
      if(unlock_it(id,pid))
      {
        resp = "Ok";
      }
      else
      {
        resp = "Failed";
      }
      write(client,(resp+'\n').c_str(),(resp+'\n').size());

    }
    else if( command == "TRYLOCKR" )
    {
      iss >> xoff >> yoff >> width >> height;

      ostringstream temp;
      int esrefesref = lock_it(xoff,yoff,width,height,'R',pid,true);
      if(esrefesref>0)
      {
        temp << esrefesref;
      }
      else
      {
        temp << "Failed";
      }
      write(client,(temp.str()+'\n').c_str(),(temp.str()+'\n').size());
    }
    else if( command == "TRYLOCKW" )
    {
      iss >> xoff >> yoff >> width >> height;

      ostringstream temp;
      int esrefesref = lock_it(xoff,yoff,width,height,'W',pid,true);
      if(esrefesref>0)
      {
        temp << esrefesref;
      }
      else
      {
        temp << "Failed";
      }
      write(client,(temp.str()+'\n').c_str(),(temp.str()+'\n').size());
    }
    else if( command == "MYLOCKS" )
    {
      mylocks(pid);
    }
    else if( command == "GETLOCKS" )
    {
      iss >> xoff >> yoff >> width >> height;
      getlocks(xoff,yoff,width,height);
    }
    else if( command == "WATCH" )
    {
      iss >> xoff >> yoff >> width >> height;

      ostringstream temp;
      temp << create_watch_thread(pid,xoff,yoff,width,height);
      write(client,(temp.str()+'\n').c_str(),(temp.str()+'\n').size());

    }
    else if( command == "UNWATCH" )
    {
      iss >> id;
      string resp;
      if(unwatch_it(id,pid))
      {
        resp = "Ok";
      }
      else
      {
        resp = "Failed";
      }
      write(client,(resp+'\n').c_str(),(resp+'\n').size());


    }
    else if( command == "WATCHES" )
    {

      print_watches(pid);
    }
    else if(command == "BYE")
    {
      for(int i=1;i<=lock_id;i++)
      {
        unlock_it(i,pid);
      }
      for(int i=1;i<=watch_id;i++)
      {
        unwatch_it(i,pid);
      }

      close(client);
      exit(0);
    }
    buf[0]  =0 ;

  }
}


int main(int argc, char **argv)
{
  pid_t pid;

  if(argc<2)
  {
    printf("GIVE ME SOCKET!\n");
    return 1;
  }
  create_shared_memorys();
  create_socket(argv[1]);
  create_mutexes_and_conds();

  while((client = accept(sock, 0, 0)) ) if((pid=fork()))
  {
    agent(pid);
  }

//  while(wait(&wait_status)>0);
  return 0;

}
