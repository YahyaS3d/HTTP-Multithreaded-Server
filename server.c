//server.c file
//Name: Yahya Saad
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threadpool.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
//---------some macros---------
#define VERSION1 "HTTP/1.0"
#define BUF_SIZE 4096 // buffer size supposed to get max size up to 4096 --> 2^12
#define RESPONSE 512// max response size
#define HTML 255 // 2^8 - 1
#define MAX_PORT_VAL 9999
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT" //construct the date header
#define STATUS 255// 2^8 - 1
#define EMPTY   0// set content and some char* to empty
#define ENTITY_LINE 500 //length of the first line in the request is no more than 500
//---status code parameters---
#define FOUND 302 // if the error is 302 then add location header, otherwise omit the location header
//FOUND response too,  Missing "/" at the end of the requested directory
#define INTERNAL 500 //"Internal Server Error"
#define BAD_REQ 400 //"Bad request"
#define NOT_SUPP 501//Method is not "GET" -> this code run GET method only
#define NOT_FOUND 404 //File not found
#define FORBIDDEN 403 // Cannot access file.(not regular file or no permissions)
#define OK 200 //Proper request GET the request in normal situation
typedef enum
{
    false,
    true
} bool; //bool struct: set a boolean value to an important flags
char timebuf[128]; // return the current time through another function
//---------private functions---------
void printUsage();//usage error
void systemError(char *); //return error msg
char *getTime();//returns the current date and time
char *get_mime_type(char *);// check the content type is the mime type of the response body
int connect_to_server(void*);//modifying the content with client request and set the response
//-----------------------------------
char *get_mime_type(char *name)
{
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}
int connect_to_server(void* in)
{
    //---response content parameters---
    char* res_header=NULL;
    unsigned char* res_body=NULL;
    char* res_type=NULL;
    int status=0;//status code for each response
    bool errorFlag=false; // error flag
    //-------------------
    //---file parameters---
    int stream=*(int*)in;
    struct stat flop;
    struct stat fs;
    int file=EMPTY;
    struct dirent* dentry;
    DIR* dir;
    bool fileFlag=false;
    //-------------------
    bool contentFlag=false;
    //---request content parameters---
    char* req_path=NULL;
    char* req_protocol=NULL;
    char* req_method=NULL;
    //---------------------
    int size=EMPTY;
    // in case accept failed in main failed
    if(stream<0){
        perror("ERROR on accept");
        errorFlag=true;
        status=INTERNAL;
        res_header="Internal Server Error";
        res_body="Some server side error.";
        res_type="text/html";
    }
    char request[BUF_SIZE]={EMPTY};// build the request with make buffer size
    int rc=0;

    rc=read(stream,request,BUF_SIZE);
    // in case read failed
    if(rc < 0){
        perror("Read failed");

        errorFlag=true;
        status=INTERNAL;
        res_header="Internal Server Error";
        res_body="Some server side error.";
        res_type="text/html";
    }
    if(rc == 0){
        return EXIT_SUCCESS;
    }

    char buildReq[2]=" "; //get request tokens - start building it
    req_method=strtok(request,buildReq);
    req_path=strtok(NULL,buildReq);
    req_protocol=strtok(NULL,buildReq);

    if(req_protocol!=NULL){
        int count=0;
        while(req_protocol[count]!='\r')
            count++;

        req_protocol[count]='\0';
    }

    //In case the request is wrong, send a “400 Bad Request" response
    if(req_method==NULL || req_path==NULL || req_protocol==NULL){
        errorFlag=true;
        status=BAD_REQ;
        res_header="Bad Request";
        res_body="Bad Request.";
        res_type="text/html";

    }

        //HTTP versions
    else if(strncmp(req_protocol,VERSION1,strlen(req_protocol))!=0 ){
        errorFlag=true;
        status=BAD_REQ;
        res_header="Bad Request";
        res_body="Bad Request.";
        res_type="text/html";
    }


    else if(strncmp(req_method,"GET",strlen(req_method))!=0){//GET requests only

        errorFlag=true;
        status=NOT_SUPP;
        res_header="Not supported";
        res_body="Method is not supported.";
        res_type="text/html";
    }
    if(errorFlag==false){
        //create full path
        char createPath[STATUS]={EMPTY};
        createPath[0]='.';
        strcat(createPath,req_path);

        //create dir content
        char dirContent[STATUS]={EMPTY};
        dirContent[0]='.';
        strcat(dirContent,req_path);

        dir=opendir(createPath);

        // in case path is not directory or no permissions
        if(dir==NULL){


            char* mkdir = (char*)calloc(sizeof(createPath),sizeof(char));
            int index = 0;

            while(index < strlen(createPath))
            {
                while(createPath[index] != '/' && index < strlen(createPath))
                {
                    mkdir[index] = createPath[index];
                    index++;
                }

                if(stat(mkdir,&flop) < 0)
                {
                    errorFlag=true;
                    status=NOT_FOUND;
                    res_header="Not Found";
                    res_body="File not found.";
                    res_type="text/html";
                    break;
                }

                if(S_ISREG(flop.st_mode))
                {
                    if((flop.st_mode & S_IROTH) && (flop.st_mode & S_IRGRP) && (flop.st_mode & S_IRUSR)){}
                    else
                    {
                        errorFlag=true;
                        status=FORBIDDEN;
                        res_header="Forbidden";
                        res_body="Access denied.";
                        res_type="text/html";
                        break;
                    }
                }
                else if(S_ISDIR(flop.st_mode))
                {
                    if(flop.st_mode & S_IXOTH ){}
                    else
                    {
                        errorFlag=true;
                        status=FORBIDDEN;
                        res_header="Forbidden";
                        res_body="Access denied.";
                        res_type="text/html";

                        break;
                    }
                }

                mkdir[index]='/';
                index++;
            }

            free(mkdir);

            // return file
            if(status==0)
            {
                file=open(createPath,O_RDONLY);
                stat(createPath,&fs);
                fileFlag=true;
                status=OK;
                res_header="OK";
                res_type=get_mime_type(req_path);
                size=(int)fs.st_size;
                res_body=(unsigned char*)calloc(size+1,sizeof(unsigned char));
                read(file,res_body,size);
            }
        }

            // directory found
        else
        {
            // 302 ERROR
            if(req_path[strlen(req_path)-1]!='/')
            {
                errorFlag=true;
                status=FOUND;
                res_header="Found";
                res_body="Directories must end with a slash.";
                res_type="text/html";
                closedir(dir);
            }

                // directory found
            else
            {
                // index.html file
                strcat(createPath,"index.html");
                file=open(createPath,O_RDONLY);
                if(file>EMPTY)
                {
                    stat(createPath,&fs);
                    status=OK;
                    res_header="OK";
                    res_type="text/html";
                    fileFlag=true;
                    size=(int)fs.st_size;
                    res_body=(unsigned char*)calloc(size+1,sizeof(unsigned char));
                    read(file,res_body,size);
                    closedir(dir);
                }

                    // no index.html, return contents of the directory
                else
                {
                    stat(dirContent,&fs);
                    status=OK;
                    res_header="OK";
                    res_type="text/html";
                    fileFlag=true;
                    contentFlag=true;

                    while((dentry=readdir(dir))!=NULL)
                    {
                        size+=ENTITY_LINE;
                    }
                    closedir(dir);
                    res_body=(unsigned char*)calloc((size+1+RESPONSE),sizeof(unsigned char));
                    strcat(res_body,"<HTML>\r\n<HEAD><TITLE>Index of ");


                    strcat(res_body,dirContent);
                    strcat(res_body,"</TITLE></HEAD>\r\n\r\n<BODY>\r\n<H4>Index of ");
                    strcat(res_body,dirContent);
                    strcat(res_body,"</H4>\r\n\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n");
                    dir=opendir(dirContent);
                    while((dentry=readdir(dir))!=NULL)
                    {
                        //set a temporary parameter to strcat dirContent
                        char* tmp=(char*)calloc(strlen(dirContent)+1+strlen(dentry->d_name),sizeof(char));
                        strcat(tmp,dirContent);
                        strcat(tmp,dentry->d_name);

                        stat(tmp,&flop);
                        strcat(res_body,"<tr>\r\n<td><A HREF=");
                        strcat(res_body,dentry->d_name);
                        strcat(res_body,">");
                        strcat(res_body,dentry->d_name);
                        strcat(res_body,"</A></td><td>");
                        time_t now = flop.st_mtime;
                        strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
                        strcat(res_body,timebuf);
                        strcat(res_body,"</td>\r\n<td>");

                        if(!S_ISDIR(flop.st_mode))
                        {
                            int time=(int)flop.st_size;
                            sprintf(timebuf,"%d",time);
                            strcat(res_body,timebuf);
                        }

                        strcat(res_body,"</td>\r\n</tr>");

                        free(tmp);
                    }

                    strcat(res_body,"\r\n\r\n</table>\r\n\r\n<HR>\r\n\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n\r\n</BODY></HTML>\r\n");
                    closedir(dir);

                }
            }
        }
    }
    char status_response[STATUS]={EMPTY};//response status
    char html[HTML]={EMPTY};// HTML length

    if(errorFlag==true)
    {
        //HTML Text
        strcat(html,"<HTML><HEAD><TITLE>");
        sprintf(status_response,"%d",status);
        strcat(html,status_response);
        strcat(html," ");
        strcat(html,res_header);
        strcat(html,"</TITLE></HEAD>\r\n<BODY><H4>");
        strcat(html,status_response);
        strcat(html," ");
        strcat(html,res_header);
        strcat(html,"</H4>\r\n");
        strcat(html,res_body);
        strcat(html,"\r\n</BODY></HTML>\r\n");
    }
    //Response headers
    char response[RESPONSE]={EMPTY};
    strcat(response,"HTTP/1.1 ");
    sprintf(status_response,"%d",status);
    strcat(response,status_response);
    strcat(response," ");
    strcat(response,res_header);
    strcat(response,"\r\nServer: webserver/1.0\r\nDate: ");
    char *dateTime = getTime();
    strcat(response, dateTime);

    if(status==FOUND){
        strcat(response,"\r\nLocation: ");
        strcat(response,req_path);
        strcat(response,"/");
    }
    if(res_type!=NULL)
    {
        strcat(response,"\r\nContent-Type: ");
        strcat(response,res_type);
    }

    strcat(response,"\r\nContent-Length: ");

    if(errorFlag==true)
    {
        sprintf(status_response,"%ld",strlen(html));
        strcat(response,status_response);
    }
    else
    {
        if(contentFlag==true)
            sprintf(status_response,"%ld",strlen(res_body));
        else
            sprintf(status_response,"%d",size);

        strcat(response,status_response);
        strcat(response,"\r\nLast-Modified: ");
        time_t now = fs.st_mtime;
        strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
        strcat(response,timebuf);
    }


    strcat(response,"\r\nConnection: close\r\n\r\n");
    if(errorFlag==true)
        strcat(response,html);


    write(stream, response, strlen(response));

    if(fileFlag==true){
        write(stream,res_body,size);
        free(res_body);
    }

    close(stream);
}

