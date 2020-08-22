#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


//some definitions
#define FALSE 0
#define TRUE 1
#define ADDR unsigned long
#define DATA unsigned long
#define BOOL char
#define log2(x) log(x)/log(2)

typedef struct _MEMREQUEST{
    ADDR addr;
    BOOL is_write;
    DATA data;
} MEMREQUEST;

typedef struct _CACHE{
    BOOL valid;
    BOOL dirty;
    int tag;
    DATA data[64]; // max 1block = 64words
} CACHE;

typedef struct _MEMORY{
    BOOL valid;
    DATA idx;
    DATA tag;
    DATA data[64];
} MEMORY;
//misc. function
BOOL read_new_memrequest(FILE*, MEMREQUEST*);  //read new memory request from the memory trace file (already implemented)

//TODO: implement this function
//configure a cache
void init_cache();
void init_memory();
BOOL search_mem();
//TODO: implement this function
//check if the memory request hits on the cache
BOOL isHit(MEMREQUEST *mem_request);

//TODO: implement this function
//write data to the cache. Data size is 4B
void write_data(MEMREQUEST *mem_request);

//TODO: implement this function
//insert a new block into the cache
//the initial values of the block are all zeros
void insert_to_cache();

//TODO: implement this function
//print the contents stored in the data storage of the cache
void print_contents();

//TODO: update this function so that some simulation statistics are calculated in it
//print the simulation statistics
void print_stats();


////global variables///
int cache_size=32768;               //cache size
int block_size=32;                  //block size
int cache_idx;

///////////////stats
long long hit_cnt=0;               //total number of cache hits
long long miss_cnt=0;              //total number of cache misses
float miss_rate=0;                 //miss rate
long long dirty_block_num=0;       //total number of dirty blocks in cache at the end of simulation
float average_mem_access_time=0; //average memory access time
long miss_penalty=200;             //miss penalty
long cache_hit_time=1;             //cache hit time

///////////////
CACHE cache[262144];                       //cache struct array
MEMORY mem[270000];                             //mem array
DATA bufdata[64];
int mem_idx = 0;
int addr_offset;                    //offset of address by given block size and cache size
int addr_wordoffset;                //word offset of address by given block size and cache size
int addr_idx;                       //index of address by given block size and cache size
int addr_tag;                       //tag of address by given block size and cache size
unsigned long this_tag;                       //tag of this given address
unsigned long this_idx;                       //index of this given address
unsigned long this_word;                       //word of this given addres

/*
 * main
 *
 */
int main(int argc, char*argv[])  
{
    char trace_file[100];
	
    //Read through command-line arguments for options.
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') 
        {
            if (argv[i][1] == 's') 
                cache_size=atoi(argv[i+1]);
            
            if (argv[i][1] == 'b')
                block_size=atoi(argv[i+1]);
            
            if (argv[i][1] == 'f')
                strcpy(trace_file,argv[i+1]);
        }
    }
 
    //open the mem trace file
    FILE* fp = 0;
    fp=fopen(trace_file,"r");
    if(fp==NULL)
    {
        printf("[error] error opening file");
        fflush(stdout);
        exit(-1);   
    }

    ///main body of cache simulator /////////////////
    init_cache();   //configure a cache with the cache parameters specified in the input arguments
    init_memory();

    while(1)
	{
        MEMREQUEST new_request;
        BOOL success=read_new_memrequest(fp, &new_request);  //read a new memory request from the memory trace file
        
        if(success!=TRUE)   //check the end of the trace file
            break;
        
        //check if the new memory request hits on the cache
        //if miss on the cache, insert a new block to the cache
        if(isHit(&new_request)==FALSE) 
        {
            insert_to_cache();  
        }

        // //if the request type is a write, write data to the cache
        if(new_request.is_write)
            write_data(&new_request);
    }
    print_contents();  //print the contentns (blocks) stored in the data storage of the cache
    print_stats();     //print simulation statistics
    
    fclose(fp);
    return 0;
}

