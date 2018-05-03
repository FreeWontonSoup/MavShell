#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    FILE *in;
    in = freopen("test.txt","w",stdout);
    
    int i = 0;
    while(i < 10)
    {
        printf("%d\n",i);
        i++;
        sleep(1);
    }
    fclose(in);
}
