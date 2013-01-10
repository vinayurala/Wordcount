/* Multi-threaded program to count words, lines and unique words in a given file */
/* Usage: ./my_wc -w|l|u <input_filename> (one caveat is that it takes only one option
          either -w or -l or -u at this time. 
*/
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#define NUM_THREADS 3
#define MAX_UNIQ_WORDS 10000   // Setting a cap on maximum unique words

pthread_mutex_t mutex_var;

/* Struct to hold unique words found in the file */
struct uniq_words 
{ 
  char *words[MAX_UNIQ_WORDS]; 
  int ind_word_count[MAX_UNIQ_WORDS];
}uniq_wlist;

/* This struct has variables that are local to a thread, so that we don't have 
   to use locks to update them 
*/
struct thread_args
{
  int thread_num;
  int region;
  int start_pos;
  int word_count;
  int line_count;
  FILE *fp;
  char *buffer;
}thd_args[NUM_THREADS];

/* Function to count the words in a file. A pointer to this function is sent as an 
   argument to the thread.
   Input: Pointer to thread_args struct 
   Output: Updates local variable, word_count local to a thread
*/   
void count_words(struct thread_args *thd_args)
{

  fseek(thd_args->fp, thd_args->start_pos, SEEK_SET);

  /* Because a thread is assigned a region of the file based on the size, adjusting
     seek pointer so that it does not read the word or part of it, which will be read
     by the previous thread
  */ 
  if(thd_args->thread_num)
    fscanf(thd_args->fp, "%s", thd_args->buffer);

  while((ftell(thd_args->fp)) <= thd_args->region)
    {
      if((fscanf(thd_args->fp, "%s", thd_args->buffer)) == EOF)
	break;
      
      thd_args->word_count++;
    }

  return;
}

/* Function to count the lines in a file. A pointer to this function 
   is sent as an argument to the thread.
   Input: Pointer to thread_args struct 
   Output: Updates local variable, line_count local to a thread
*/
void count_lines(struct thread_args *thd_args)
{

  fseek(thd_args->fp, thd_args->start_pos, SEEK_SET);

  while((ftell(thd_args->fp)) < thd_args->region)
    {
      if((thd_args->buffer[0] = fgetc(thd_args->fp)) == EOF)
	break;

      if(thd_args->buffer[0] == '\n')
	thd_args->line_count++;
    }

  return;
}

/* Function to count the unique words in a file. A pointer to this function is 
   sent as an argument to the thread.
   Input: Pointer to thread_args struct 
   Output: Updates members of global struct 'uniq_wlist'.
   Side-note: Unfortunately, I ended up using locks in this procedure, even though 
   I did not want to. I could have used thread local variables again, but that does
   not give a major performance boost compared to locks. The code complexity also
   increases, if thread local variables were used in this case. I've tried my best
   to minimize the CPU time spent while holding a lock, by acquiring only when its
   absolutely necessary.
*/
void unique_word_count(struct thread_args *thd_args)
{

  int i = 0;
  int check = 0;
 
  fseek(thd_args->fp, thd_args->start_pos, SEEK_SET);

  if(thd_args->thread_num)
    fscanf(thd_args->fp, "%s", thd_args->buffer);

  while((ftell(thd_args->fp)) <= thd_args->region)
    {
      if(fscanf(thd_args->fp, "%s", thd_args->buffer) == EOF)
	break;
       
      for(i = 0; *uniq_wlist.words[i] != '\0'; i++)
	{
	  if(!(strcmp(thd_args->buffer, uniq_wlist.words[i])))
	    {
	      pthread_mutex_lock(&mutex_var);
	      uniq_wlist.ind_word_count[i]++;
	      check = 1;
	      pthread_mutex_unlock(&mutex_var);
	    }
	}

      pthread_mutex_lock(&mutex_var);
      if(check)
	  check = 0;
      else
	{
	  if(i > MAX_UNIQ_WORDS)
	    return;
	  memmove(uniq_wlist.words[i], thd_args->buffer, strlen(thd_args->buffer));
	  uniq_wlist.ind_word_count[i] = 1;
	}

      pthread_mutex_unlock(&mutex_var);	
    }

  return;
}

/* Function to intialize the members of thread_args and uniq_wlist struct.
   This function also assigns regions that the files have to crawl thru.
   Input: File name string 
   Output: Updates members of thread_args and uniq_wlist struct.
*/
int init_thread_args(char *file_name)

