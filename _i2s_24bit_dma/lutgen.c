#include <math.h>
#include <stdio.h>
#include <stdint.h>

#define PI 3.14159265359
#define AMPLITUDE 5701632
#define NUM_POINTS 512

int main()
{
  int i;  
  FILE* fd;

  fd = fopen("lut.h","w+");

  fprintf(fd,"#include <stdint.h>\n\n");
  fprintf(fd,"const int32_t sin_lut[%d] = {\n  ",NUM_POINTS);

  for(i=0; i<NUM_POINTS-1; i++)
  {
    if(((i % 10) == 0) && (i > 0))
    {
      fprintf(fd,"\n  ");
    }
    fprintf(fd,"%9d,",(int)(sin(2*PI*i/NUM_POINTS) * AMPLITUDE));
  }
  
  fprintf(fd,"%9d\n};",(int)(sin(2*PI*i/NUM_POINTS) * AMPLITUDE));

  fclose(fd);

  return 0;
}
