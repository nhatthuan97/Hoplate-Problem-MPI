#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mpi.h"
#include <string.h>
#include <math.h>
#include <time.h>
double timestamp()
{
    struct timeval tv;

    gettimeofday( &tv, ( struct timezone * ) 0 );
    return ( tv.tv_sec + (tv.tv_usec / 1000000.0) );
}
int main(int argc, char *argv[]){
  if (argc<8){
    printf("Not enough arguments\n");
    exit(1);
  }
  int rc,numranks,myranks,num_rows,num_cols,standard_work,remains,moreworks;
  float local_max_epsilon,global_max_epsilon;
  num_rows=atoi(argv[1]);
  num_cols=atoi(argv[2]);


  MPI_Status status;
  rc = MPI_Init(NULL,NULL);
  rc = MPI_Comm_size(MPI_COMM_WORLD,&numranks);
  rc = MPI_Comm_rank(MPI_COMM_WORLD,&myranks);
  standard_work=num_rows/(numranks-1);
  remains=num_rows%(numranks-1);
  alarm(300);

  //Rank 0 section
  if (myranks==0){
    float top_temp=atof(argv[3]);
    float left_temp=atof(argv[4]);
    float right_temp=atof(argv[5]);
    float bot_temp=atof(argv[6]);
    float goal_max_epsilon = atof(argv[7]);
    float **original_array;
    original_array = (float **)malloc(num_rows * sizeof(float *));
    for (int i=0;i<num_rows;i++){
      original_array[i]=(float *)malloc(num_cols*sizeof(float));
    }
    //Initialized array top rows
    for (int i=0;i<num_cols;i++){
      original_array[0][i]=top_temp;
    }
    //Initialized lef cols
    for (int i=0;i<num_rows;i++){
      original_array[i][0]=left_temp;
    }
    //Right cols next
    for (int i=0;i<num_rows;i++){
      original_array[i][num_cols-1]=right_temp;
    }
    //Bot rows last
    for (int i=0;i<num_cols;i++){
      original_array[num_rows-1][i]=bot_temp;
    }
    //Calculate value to Initialized
    float sum=0;
    float edge_amt=0;
    for (int i=0;i<num_cols;i++){
      sum=original_array[0][i]+original_array[num_rows-1][i]+sum;
      edge_amt=edge_amt+2;
    }
    for (int i=1;i<num_rows-1;i++){
      sum=original_array[i][0]+original_array[i][num_cols-1]+sum;
      edge_amt=edge_amt+2;
    }
    float value = sum/edge_amt;
    for (int i=1; i<num_rows-1;i++){
      for (int j=1;j<num_cols-1;j++){
        original_array[i][j]=value;
      }
    }
    //pass array data to other ranks
    int start_work_row=0;
    for (int rank=1;rank<=numranks-1;rank++){
      int works= standard_work;
      if (rank<=remains){
        works++;
      }

      for (int i =0; i<works;i++){
        //Send a single rows to appropriate rank.
        float buffer[num_cols];
        for (int j=0;j<num_cols;j++){
          buffer[j]=original_array[start_work_row][j];
        }
        MPI_Ssend(&buffer,num_cols,MPI_FLOAT,rank,0,MPI_COMM_WORLD);
        start_work_row++;
      }
    }

    //Rank 0 main works flows:
    double stime, etime;
    stime=timestamp();
    moreworks=1;
    int itt =0;
    MPI_Bcast(&moreworks,1,MPI_INTEGER,0,MPI_COMM_WORLD);
    while (moreworks==1){
      local_max_epsilon=-10;
      MPI_Reduce(&local_max_epsilon,&global_max_epsilon,1,MPI_FLOAT,MPI_MAX,0,MPI_COMM_WORLD);
      if (global_max_epsilon<=goal_max_epsilon){
        moreworks=0;
        printf("%d %f\n",itt,global_max_epsilon);
      }
      else{
        moreworks=1;
        int logTrunct = (int) log2(itt);
        if (log2(itt)==logTrunct){
          printf("%d %f\n",itt,global_max_epsilon);
        }
      }
      MPI_Bcast(&moreworks,1,MPI_INTEGER,0,MPI_COMM_WORLD);
      itt++;
    }
    etime = timestamp();
    printf("TOTAL TIME %.2f\n",etime-stime );
  }
  else{
    MPI_Request recv_from_above, recv_from_below;
    int works=standard_work;
    if (myranks<=remains){
      works++;
    }
    float extra_above[num_cols],extra_below[num_cols],**old_arr,**new_arr,new_value;
    //Create array to store the data
    old_arr = (float **)malloc(works * sizeof(float *));
    new_arr = (float **)malloc(works * sizeof(float *));
    for (int i=0;i<works;i++){
      old_arr[i]=(float *)malloc(num_cols*sizeof(float));
      new_arr[i]=(float *)malloc(num_cols*sizeof(float));
    }
    //Recv array information
    for (int i=0;i<works;i++){
      float buffer[num_cols];
      MPI_Recv(&buffer,num_cols,MPI_FLOAT,0,0,MPI_COMM_WORLD,&status);
      for (int j=0;j<num_cols;j++){
        old_arr[i][j]=buffer[j];
        new_arr[i][j]=buffer[j];
      }
    }
    MPI_Bcast(&moreworks,1,MPI_INTEGER,0,MPI_COMM_WORLD);

    //Main loops procedures for ranks other than 0:
    while (moreworks==1){
      local_max_epsilon=-10;
      //If I am not top, there is guys above me -> received from above me
      if (myranks!=1){
        MPI_Irecv(&extra_above,num_cols,MPI_FLOAT,myranks-1,0,MPI_COMM_WORLD,&recv_from_above);
      }
      //If I am not bot, there is guys below me -> received from below me
      if (myranks!=numranks-1){
        MPI_Irecv(&extra_below,num_cols,MPI_FLOAT,myranks+1,0,MPI_COMM_WORLD,&recv_from_below);
      }
      //If I am not top, send information of my top row to the guys above me:
      if (myranks!=1){
        float buffer[num_cols];
        for (int i=0;i<num_cols;i++){
          buffer[i]=old_arr[0][i];
        }
        MPI_Send(&buffer,num_cols,MPI_FLOAT,myranks-1,0,MPI_COMM_WORLD);
      }
      //If I am not bot, send information of my bot row to the guys below me:
      if (myranks!=numranks-1){
        float buffer[num_cols];
        for (int i=0;i<num_cols;i++){
          buffer[i]=old_arr[works-1][i];
        }
        MPI_Send(&buffer,num_cols,MPI_FLOAT,myranks+1,0,MPI_COMM_WORLD);
      }
      //If iam not top, received information from the guy above me and update my new top
      if (myranks!=1){
        MPI_Wait(&recv_from_above,&status);
        for (int i=1;i<num_cols-1;i++){
          new_value=(old_arr[0][i-1]+old_arr[0][i+1]+old_arr[1][i]+extra_above[i])/4;
          new_arr[0][i]=new_value;
          if (local_max_epsilon<fabs(new_arr[0][i]-old_arr[0][i])){
            local_max_epsilon=fabs(new_arr[0][i]-old_arr[0][i]);
          }
        }
      }
      //If iam not bot, received information from the guy below me and update my new bot
      if (myranks!=numranks-1){
        MPI_Wait(&recv_from_below,&status);
        for (int i=1;i<num_cols-1;i++){
          new_value=(old_arr[works-1][i-1]+old_arr[works-1][i+1]+old_arr[works-2][i]+extra_below[i])/4;
          new_arr[works-1][i]= new_value;
          if (local_max_epsilon<fabs(new_arr[works-1][i]-old_arr[works-1][i])){
            local_max_epsilon=fabs(new_arr[works-1][i]-old_arr[works-1][i]);
          }
        }
      }
      //Calculate mid section
      for (int i=1; i<works-1;i++){
        for (int j=1;j<num_cols-1;j++){
          new_value=(old_arr[i-1][j]+old_arr[i+1][j]+old_arr[i][j-1]+old_arr[i][j+1])/4;
          new_arr[i][j]=new_value;
          if (local_max_epsilon<fabs(old_arr[i][j] -new_arr[i][j])) {
            local_max_epsilon=fabs(old_arr[i][j] -new_arr[i][j]);
          }
        }
      }
      MPI_Reduce(&local_max_epsilon,&global_max_epsilon,1,MPI_FLOAT,MPI_MAX,0,MPI_COMM_WORLD);
      MPI_Bcast(&moreworks,1,MPI_INTEGER,0,MPI_COMM_WORLD);
      float **arrTemp=old_arr;
      old_arr=new_arr;
      new_arr=arrTemp;
    }
  }

  return 0;
}
