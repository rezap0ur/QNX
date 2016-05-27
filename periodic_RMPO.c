#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/netmgr.h>
#include <time.h>

#define SEC_NSEC	1000000000LL	// 1 second
#define MSEC_NSEC 	1000000LL		// 1 millisecond
#define TIMER_PERIOD_NS	999847LL	//
#define MY_PULSE_CODE   _PULSE_CODE_MINAVAIL
#define N_TASKS 5
#define TOP_PRIORITY 255

static int C[N_TASKS]={5,1,1,2,2};
static int T[N_TASKS]={10,20,30,40,25};
static int Phi[N_TASKS]={10,10,10,10,10};
static int P[N_TASKS]={100,100,100,100,100};
static int instances[N_TASKS]={5,20,40,5,10};

void f_tao(void*);
void show_taskset(void);
int rmpo(void);

long int counter[N_TASKS]={0};

/* Liu-Layland bounds */
static const double Ulub[11]={0.69, // for more than 10 tasks
 1.000,0.828,0.780,0.757,0.743,0.735,0.279,0.724,0.721,0.718}; // for 1..10 tasks


typedef union {
        struct _pulse   pulse;

} my_message_t;

struct sigevent         event_timer[N_TASKS];
struct itimerspec       itime[N_TASKS];
timer_t                 timer_id[N_TASKS];

struct thread_info{
	int						T,P,Phi,instances,C;
	pthread_t id;
	struct sched_param 		params;
	pthread_attr_t    		attr;
	int 					policy;
	int                     chid;
	int                     rcvid;
	my_message_t            msg;
	long int 				counter;
	unsigned long long int  Df,Dt;
	struct timespec 		start, stop;

}thread[N_TASKS];

void f_tao(void* arg){
	int i;
	struct thread_info *this;
	this = (struct thread_info*)arg;

	this->Dt=MSEC_NSEC*this->T;   // copy T[i] in Dt to check deadline time later
								// get start time of task (Phi is also included)
	if( clock_gettime( CLOCK_REALTIME, &this->start) == -1 ) {
			  perror( "clock gettime" );
			  exit(0);
	}
	for (i=0;i<this->instances;i++) {
	this->rcvid = MsgReceive(this->chid, &this->msg, sizeof(my_message_t), NULL);

	                nanospin_ns(MSEC_NSEC*this->C);
					// get finish time of each period
	                if( clock_gettime( CLOCK_REALTIME, &this->stop) == -1 ) {
					  perror( "clock gettime" );
					  exit(0);
					}
					// check if deadline missed

	                this->Df=(this->stop.tv_sec - this->start.tv_sec )*SEC_NSEC+( this->stop.tv_nsec - this->start.tv_nsec );
	                if(i==0) // depend on consideration to Phi for missed deadlines,this line could be changed
	                	this->Df-=(MSEC_NSEC*this->Phi);
	                printf("tid %d[%d] finished time: %llu ns\n",pthread_self(),i,this->Df);
	                if(this->Df>this->Dt){
	          			printf( "deadline missed in %dth run of tid %d \n", i+1,pthread_self());
					}
	                this->Dt+=(MSEC_NSEC*this->T); // adding T[i] for next iteration
				    this->counter++;

	 }
	printf("---> task %d has finished after %ld instances\n",pthread_self(),this->counter);
}