int main(int argc,char* argv[]){


    if(argc!=4) //4 args as an input
    {
        printUsage();
        exit(EXIT_SUCCESS);
    }
    //server <port> <pool-size> <max-number-of-request>
    int port = atoi(argv[1]);
    char* threadsNum=argv[2];
    char* reqNum=argv[3];
    //invalid input
    if(port < 1 || atoi(threadsNum) < 1 || atoi(reqNum)< 1)
    {
        printUsage();
        exit(EXIT_SUCCESS);
    }
    int stream;   // socket fd stream
    struct sockaddr_in serv_addr;

    stream = socket(AF_INET, SOCK_STREAM, 0);
    //check the socket
    if (stream < 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    //the server listens to requests in any of its addresses.
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if (port < 0 || port > MAX_PORT_VAL)
    {
        printUsage();
        close(stream);
        systemError("Invalid port number");
    }
    serv_addr.sin_port = htons(port);
    //check bind
    if(bind(stream,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0){
        systemError("Bind failed");
    }
    //check listen
    if(listen(stream, 5) < 0){ // max = 5
        close(stream);
        systemError("Listen failed");
    }
    //set final request with the thread pool
    int* finalReq=(int*)calloc(atoi(reqNum),sizeof(int));
    if(finalReq==NULL){
        systemError("Can't allocate memory");
    }

    threadpool* thread=create_threadpool(atoi(threadsNum));
    if(thread==NULL){
        free(finalReq);
        systemError("Creating the thread pool failed");
    }

    struct sockaddr acc_addr; //accept address
    int acc_len = sizeof(serv_addr); // accept length
    int acc_socket; // accept socket
    for(int i=0; i<atoi(reqNum) ;i++)
    {
        acc_socket = accept(stream, &acc_addr, (socklen_t *)&acc_len);

        finalReq[i]=acc_socket;
        if(finalReq[i] < 0){
            close(stream);
            systemError("Error on accept");
        }
        dispatch(thread,connect_to_server,&finalReq[i]);
    }

    close(stream);
    destroy_threadpool(thread);
    free(finalReq);

    return EXIT_SUCCESS;
}
//print usage error
void printUsage(){
    printf("Usage: server <port> <pool-size> <max-number-of-request>\n"); //Command line usage
}
void systemError(char *str) //return error msg
{
    perror(str);
    exit(EXIT_FAILURE);
}
char *getTime() // return current time
{
    time_t now;
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    return timebuf;
}