/*
 * Function: read_new_memrequest
 * ____________________________
 * read a new memory request from the memory trace file
 *
 */
BOOL read_new_memrequest(FILE* fp, MEMREQUEST* mem_request)
{
    ADDR request_addr;
    DATA data;
    char request_type[5];
    char str_read[100];
    
    if(mem_request==NULL)
    {
        fprintf(stderr,"MEMREQUEST pointer is null!");
        exit(2);
    }

    if(fgets(str_read,100,fp))
    {
        str_read[strlen(str_read) - 1] = '\0';
        char *token = strtok(str_read," ");
        if(token)
        {
            mem_request->addr=strtol(token,NULL,16);

            token=strtok(NULL," ");
            if(strcmp(token,"R")==0)
                mem_request->is_write=FALSE;
            else if(strcmp(token,"W")==0){
                mem_request->is_write=TRUE;
                token= strtok(NULL, " ");
                if(token)
                    mem_request->data=atoi(token);
                else
                { 
                    fprintf(stderr,"[error] write request with no data!!\n");
                    exit(-1);
                }
            }
            else
            {
                printf("[error] unsupported request type!:%s\n",token);
                fflush(stdout);
                exit(-1);
            }
            return TRUE;
            
        }
    }       
    
    return FALSE;
}


void init_cache(){
    int i,j;


    addr_offset = log2(block_size);
    addr_wordoffset = addr_offset -2 ;
    cache_idx = cache_size/block_size;
    addr_idx = log2(cache_idx);
    addr_tag = 32 - (addr_offset + addr_idx);
    

    for(i=0; i<cache_idx; i++)
    {
        cache[i].valid = 0;
        cache[i].dirty = 0;
        cache[i].tag = -1;
        for(j=0; j<block_size/4; j++)
        {
            cache[i].data[j] = 0;
        }
    }
}

//init memory
void init_memory(){
    int i,j;

    for(i=0; i<270000; i++)
    {
        mem[i].valid = 0;
        mem[i].idx = -1;
        mem[i].tag = -1;
        for(j=0; j<block_size/4; j++)
        {
            mem[i].data[j] = 0;
        }
    }
}

//search in memory with index,tag
BOOL search_mem()
{
    int i,j ;

    for(i=0; i<mem_idx; i++)
    {
        if(  (mem[i].idx == this_idx) && (mem[i].tag == this_tag) && (mem[i].valid == 1) )
        {
            for(j=0; j<block_size/4; j++)
            {
                bufdata[j] = mem[i].data[j];
                mem[i].data[j] = 0;
            }
            mem[i].valid = 0;
            mem[i].idx = -1;
            mem[i].tag = -1;

            for(j=i; j<mem_idx; j++) //배열 한칸씩 당기기
            {
                if(mem[j+1].valid != 0)
                    mem[j] = mem[j+1];
            }
            mem_idx--;
            return TRUE;
        }
    }

    return FALSE;
}

//check if the memory request hits on the cache
BOOL isHit(MEMREQUEST *mem_request){
    int i,j;
    int mask = 1;

    this_tag = mem_request->addr;
    for(i=0; i<32-addr_tag; i++)
        this_tag = this_tag >> 1; 

    //this_tag = this_tag >> (32-addr_tag) ; //this request tag

    //determin this request idx
    if (addr_idx != 0)
    {
        this_idx = mem_request->addr;
        for (i = 31; i >= 32 - addr_tag; i--)
        {
            mask = mask << i;
            this_idx &= ~(mask);
            mask = 1;
        }
        this_idx = this_idx >> addr_offset;
    }
    else
    {
        this_idx = 0;
    }



    if (addr_wordoffset != 0)
    {
        this_word = mem_request->addr;
        mask = 1;
        for (i = 31; i >= 32 - (addr_tag+addr_idx); i--)
        {
            mask = mask << i;
            this_word &= ~(mask);
            mask = 1;
        }
        this_word = this_word >> 2;
    }
    else
    {
        this_word = 0;
    }

//printf("0\n");
    if (cache[this_idx].valid == 1)//valid check
    {
        if (cache[this_idx].tag == this_tag)//hit
        {
            hit_cnt++;
            return TRUE;
        }
        else //miss, not my tag
        {
            //printf("1\n");
            miss_cnt++;
            return FALSE;
        }
    }
    else //miss
    {
        //printf("2\n");
        //printf("miss index: , tag:%lu\n",this_tag);
        miss_cnt++;
        return FALSE;
    }
}

