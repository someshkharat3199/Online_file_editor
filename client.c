//Assignment 3
//Roll 21CS60R40

#include<stdio.h>
#include<stdlib.h>
#include<strings.h>
#include<unistd.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<ctype.h>
#include<netdb.h>
#include<string.h>
#include<fcntl.h>
#include<errno.h>
#include<sys/stat.h>
#define PORT 5000
#define BUFFER_SIZE 1024
#define SERV_NAME "localhost"

char *download_path;
char *local_file_path;

int invite_request = 0;
int download = 0;
int print_data = 0;
typedef struct invite_req{
    char invite[256];
    struct invite_req *next;
} invite_req;

invite_req *invite_req_head;

void errors(char *msg,int sockfd){
    perror(msg);
    close(sockfd);
    exit(1);
}

int check_send_recv_errors(int n){
    if(n <= 0){
        //checking whether connection is closed by client
        if(n == 0){
            perror("Server closed the connection\n");
        }
        if(errno != EWOULDBLOCK){
            perror("ERROR: operation failed");
        } 
        return -1;
    } 
    return 0;
}
int is_blank(char *str){
    int i = 0;
    for(i = 0; i < strlen(str); i++){
        if(!isspace(str[i])){
            return 0;
        }
    }
    return 1;
}
int message_check(char *msg){
    if(strlen(msg) == 0 || is_blank(msg)){
        return -1;
    }
    if(isspace(msg[0]) || isspace(msg[strlen(msg)-1])){
        return 0;
    }
    if(msg[0] != '\"' || msg[strlen(msg)-1] != '\"'){
        return 0;
    }
    if(strlen(msg) == 2){
        //messsage is blank and only contains double quotes
        return 0;
    }

    return 1;
}
int number_check(char* num){
    int i =0;
    if(strlen(num) == 0 || is_blank(num)){
        return -1;
    }
    if(num[0] != '+' && num[0] != '-' && !isdigit(num[0])){
        return 0;
    }
    if(num[0] == '+' || num[0] == '-'){
        if(strlen(num) == 1){
            return 0;
        }
    }
    for(i = 1; i < strlen(num);i++){
        if(!isdigit(num[i])){
            return 0;
        }
    }
    return 1;
}

int save_data(char *file_data, char *file_name){

    //copy file_data to the file
    char file_path[255] = {0};
    strcpy(file_path,download_path);
    strcat(file_path,file_name);
    FILE *fd = fopen(file_path,"w");
    if(fd == NULL){
        return -1;
    }
    fputs(file_data+strlen(file_name)+1,fd);
    fclose(fd);
    return 0;
}