{
  FILE *fp;
  int size, i, j;

  fp = fopen(file_name, "r");
  if(!fp)
    return 0;
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);

  for (i = 0; i < NUM_THREADS; i++)
    {
      thd_args[i].fp = fopen(file_name, "r");
      
      if(!(thd_args[i].fp))
	return 0;
      
      thd_args[i].region = ceil(((i+1)*size)/NUM_THREADS);
      thd_args[i].start_pos = ceil((i*size)/NUM_THREADS);
      thd_args[i].word_count = thd_args[i].line_count = 0;
      thd_args[i].buffer = (char *)malloc(sizeof(char[256]));
    }

    for(j = 0; j < MAX_UNIQ_WORDS; j++)
   {
      uniq_wlist.words[j]= (char *)malloc(sizeof(char [256]));
      uniq_wlist.ind_word_count[j] = -1;
   }

  fclose(fp);
  return 1;
}

/* Function to de-allocate and destroy certain members of thread_args and uniq_wlist struct.
   Input: 'thread_args' struct instance. 
   Output: De-allocate memory and closes all the open file descriptor
*/
void destroy_thread_args(struct thread_args t_args)
{
  int i;

  fclose(t_args.fp);
  t_args.buffer = NULL;
  free(t_args.buffer);

  for(i = 0; i < MAX_UNIQ_WORDS; i++)
    {
      uniq_wlist.words[i] = NULL;
      free(uniq_wlist.words[i]);
    }

  return;

}

/* Main Function */
int main(int argc, char **argv)
{
  pthread_t threads[NUM_THREADS];
  int rc, i, tot_wc = 0, tot_lc = 0, tot_uniq_wc = 0, opt1 = 0, opt2 = 0, opt3 = 0, c, num_ops = 0;
  char *filename = (char *)malloc(sizeof(char));

  void *thread_func_ptr;

  if(argc < 3)
    {
      printf("\nUsage: %s -l|-w|-u <input_filename>\n", argv[0]);
      return -1;
    }

  sscanf(argv[2],"%s", filename);

  while((c = getopt(argc, argv,"w:l:u:")) != -1)
    {
      switch(c)
	{
	case 'w': opt1 = 1;
	          thread_func_ptr = count_words;
	          break;
	case 'l': opt2 = 1;
	          thread_func_ptr = count_lines;
	          break; 
	case 'u': opt3 = 1;
	          thread_func_ptr = unique_word_count;
	          break;
	default:  printf("\nUsage: %s -l|-w|-u <input_filename>\n", argv[0]);
	          return -1;
	}
    } 

  pthread_mutex_init(&mutex_var, NULL);

  if(!(init_thread_args(filename)))
    {
      printf("\nFile %s does not exist!!\n", filename);
      return -1;
    }

  for(i = 0; i < NUM_THREADS; i++)
    {
      thd_args[i].thread_num = i;
      rc = pthread_create(&threads[i], NULL, thread_func_ptr, &thd_args[i]);
      if(rc)
	{
	  printf("\nError while waiting for threads to complete. Error code: %d\n",rc);
	  return -1;
	}
    }
  
  for(i = 0; i < NUM_THREADS; i++)
    {
      rc = pthread_join(threads[i], NULL);
      if(rc)
	{
	  printf("\nError while waiting for threads to complete. Error code: %d\n",rc);
	  return -1;
	}
    }
      
  if(opt1)
    {
      for(i = 0; i < NUM_THREADS; i++)
	tot_wc += thd_args[i].word_count;
      printf("\nWord count: %d", tot_wc);
    }
      
  if(opt2)
    {
      for(i = 0; i < NUM_THREADS; i++)
	tot_lc += thd_args[i].line_count;
      printf("\nLine count: %d", tot_lc);
    }

  if(opt3)
    {
      for(i = 0; uniq_wlist.ind_word_count[i] != -1; i++)
	tot_uniq_wc++;
      printf("\nUnique word count: %d", tot_uniq_wc);
    }

  for(i = 0; i < NUM_THREADS; i++)
    destroy_thread_args(thd_args[i]);
  
  filename = NULL;
  free(filename);
  pthread_mutex_destroy(&mutex_var);
  
  printf("\n");    
  return 0;
}
