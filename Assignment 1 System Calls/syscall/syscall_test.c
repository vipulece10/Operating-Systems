    #include<stdio.h>
    #include<unistd.h>
    #include<stdlib.h>
    #include<string.h>
    #include<sys/syscall.h>
    int main(int argc,char *argv[])
    {
        int val;
        int isString=0,isKey=0;
        char *usr_string;
        char *key_val;
	int flag=1;
        while((val=getopt(argc,argv,"s:k:"))!=-1)
        {
            switch(val)
            {
                case 's':
                isString=1;
                usr_string=optarg;
                break;

                case 'k':
                isKey=1;
                key_val=optarg;
                break;

                case '?':
                if(optopt=='s'||optopt == 'k'){
		flag=0;
                }
                else{
		flag=0;
             	}
            	break;
                default:
	        flag=0;
                //fprintf(stderr,"getopt");
         }
     }
     if(flag && isString && isKey){
       int t=atoi(key_val);
       long res=syscall(335,usr_string,t);
       if(res < 0)
       printf("The System call returned %d.\n",(int)res);
       else{
       printf("The System call returned %d.\n",(int)res);	       
       printf("The encrypted string is %s\n",usr_string);
       }
    }
    else{
    fprintf(stderr,"Invalid Input.\n");
    }
    return 0;
}