int perm_check(char *perm){
    if(strlen(perm) == 0 || is_blank(perm)){
        return -1;
    }
    if(strlen(perm) != 1){
        return 0;
    }
    if(perm[0] != 'V' && perm[0] != 'E'){
        return 0;
    }
    return 1;
}
int  file_name_check(char* file_name){
    int i = 0;
    if(strlen(file_name) == 0 || is_blank(file_name)){
        return -1;
    }
    //check if quotes are correct
    if(file_name[0] == '\"' || file_name[1] == '\''){
        if(file_name[0] != file_name[strlen(file_name)-1]){
            return 0;
        }
    }
    //filnames can contain alphanumeric characters, underscores, dot.
    for(i = 0; i < strlen(file_name);i++){
        if(isspace(file_name[i])){
            return 0;
        }
    }
    return 1;
}
int calc_args(char *inst, char *temp){
    //seperating file names and field names using '&' character
    int arg_num = 0;
    int i;
    char cmd[15] = {0};
    int quote = 0;
    sscanf(inst,"%s",cmd);
    if(!strcmp(cmd,"/insert")){
        for(i = 0; i < strlen(inst); i++){
            //instruction shouldn't contain '&' character already in it
            if(inst[i] == '&'){
                return -1;
            }
            //search for start quote
            if(inst[i] == '\"' && !quote){
                quote = 1;
                continue;
            }
            //search for end quote
            if(inst[i] == '\"' && quote){
                quote = 0;
                continue;
            }
            //if whitespace found replace it with '&' delimiter
            if(isspace(inst[i]) && !isspace(inst[i-1]) && !quote){
                temp[i] = '&';
                arg_num++;
            }
        }
    }
    else{
        for(i = 0; i < strlen(inst); i++){
            //instruction shouldn't contain '&' character already in it
            if(inst[i] == '&'){
                return -1;
            }
            //if whitespace found replace it with '&' delimiter
            if(isspace(inst[i]) && !isspace(inst[i-1])){
                temp[i] = '&';
                arg_num++;
            }
        }
    }
    if(temp[i-1] != '&'){
        temp[i] = '&';
        arg_num++;
    }
    return arg_num-1;
}
int parseInst(char *inst){
    int i =0;
    char cmd[15] = {0};

    //extract command from instruction
    sscanf(inst,"%s",cmd);

    //creating copy of input instruction
    char temp[strlen(inst)+2];      
    bzero(temp,strlen(inst)+2);
    strcpy(temp,inst);

    //instructions shouldn't begin with white spaces
    if(isspace(inst[0]) || isspace(inst[strlen(inst)-1])){
        printf("ERROR: instructions should not begin or end with white sapces\n");
        return 0;
    }

    if(!strcmp(cmd,"/users") || !strcmp(cmd,"/exit") || !strcmp(cmd,"/files")){
        //users or exit or files command
        if(strlen(inst) != strlen(cmd)){
            printf("%s ERROR: too many arguments given\n",cmd);
            return 0;
        }
    }
    else if(!strcmp(cmd,"/upload") || !strcmp(cmd,"/download")){
        //upload or download command

        int valid = 0;
        int arg_num = calc_args(inst,temp);
        if(arg_num < 0){
            printf("%s ERROR: special character '&' not allowed in command\n",cmd);
            return 0;
        }
        else if(arg_num < 1){
            printf("%s ERROR: less arguments provided\n",cmd);
            return 0;
        }
        else if(arg_num > 1){
            printf("%s ERROR: too many arguments provided\n",cmd);
            return 0;
        }

        char file_name[strlen(inst)+1];
        bzero(file_name,strlen(inst)+1);

        //extract file name
        sscanf(temp,"%*[^&]&%[^&]&",file_name);
        
        //check file name
        valid = file_name_check(file_name);
        if(valid == -1){
            printf("%s ERROR: file name not present\n",cmd);
            return 0;
        }
        if(!valid){
            printf("%s ERROR: file name not in correct format or extra white spaces present around filename\n",cmd);
            return 0;
        }
    }
    else if(!strcmp(cmd,"/invite")){
        //invite command

        int valid = 0;
        int arg_num = calc_args(inst,temp);
        if(arg_num < 0){
            printf("%s ERROR: special character '&' not allowed in command\n",cmd);
            return 0;
        }
        else if(arg_num < 3){
            printf("%s ERROR: less arguments provided\n",cmd);
            return 0;
        }
        else if(arg_num > 3){
            printf("%s ERROR: too many arguments provided\n",cmd);
            return 0;
        }

        //extract arguments
        char file_name[255] = {0};
        char client_id[strlen(inst)+1];
        bzero(client_id,strlen(inst)+1);
        char perm[strlen(inst)+1];
        bzero(perm,strlen(inst)+1);
        sscanf(temp,"%*[^&]&%[^&]&%[^&]&%[^&]&",file_name,client_id,perm);

        //check file name
        valid = file_name_check(file_name);
        if(valid == -1){
            printf("%s ERROR: file name not present\n",cmd);
            return 0;
        }
        if(!valid){
            printf("%s ERROR: file name not in correct format or extra white spaces present around filename\n",cmd);
            return 0;
        }

        //check client_id is valid
        valid = number_check(client_id);
        if(valid == -1){
            printf("%s ERROR: client id not present\n",cmd);
            return 0;
        }
        if(!valid){
            printf("%s ERROR: client id is not in correct format or extra white spaces around client id\n",cmd);
            return 0;
        }

        //check permissions
        valid = perm_check(perm);
        if(valid == -1){
            printf("%s ERROR: permission not present\n",cmd);
            return 0;
        }
        if(!valid){
            printf("%s ERROR: permission not correct or extra white spaces present around permission\n",cmd);
            return 0;
        }

    }
    else if(!strcmp(cmd,"/read") || !strcmp(cmd,"/delete")){
        //read or delete command

        int valid = 0;
        int arg_num = calc_args(inst,temp);
        if(arg_num < 0){
            printf("%s ERROR: special character '&' not allowed in command\n",cmd);
            return 0;
        }
        else if(arg_num == 0){
            printf("%s ERROR: very less arguments provided\n",cmd);
            return 0;
        }
        else if(arg_num > 3){
            printf("%s ERROR: too many arguments provided\n",cmd);
            return 0;
        }

        //extract arguments
        char file_name[225] = {0};
        char start_idx[strlen(inst)+1];
        char end_idx[strlen(inst)+1];
        bzero(start_idx,strlen(inst)+1);
        bzero(end_idx,strlen(inst)+1);
        sscanf(temp,"%*[^&]&%[^&]&%[^&]&%[^&]&",file_name,start_idx,end_idx);

        //check file name
        valid = file_name_check(file_name);
        if(valid == -1){
            printf("%s ERROR: file name not present\n",cmd);
            return 0;
        }
        if(!valid){
            printf("%s ERROR: file name not in correct format or extra white spaces present around filename\n",cmd);
            return 0;
        }

        //check start_idx
        valid = number_check(start_idx);
        if(valid != -1 && !valid){
            printf("%s ERROR: start index not in correct format or extra white spaces present around index\n",cmd);
            return 0;
        }

        //check end_idx
        valid = number_check(end_idx);
        if(valid != -1 && !valid){
            printf("%s ERROR: end index not in correct format or extra white spaces present around index\n",cmd);
            return 0;
        }

    }
    else if(!strcmp(cmd,"/insert")){
        //insert command

        int valid = 0;
        int arg_num = calc_args(inst,temp);
        if(arg_num < 0){
            printf("%s ERROR: special character '&' not allowed in command\n",cmd);
            return 0;
        }
        if(arg_num < 2){
            printf("%s ERROR: less number of arguments provided\n",cmd);
            return 0;
        }
        if(arg_num > 3){
            printf("%s ERROR: too many arguments provided\n",cmd);
            return 0;
        }

        //extract arguments
        char file_name[255] = {0};
        char index[strlen(inst)+1];
        char msg[strlen(inst)+1];
        bzero(index,strlen(inst)+1);
        bzero(msg,strlen(inst)+1);
        
        int k = sscanf(temp,"%*[^&]&%[^&]&%[^&]&%[^&]&",file_name,index,msg);
        if(k == 2 && !number_check(index)){
            sscanf(temp,"%*[^&]&%[^&]&%[^&]&",file_name,msg);
            bzero(index,strlen(inst)+1);
        }

        //check file name
        valid = file_name_check(file_name);
        if(valid == -1){
            printf("%s ERROR: file name not present\n",cmd);
            return 0;
        }
        if(!valid){
            printf("%s ERROR: file name not in correct format or extra white spaces present around filename\n",cmd);
            return 0;
        }

        //check index
        valid = number_check(index);
        if(valid != -1 && !valid){
            printf("%s ERROR: index not in correct format or extra white spaces present around index\n",cmd);
            return 0;
        }

        //check message
        valid = message_check(msg);
        if(valid == -1){
            printf("%s ERROR: message not present\n",cmd);
            return 0;
        }
        if(!valid){
            printf("%s ERROR: message not in correct format or extra white spaces present around message\n",cmd);
            return 0;
        }
    }
    else{
        printf("%s Command not found\n",cmd);
        return 0;
    }
    return 1;
}
int main(){
    int sockfd, portno = PORT,max_fd,n=0;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    fd_set master_set,working_set;
    
    //creating socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        errors("ERROR opening socket",sockfd);
    }

    //find server
    server = gethostbyname(SERV_NAME);
    if(server == NULL){
        errors("ERROR no such host",sockfd);
    }

    //fill serv_addr object
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr,server->h_length);

    //connect
    if(connect(sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        errors("Error connecting",sockfd);
    }

    //initializing the master fd_set
    FD_ZERO(&master_set);
    max_fd = sockfd;
    FD_SET(sockfd,&master_set);
    FD_SET(0,&master_set);

    char buffer[BUFFER_SIZE];
    char command[10];
    bzero(command,10);
    bzero(buffer,BUFFER_SIZE);
    //recieve first message from server after sending connection request
    n = recv(sockfd,buffer,BUFFER_SIZE,0);
    if(n < 0){
        errors("ERROR reading from socket",sockfd);
    }
    printf("Message from server: %s\n",buffer);
    if(!strncmp(buffer,"Max Limit reached",18)){
        close(sockfd);
        exit(0);
    }

    //extract client id
    int client_id;
    sscanf(buffer,"%d",&client_id);

    //assign local files path
    local_file_path = malloc(sizeof(char)*20);
    bzero(local_file_path,20);
    sprintf(local_file_path,"local_%d/",client_id);

    //create local files directory for the client
    mkdir(local_file_path,0777);

    //assign downloads path
    download_path = malloc(sizeof(char)*30);
    bzero(download_path,30);
    strcpy(download_path,local_file_path);
    strcat(download_path,"Downloads/");

    //create downloads files directory for the client
    mkdir(download_path,0777);

    do{ 
        //copy master_fd set to wirking fd_set
        memcpy(&working_set, &master_set, sizeof(master_set));

        //call select and wait
        n = select(max_fd+1,&working_set,NULL,NULL,NULL);
        if(n < 0){
            perror("select failed");
            break;
        }
        if(FD_ISSET(0,&working_set)){
            //STDIN FILE DESCRIPTOR

            //take input from user
            bzero(buffer,BUFFER_SIZE);
            bzero(command,10);

            //check for pending invites
            if(invite_request){
                fgets(buffer,1024,stdin);
                buffer[strlen(buffer)-1] = '\0';
                while(strcmp(buffer,"Yes") && strcmp(buffer,"No")){
                    printf("Provide response Yes/No\n");
                    bzero(buffer,BUFFER_SIZE);
                    fgets(buffer,BUFFER_SIZE,stdin);
                    buffer[strlen(buffer)-1] = '\0';
                }
                invite_request--;
            }
            else{
                //take input command from user
                scanf("%[^$]$%*c",buffer);

                //parse instruction
                if(!parseInst(buffer)){
                    continue;
                }
                //extract command from instruction
                sscanf(buffer,"%s",command);
            }
            
            if(strncmp(command,"/upload",7)){
                //send command to server
                n = send(sockfd,buffer,BUFFER_SIZE,0);
                if(check_send_recv_errors(n) < 0){
                    break;
                }
            }

            //check whether exit commmand was fired
            if(!strncmp(command, "/upload",7)){
                char file_path[255] = {0};
                char f_name[255] = {0};                              
                sscanf(buffer,"%*s %s",f_name);   
                strcpy(file_path,local_file_path);
                strcat(file_path,f_name);

                FILE *fd = fopen(file_path,"r");
                if(fd == NULL){                        
                    perror("ERROR opening file");
                    continue;
                }

                //send command to server
                n = send(sockfd,buffer,BUFFER_SIZE,0);
                if(check_send_recv_errors(n) < 0){
                    break;
                }

                //extract file name 
                int j = strlen(f_name)-1;
                while(f_name[j] != '/' && j >= 0){
                    j--;
                }
                char file_name[256] = {0};
                strcpy(file_name,f_name+j+1);

                //calculate size of file
                fseek(fd,0,SEEK_END);
                size_t file_size = ftell(fd) + strlen(file_name) + 1;

                //send size of file to server.
                n = send(sockfd,&file_size,sizeof(file_size),0);
                if(check_send_recv_errors(n) < 0){
                    break;
                }

                //copy contents of file into buffer.
                char *file_data = malloc(sizeof(char)*file_size);
                bzero(file_data,file_size+1);
                char *line = NULL;
                size_t len = 0;
                strcpy(file_data,file_name);
                strcat(file_data,"\n");
                rewind(fd);
                while(getline(&line,&len,fd) != -1){
                    strcat(file_data,line);
                }
                free(line);

                //send file data to server
                n = send(sockfd,file_data,file_size,0);
                if(check_send_recv_errors(n) < 0){
                    fclose(fd);
                    free(file_data);
                    break;
                }

                //cleanup
                fclose(fd);
                free(file_data);

            }
            else if(!strncmp(command,"/download",9)){
                download = 1;
            }
            else if(!strncmp(command,"/read",5) || !strncmp(command,"/insert",7) || !strncmp(command,"/delete",7)){
                print_data = 1;
            }
            else if(!strncmp(command,"/exit",5)){
                break;
            }
        }
        if(FD_ISSET(sockfd,&working_set)){
            //SOCKET FILE DESCRIPTOR

            if(download){
                //If download request was sent.
                size_t file_size = 0;
                char file_name[256] = {0};

                //recieve size of file
                n = recv(sockfd,&file_size,sizeof(file_size),0);
                if(check_send_recv_errors(n) < 0){
                    download = 0;
                    break;
                }

                //recieve file data from server
                char *file_data = malloc(sizeof(char)*(file_size+1));
                bzero(file_data,file_size+1);
                n = recv(sockfd,file_data,file_size,0);
                if(check_send_recv_errors(n) < 0){
                    free(file_data);
                    download = 0;
                    break;
                }

                if(!strcmp(file_data,"Download Failed: File doesnot exists") || !strcmp(file_data,"ERROR: opening file")|| !strcmp(file_data,"Download Failed: Not permitted to read file")){
                    download = 0;
                    printf("%s\n",file_data);
                    free(file_data);
                    continue;
                }
                //extract file name
                sscanf(file_data,"%[^\n]s\n",file_name);

                //saving the data in file
                if(save_data(file_data,file_name) < 0){
                    perror("ERROR:\n");
                }
                else{
                    printf("%s file downloaded successfully\n",file_name);
                }
                download = 0;
                free(file_data);

            }
            else if(print_data){
                //If download request was sent.
                size_t data_size = 0;

                //recieve size of file
                n = recv(sockfd,&data_size,sizeof(data_size),0);
                if(check_send_recv_errors(n) < 0){
                    print_data = 0;
                    break;
                }

                //recieve file data from server
                char *data = malloc(sizeof(char)*(data_size+1));
                bzero(data,data_size+1);
                n = recv(sockfd,data,data_size,0);
                if(check_send_recv_errors(n) < 0){
                    free(data);
                    print_data = 0;
                    break;
                }

                printf("Server message:\n%s\n",data);
                print_data = 0;
                free(data);

            }
            else{
                bzero(buffer,BUFFER_SIZE);
                n = recv(sockfd,buffer,BUFFER_SIZE,MSG_DONTWAIT);
                if(check_send_recv_errors(n) < 0){
                    break;
                }
                
                if(!strncmp(buffer,"/invite",7)){
                    invite_request++;
                    if(invite_request > 1){
                        //create node for new invite
                        invite_req *new_invite = malloc(sizeof(invite_req));
                        strcpy(new_invite->invite,buffer);
                        new_invite->next = NULL;

                        //add new invite to pending invites linklist
                        invite_req *temp = invite_req_head;
                        if(temp == NULL){
                            invite_req_head = new_invite;
                        }
                        else{
                            while(temp->next != NULL){
                                temp = temp->next;
                            }
                            temp->next = new_invite;
                        }
                    }
                    else{
                        printf("server message:\n");
                        printf("%s\n",buffer);
                        printf("provide Yes/No response\n");
                    }     
                }
                else{
                    printf("server message:\n");
                    printf("%s\n",buffer);

                    //flash pending invites
                    if(invite_req_head != NULL){
                        invite_req *temp = invite_req_head;
                        printf("%s\n",temp->invite);
                        printf("Provide response Yes/No\n");

                        //remove that pending request from linklist
                        invite_req_head = temp->next;
                        free(temp);
                    }
                }
            }

            
        }
    }while(1);
    close(sockfd);
    free(local_file_path);
    free(download_path);
    return 0;
}