int
main (void) // ignore arguments
{int i;
// 	copy taskset properties
	for(i=0;i<N_TASKS;i++){
		memcpy(&thread[i].C, &C[i], sizeof(int));
		memcpy(&thread[i].instances, &instances[i], sizeof(int));
		memcpy(&thread[i].T, &T[i], sizeof(int));
		memcpy(&thread[i].Phi, &Phi[i], sizeof(int));
		memcpy(&thread[i].counter, &counter[i], sizeof(int));
	}

	int disable=0; 	// disable interrupts
    nanospin_calibrate(disable);

    // Initialize Timers

    for(i=0;i<N_TASKS;i++){
    	  thread[i].chid = ChannelCreate(0);

          event_timer[i].sigev_notify = SIGEV_PULSE;
          event_timer[i].sigev_coid = ConnectAttach(ND_LOCAL_NODE, 0,
                                           thread[i].chid,
                                           _NTO_SIDE_CHANNEL, 0);
          event_timer[i].sigev_priority = getprio(0);  // priority of the calling thread
          event_timer[i].sigev_code = MY_PULSE_CODE;
          timer_create(CLOCK_REALTIME, &event_timer[i], &timer_id[i]);

          itime[i].it_value.tv_sec = 0;
          itime[i].it_value.tv_nsec = thread[i].Phi*MSEC_NSEC;
          itime[i].it_interval.tv_sec = 0;
          itime[i].it_interval.tv_nsec = thread[i].T*MSEC_NSEC;
          timer_settime(timer_id[i], 0, &itime[i], NULL);
    }

// Guarantee test by Rate Monotonic Period Ordering
int guaranteed;
guaranteed=rmpo();
if(guaranteed==0){
			printf("Press enter to continue or 'e' for exit!");
			char ch=getchar();
			if(ch=='e')
				exit(0);
		}

printf(" Running %d Tasks guaranteed\n", guaranteed);
show_taskset();

// assign attributes for scheduling
int        ret;
for(i=0;i<N_TASKS;i++){
	pthread_attr_init(&thread[i].attr);
	ret = pthread_attr_setinheritsched(&thread[i].attr,
			 PTHREAD_EXPLICIT_SCHED);
	if(ret != 0) {
		printf("pthread_attr_setinheritsched() failed %d \n",
			   errno);
		return 1;
	}

	ret = pthread_attr_setschedpolicy(&thread[i].attr, SCHED_FIFO);
	if(ret != 0) {
		printf("pthread_attr_setschedpolicy() failed %d %d\n",
			   ret, errno);
		return 1;
	}
	memcpy(&thread[i].params.sched_priority , &P[i],sizeof(int));  // set priority here (or highest prio)
	ret = pthread_attr_setschedparam(&thread[i].attr, &thread[i].params);
	if(ret != 0) {
		printf("pthread_attr_setschedparam() failed %d \n", errno);
		return 1;
	}
}


// starting tasks
for(i=0;i<N_TASKS;i++)
		pthread_create (&thread[i].id, &thread[i].attr, f_tao , &thread[i] );
// wait for tasks to finish
for(i=0;i<N_TASKS;i++)
		pthread_join(thread[i].id,NULL);
}


int rmpo(void) {

   int i,j,min,temp,test1,test2;
   test1=0;
   test2=0;
   double Ut=0;

   // GUARANTEE TESTS
   /* Liu % Lyland bound test*/
   for(i=0;i<N_TASKS;i++){
	   Ut+= (double)C[i]/(double)T[i];
   }
   	   if(Ut>Ulub[5]) { // **check LL bound!
   		printf("Task-set not guaranteed by Liu & Lyland Bound .\n");
   		test1=1;
    }
   	   Ut=1;
//   Hyperbolic Bound
   for(i=0;i<N_TASKS;i++){
   	   Ut*=(((double)C[i]/(double)T[i])+1) ;
   }
   if(Ut>2) { // **check HB bound!
	   printf("Task-set not guaranteed by Hyperbolic Bound .\n");
	   test2=1;
      }
   if(test1==1||test2==1){
	   return 0;
   }

   // SET PRIORITIES BASED ON RMPO
// set priorities based on period length

   int T1[N_TASKS];
   memcpy(T1,T,sizeof(T));
   // sort T1
   for(i=0;i<N_TASKS;i++) {
       min = i;
       for(j=i+1;j<N_TASKS;j++) {
           if(T1[j]<T1[min])
               min = j;
       }
       if(min!=i) {
           temp = T1[i];
           T1[i] = T1[min];
           T1[min] = temp;
       }
   }

   // assign priorities based on T1
   for(i=0;i<N_TASKS;i++) {
       for(j=0;j<N_TASKS;j++) {
           if(T[i]==T1[j]) {
               P[i]=TOP_PRIORITY-j;   //highest priority for shortest T
               break;
           }
       }
   }
   return N_TASKS; // guaranteed
}

void show_taskset(void) {
    int i;
    printf("\nTaskset:\n");
    for(i=0;i<N_TASKS;i++) {
    	printf("tau[%d]: T=%dms\tC=%dms\tU=%.2f\tP=%d\n",
                  i,
                  T[i],
                  C[i],
                  (double)C[i]/(double)T[i],
                  P[i]
                  );
    }
}

