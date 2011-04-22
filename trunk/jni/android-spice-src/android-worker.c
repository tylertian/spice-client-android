#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#include <stdio.h>
#include <jpeglib.h>
#include "spice-common.h"
#include "android-spice.h"
#include "android-spice-priv.h"

extern GMainLoop* volatile android_mainloop;
extern pthread_mutex_t android_mutex;  
extern pthread_cond_t android_cond;  
extern volatile int android_task_ready;
extern volatile int android_task;
volatile AndroidShow android_show_display;
gboolean key_event(AndroidEventKey* key);
gboolean button_event(AndroidEventButton *button);

void getval(void* dest,void* src,int type)
{
    int i;
    switch(type)
    {
	case INT:
	    memset(dest,0,4);
	    for(i=0;i<4;i++)
		memcpy(dest+i,src+3-i,1);
	    break;
	case UINT_8:
	    break;
    }
}
void error(const char *msg)
{
    SPICE_DEBUG(msg);
    exit(0);
}
void android_send_task(int task)
{
    while(!android_task_ready);  
    SPICE_DEBUG("send task:%d\n",task);
    pthread_mutex_lock(&android_mutex);  
    android_task_ready = 0;  
    android_task = task;  
    pthread_cond_signal(&android_cond);  
    pthread_mutex_unlock(&android_mutex);  
    while(android_task);  
    SPICE_DEBUG("send task done:%d\n",task);
}
int msg_recv_handle(int sockfd,char* buf)
{
    int n,type;
    n = read(sockfd,buf,4);
    if(n==4)
    {
	getval(&type,buf,INT);
	SPICE_DEBUG("Got event:%d\n",type);
	switch(type)
	{
	    case ANDROID_OVER:
		{
		    android_send_task(ANDROID_TASK_OVER);
		    g_main_loop_quit(android_mainloop);
		    exit(1);
		    return 1;
		}
		break;
	    case ANDROID_KEY_PRESS:
	    case ANDROID_KEY_RELEASE:
		n = read(sockfd,buf,4);
		if(n==4)
		{
		    AndroidEventKey* key =(AndroidEventKey*)malloc(8);
		    key->type = type;
		    getval(&key->hardware_keycode,buf,INT);
		    key_event(key);
		    free(key);
		}
		else
		    error("msg_recv error!\n");
		break;
	    case ANDROID_BUTTON_PRESS:
	    case ANDROID_BUTTON_RELEASE:
		n = read(sockfd,buf,8);
		if(n==4)
		    n += read(sockfd,buf+4,8);
		if(n==8)
		{
		    AndroidEventButton* button =(AndroidEventButton*)malloc(12);
		    button->type = type;
		    getval(&button->x,buf,INT);
		    getval(&button->y,buf+4,INT);
		    button_event(button);
		    free(button);
		}
		else
		    error("msg_recv error!\n");
		break;
	}
    }
    else
	error("msg_recv error!\n");
    return 0;
}
int write_data(int sockfd,uint8_t* buf,int size,int type)
{
    int i;
    int num=0;
    int steps = 0;
    switch(type)
    {
	case INT:
	    while(steps<size)
	    {
		for(i=3;i>=0;i--)
		    num+=write(sockfd,buf+steps+i,1);
		steps+=4;
	    }
    }
    return num;
}
int msg_send_handle(int sockfd)
{
    int n;
    uint8_t* buf = (uint8_t*)&(android_show_display.type);
    n = write_data(sockfd,buf,24,INT);
    if(n<=0)
	goto error;
    n = write(sockfd,android_show_display.data,android_show_display.size);
    if(n<=0)
	goto error;
    //free(android_show_display.data);
    SPICE_DEBUG("Image bytes sent:%d",n);
    return 0;
error:
    if(n=0)
	error("connection error!\n");
    else if(n<0)
	error("msg_send error!\n");
}