//write data to the cache. Data size is 4B
void write_data(MEMREQUEST *mem_request){

    if (cache[this_idx].dirty == 0)
    {
        cache[this_idx].dirty = 1;
        dirty_block_num++;
    }
    cache[this_idx].data[this_word] = mem_request->data;   
    
}

//insert a new block into the cache
//the initial values of the block are all zeros
void insert_to_cache(MEMREQUEST *mem_request){
    int i;

    if( cache[this_idx].valid == 1) //cache에 데이터가 있을때 evict
    {
        if(cache[this_idx].dirty == 1) //write-back 해줘야하면
        {
            dirty_block_num--;

            //insert to memory
            mem[mem_idx].valid = 1;
            mem[mem_idx].idx = this_idx;
            mem[mem_idx].tag = cache[this_idx].tag;
            //printf("evict addr: %lx, idx: %lu, tag: %lu\n",mem_request->addr,this_idx,this_tag);
            for (i = 0; i < block_size / 4; i++)
            {
                //printf("%ld ",cache[this_idx].data[i]);
                mem[mem_idx].data[i] = cache[this_idx].data[i];
            }

            mem_idx++;
        }

        cache[this_idx].tag = this_tag;
        cache[this_idx].dirty = 0;

        if(search_mem() == TRUE)
        {
            //printf("true addr: %lx, idx: %lu, tag: %lu\n",mem_request->addr,this_idx,this_tag);
            for (i = 0; i < block_size / 4; i++)
            {
                //printf("%ld ",bufdata[i]);
                cache[this_idx].data[i] = bufdata[i];
            }
            //puts("");
        }
        else
        {
            for (i = 0; i < block_size / 4; i++)
                cache[this_idx].data[i] = 0;
        }
    }
    else //빈 cache 일때
    {
        cache[this_idx].valid = 1;
        cache[this_idx].tag = this_tag;
        cache[this_idx].dirty = 0;

        if(search_mem() == TRUE)
        {
            for (i = 0; i < block_size / 4; i++)
                cache[this_idx].data[i] = bufdata[i];
        }
        else
        {
            for (i = 0; i < block_size / 4; i++)
                cache[this_idx].data[i] = 0;
        }
    }
}

/*
 * Function: print_contents
 * --------------------------
 * print the contents (blocks) stored in the cache
 *
 */
void print_contents(){
    int i,j;
    printf("\n1.Cache contents");
    printf("\nindex     contents \n");
    for(i=0; i<cache_idx; i++)
    {
      printf("%d: ", i);
      for(j=0; j<block_size/4; j++)
         printf("%lu ", cache[i].data[j]);
      printf("\n");
    }
    printf("\n----------------------------------------------\n");

}


/*
 * Function: print_stat
 * --------------------------
 * print the simulation statistics
 *
 */
void print_stats()
{
    //TODO: Calculate some simulation statistics
    miss_rate = (float) miss_cnt / (float)(hit_cnt+miss_cnt);
    average_mem_access_time = cache_hit_time + miss_rate*miss_penalty;

    //print the simualtion statistics
    printf("\n2.Simulation statistics\n");
    printf("total number of hits: %lld\n", hit_cnt);
    printf("total number of misses: %lld\n", miss_cnt);
    printf("miss rate: %f\n",miss_rate);
    printf("total number of dirty blocks: %lld\n",dirty_block_num);
    printf("average memory access time: %f\n",average_mem_access_time);

}