uint8_t* raw2jpg(uint8_t* data, int width,int height )
{
    uint8_t* bmp =(uint8_t*)malloc(3*width*height);
    uint8_t* loc = bmp;
    int n = width*height;

    while (n > 0) {
	loc[0]=data[2];
	loc[1]=data[1];
	loc[2]=data[0];
	data += 4;
	loc  += 3;
	n--;
    }
    int bytes_per_pixel = 3;
    int color_space = JCS_RGB; /* or JCS_GRAYSCALE for grayscale images */
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    /* this is a pointer to one row of image data */
    JSAMPROW row_pointer[1];
    cinfo.err = jpeg_std_error( &jerr );
    jpeg_create_compress(&cinfo);
    /*
       unsigned long outlen;
       jpeg_mem_dest (&cinfo,&outbuffer,&outlen );
       */
    char* filename = "/data/data/com.keqisoft.android.spice/ahoo.jpg";
    FILE *foolfile = fopen( filename, "w+b" );
    if ( !foolfile )
    {
	SPICE_DEBUG("Error opening output jpeg file %s\n!", filename );
	return NULL;
    }

    unsigned char *outbuffer;
    jpeg_stdio_dest(&cinfo, foolfile);

    /* Setting the parameters of the output file here */
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = bytes_per_pixel;
    cinfo.in_color_space = color_space;
    /* default compression parameters, we shouldn't be worried about these */

    jpeg_set_defaults(&cinfo );
    cinfo.num_components = 3;
    //cinfo.data_precision = 4;
    cinfo.dct_method = JDCT_FLOAT;
    jpeg_set_quality(&cinfo, 90, TRUE);
    /* Now do the compression .. */
    jpeg_start_compress( &cinfo, TRUE );
    /* like reading a file, this time write one row at a time */
    while( cinfo.next_scanline < cinfo.image_height )
    {
	row_pointer[0] = &bmp[ cinfo.next_scanline * cinfo.image_width * cinfo.input_components];
	jpeg_write_scanlines( &cinfo, row_pointer, 1 );
    }
    /* similar to read file, clean up after we're done compressing */
    jpeg_finish_compress( &cinfo );
    jpeg_destroy_compress( &cinfo );
    free(bmp);

    fseek (foolfile, 0,SEEK_END);
    int foolsize = ftell (foolfile);
    android_show_display.size = foolsize;
    rewind (foolfile);
    //freed by caller;
    fclose(foolfile);
    //outbuffer = (uint8_t*) malloc (foolsize);
    //if (outbuffer == NULL) {error("Memory error");}
    //int result = fread (outbuffer,1,foolsize,foolfile);
    //if (result != foolsize) {error("Reading error");}
    return outbuffer;
}


void android_show(spice_display* d,gint x,gint y,gint w,gint h)
{
    android_show_display.type = ANDROID_SHOW;
    android_show_display.width = w;
    android_show_display.height = h;
    android_show_display.x = x;
    android_show_display.y = y;
    //android_show_display.data =  raw2jpg((uint8_t*)d->data+y*d->width+x,w,h);
    //android_show_display.data =  raw2jpg((uint8_t*)d->data,d->width,d->height);//+y*d->width+x,w,h);
    SPICE_DEBUG("ANDROID_SHOW for %p:w--%d:h--%d:x--%d:y--%d:jpeg_size--%d",
	    (char*)android_show_display.data,
	    android_show_display.width,
	    android_show_display.height,
	    android_show_display.x,
	    android_show_display.y,
	    android_show_display.size);
    android_send_task(ANDROID_TASK_SHOW);
}
int android_spice_input()
{
    int sockfd, newsockfd, servlen, n;
    socklen_t clilen;
    struct sockaddr_un  cli_addr, serv_addr;
    char buf[16];

    if ((sockfd = socket(AF_UNIX,SOCK_STREAM,0)) < 0)
	error("creating socket");
    memset((char *) &serv_addr,0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    char* sock = "/data/data/com.keqisoft.android.spice/spice-input.socket";
    //char* sock = "/data/local/tmp/spice-input.socket";
    remove(sock);
    strcpy(serv_addr.sun_path, sock);
    servlen=strlen(serv_addr.sun_path) + 
	sizeof(serv_addr.sun_family);
    if(bind(sockfd,(struct sockaddr *)&serv_addr,servlen)<0)
	error("binding socket"); 

    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept( sockfd,(struct sockaddr *)&cli_addr,&clilen);
    if (newsockfd < 0) 
	error("accepting");
    while(1)
    {
	if(msg_recv_handle(newsockfd,buf))
	    break;
    }
    close(newsockfd);
    close(sockfd);
    SPICE_DEBUG("android input over\n");
    return 0;
}
int android_spice_output()
{
    int sockfd, newsockfd, servlen, n;
    socklen_t clilen;
    struct sockaddr_un  cli_addr, serv_addr;

    if ((sockfd = socket(AF_UNIX,SOCK_STREAM,0)) < 0)
	error("creating socket");
    memset((char *) &serv_addr,0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    char* sock = "/data/data/com.keqisoft.android.spice/spice-output.socket";
    //char* sock = "/data/local/tmp/spice-output.socket";
    remove(sock);
    strcpy(serv_addr.sun_path, sock);
    servlen=strlen(serv_addr.sun_path) + 
	sizeof(serv_addr.sun_family);
    if(bind(sockfd,(struct sockaddr *)&serv_addr,servlen)<0)
	error("binding socket"); 

    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept( sockfd,(struct sockaddr *)&cli_addr,&clilen);
    if (newsockfd < 0) 
	error("accepting");
    int over = 0;  
    while(!over)  
    {  
	pthread_mutex_lock(&android_mutex);  
	android_task_ready = 1;  
	pthread_cond_wait(&android_cond,&android_mutex);  
	SPICE_DEBUG("got task:%d\n",android_task);
	if(android_task == ANDROID_TASK_SHOW) {
	    if(msg_send_handle(newsockfd)) {
		over = 1;
	    }
	    android_task = ANDROID_TASK_IDLE;  
	} else if(android_task == ANDROID_TASK_OVER) {
	    over =1;
	    android_task = ANDROID_TASK_IDLE;  
	}
	pthread_mutex_unlock(&android_mutex);  
	SPICE_DEBUG("got task done:%d\n",android_task);
    }  

    close(newsockfd);
    close(sockfd);
    SPICE_DEBUG("android output over\n");
    return 0;
}

