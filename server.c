//Assignment 3
//Roll: 21CS60R40

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<fcntl.h>
#include<errno.h>
#include<signal.h>
#include<ctype.h>
#include<sys/stat.h>
#define PORTNO 5000
#define MAX_CONN 5
#define BUFFER_SIZE 1024
int connections = 0;

char *client_uploads_path;

//invite records
typedef struct invite_record{
    int source;
    int dest;
    char file_name[225];
    char permission;
    struct invite_record *next;
} invite_record;

//permission records
typedef  struct perm_record{
    char file_name[255];
    int num_lines;
    int owner;
    int view[5];
    int edit[5];
    struct perm_record *next;
} perm_record;

//client records
typedef struct client_info{
    int id;
    int sockfd;
    struct client_info *next;
} client_info;

invite_record *invite_record_head = NULL;
perm_record *perm_record_head = NULL;
client_info *client_info_head = NULL;

int remove_client_id(int sockfd){
    int client_id = 10000+sockfd;
    client_info *temp = client_info_head;

    //delete from front
    if(temp != NULL && temp->id == client_id){
        client_info *delete = temp;
        client_info_head = temp->next;
        free(delete);
        return 0;
    }
    while(temp != NULL && temp->next != NULL){
        if((temp->next)->id == client_id){
            client_info *delete = temp->next;
            temp->next = (temp->next)->next;
            free(delete);
            return 0;
        }
        temp = temp->next;
    }
    return 0;  
}
int generate_id(int sockfd){
    //generate unique 5 digit id and store it in file
    int client_id = 10000 + sockfd;
    client_info *new_record = malloc(sizeof(client_info));
    new_record->id = client_id;
    new_record->sockfd = sockfd;
    new_record->next = client_info_head;
    client_info_head = new_record;
    return client_id;
}
void free_lists(){
    //free linklists
    perm_record *temp1 = perm_record_head;
    client_info *temp2 = client_info_head;
    invite_record *temp3 = invite_record_head;

    //delete from beginning of linklist
    while(perm_record_head != NULL){
        perm_record_head = temp1->next;
        char file_path[256] = {0};
        strcpy(file_path,client_uploads_path);
        strcat(file_path,temp1->file_name);
        remove(file_path);
        free(temp1);
    }
    while(client_info_head != NULL){
        client_info_head = temp2->next;
        free(temp2);
    }
    while(invite_record_head != NULL){
        invite_record_head = temp3->next;
        free(temp3);
    }

}

void serverStop(int n){
    free_lists();
    free(client_uploads_path);
    exit(0);
}

void errors(char *msg,int sockfd){
    perror(msg);
    close(sockfd);
    exit(1);
}

int isActive(int client_id){
    client_info *temp = client_info_head;
    while(temp != NULL){
        if(temp->id == client_id){
            //client is active
            return 1;
        }
        temp = temp->next;
    }

    //client is not active
    return 0;
}

int remove_file(char *file_name){
    //remove pending invites associated with file
    int n = 0;
    invite_record *temp1 = invite_record_head;
    invite_record *parent1 = invite_record_head;

    //delete from beginning from link list
    while(temp1!= NULL){
        if(!strcmp(temp1->file_name,file_name)){
            //invite record found
            invite_record *delete;
            if(temp1 == invite_record_head){
                invite_record_head = temp1->next;
            }
            else{
                parent1->next = temp1->next;
            }
            delete = temp1;
            temp1 = delete->next;
            int source = delete->source;
            int dest = delete->dest;
            free(delete);
            if(isActive(dest)){
                n = send(dest-10000,"Invite cancelled: file was deleted",34,MSG_DONTWAIT);
                if(errno != EWOULDBLOCK){
                    perror("remove_file(): send() error");
                }
                if(n == 0){
                    perror("Client disconnected");
                }
            }
            if(isActive(source)){
                n = send(source-10000,"Invite cancelled: file was deleted",34,MSG_DONTWAIT);
                if(errno != EWOULDBLOCK){
                    perror("remove_file(): send() error");
                }
                if(n == 0){
                    perror("client disconnected");
                }
            }
            continue;
        }
        if(temp1 != parent1){
            parent1 = parent1->next;
        }
        temp1 = temp1->next;
    }

    //delete file from permissions record
    perm_record *temp2 = perm_record_head;
    perm_record *parent2 = perm_record_head;
    while(temp2 != NULL){
        if(!strcmp(temp2->file_name,file_name)){
            //permission record found
            if(temp2 == perm_record_head){
                perm_record_head = temp2->next;
            }
            else{
                parent2->next = temp2->next;
            }
            char file_path[256] = {0};
            strcpy(file_path,client_uploads_path);
            strcat(file_path,file_name);
            free(temp2);
            if(remove(file_path) != 0){
                return -1;
            }
            return 0;
        }
        if(parent2 != temp2){
            parent2 = parent2->next;
        }
        temp2 = temp2->next;
    } 
    return 0;
}

//closes the perticular socket and removes unique id
void closeSocket(int sockfd,int max_fd,fd_set *master_set){ 
    close(sockfd);
    FD_CLR(sockfd, master_set);
    if(sockfd == max_fd){
        while(FD_ISSET(max_fd,master_set) == 0){
            max_fd--;
        }
    }

    //remove client id form users record
    if(remove_client_id(sockfd) < 0){
        perror("closeSocket() ERROR");
    }

    //remove files owned and permissions owned by the client.
    perm_record *temp = perm_record_head;
    while(temp != NULL){
        if(temp->owner == sockfd+10000){
            //if owner of file
            if(remove_file(temp->file_name) < 0){
                perror("error while removing file");
            }
        }
        else{
            //if not owner but has some permissions on file
            int k;
            for(k = 0; k < 5; k++){
                if((temp->edit)[k] == sockfd+10000){
                    temp->edit[k] = -1;
                }
                if((temp->view[k] == sockfd+10000)){
                    temp->view[k] = -1;
                }
            }
        }
        temp = temp->next;
    }
    //remove invitations associated with the user
    invite_record *temp2 = invite_record_head;
    invite_record *parent2 = invite_record_head;
    while(temp2 != NULL){
        if(temp2->dest == sockfd+10000){
            invite_record *delete;
            if(invite_record_head == temp2){
                invite_record_head = temp2->next; 
            }
            else{
                parent2->next = temp2->next;
            }
            delete = temp2;
            temp2 = delete->next;
            int source = delete->source;
            free(delete);
            if(isActive(source)){
                int n;
                n = send(source-10000,"Invite Rejected: client disconnected",36,MSG_DONTWAIT);
                if(n == 0){
                    perror("Client disconnected");
                }
                if(errno != EWOULDBLOCK){
                    perror("remove_file(): send() error");
                }
            } 
            continue;
            
        }
        if(temp2 != parent2){
            parent2 = parent2->next;
        }
        temp2 = temp2->next;
    }

    connections--;
}

void closeSockets(int max_fd,fd_set *master_set){
    //closing sockets that are open in master set.
    int i = 0;
    for(i = 0; i <= max_fd; i++){
        if(FD_ISSET(i,master_set)){
            close(i);
        }
    }
    free_lists();
}

int nlinex(char *file_name){     
    //identify file path
    char file_path[256] = {0};
    strcpy(file_path,client_uploads_path);
    strcat(file_path,file_name);

    //count number of lines
    FILE *fd = fopen(file_path,"r");
    if(fd == NULL){
        return -1;
    }
    int count = 0;
    while(fscanf(fd,"%*[^\n]\n") != -1){
        count++;
    }

    //return number of lines
    return count;
}


int save_data(char *file_data, char *file_name, int user_id){

    //check whether file already existed
    perm_record *temp = perm_record_head;
    while(temp != NULL){
        if(!strcmp(temp->file_name,file_name)){
            return 1;
        }
        temp = temp->next;
    }

    //copy file_data to the file
    char file_path[255] = {0};
    strcpy(file_path,client_uploads_path);
    strcat(file_path,file_name);
    FILE *fd = fopen(file_path,"w");
    if(fd == NULL){
        return -1;
    }
    fputs(file_data+strlen(file_name)+1,fd);
    fclose(fd);

    //add information about file
    perm_record *new_record = malloc(sizeof(perm_record));
    strcpy(new_record->file_name,file_name);
    new_record->owner = user_id;
    new_record->num_lines = nlinex(file_name);
    int i = 0;
    for(i = 0; i < 5; i++){
        (new_record->view)[i] = -1;
        (new_record->edit)[i] = -1;
    }

    //add record to start of link list
    new_record->next = perm_record_head;
    perm_record_head = new_record;

    return 0;
}

int isfileOwner(char *file_name, int client_id){
    perm_record *temp  = perm_record_head;
    while(temp != NULL){
        if(!strcmp(temp->file_name,file_name)){
            //file found
            if(temp->owner == client_id){
                //file and owner match
                return 1;
            }
            else{
                //file match but owner didn't match
                return 0;
            }
        }
        temp = temp->next;
    }

    //file not found 
    return -1;
}

int check_send_recv_errors(int n, int i, int max_fd, fd_set *master_set){
    if(n <= 0){
        //checking whether connection is closed by client
        if(n == 0){
            printf("%d Client closed the connection\n",i+10000);
            closeSocket(i,max_fd,master_set);
        }
        //send or recv operation failed
        if(errno != EWOULDBLOCK){
            perror("ERROR: operation failed");
            closeSocket(i,max_fd,master_set);
        } 
        return -1;
    } 
    return 0;
}

char* get_file_records(){
    int i = 0;
    FILE *fd = tmpfile();
    if(fd == NULL){
        return NULL;
    }

    //write permission records to file
    perm_record *temp = perm_record_head;
    while(temp != NULL){
        fprintf(fd,"%s lines: %d, owner: %d, V: ",temp->file_name,temp->num_lines,temp->owner);
        for(i = 0; i < 5; i++){
            if((temp->view)[i] != -1){
                fprintf(fd, "%d ",(temp->view)[i]);
            }
        }
        fprintf(fd,",E: ");
        for(i = 0; i < 5; i++){
            if((temp->edit)[i] != -1){
                fprintf(fd, "%d ",(temp->edit)[i]);
            }
        }
        fprintf(fd,"\n");
        temp = temp->next;
    }

    //calculate size of file
    size_t file_size = ftell(fd);
    if(file_size == 0){
        return "";
    }
    rewind(fd);
    
    char *perm_data = malloc((file_size+1)*sizeof(char));
    bzero(perm_data,file_size+1);

    //write file data to char array
    char *line = NULL;
    size_t len = 0;
    while(getline(&line,&len,fd) != -1){
        strcat(perm_data,line);
    }
    free(line);
    fclose(fd);
    return perm_data;
}
int duplicate_invite(int source, char *file_name, int dest_id, char perm){
    int duplicate_invite = 0;
    invite_record *temp1 = invite_record_head;
    while(temp1 != NULL){
        if(temp1->source == source && !strcmp(file_name,temp1->file_name) && dest_id == temp1->dest && temp1->permission == perm){
            return 1;
            break;
        }
        temp1 = temp1->next;
    }
    return 0;
}
int has_perm(int client_id,char *file_name, char perm){
    //check file permission records
    perm_record *temp = perm_record_head;
    while(temp != NULL){
        if(!strcmp(temp->file_name,file_name)){
            //file record found
            if(perm == 'V'){
                int k;
                for(k = 0; k < 5; k++){
                    if((temp->view)[k] == client_id){
                        return 1;
                    }
                }
            }
            else if(perm == 'E'){
                int k;
                for(k = 0; k < 5; k++){
                    if((temp->edit)[k] == client_id){
                        return 1;
                    }
                }
            }

            //no permissions on given file
            return 0;       
        }
        temp = temp->next;
    }

    //file not found
    return 0; 
}

char *read_file(char *file_name,int start_idx, int end_idx){
    //get number of lines in the file.
    int N = nlinex(file_name);

    //if file is empty
    if(N == 0 && start_idx == 0 && end_idx == -1){
        char *read_data = malloc(20*sizeof(char));
        bzero(read_data,20);
        strcpy(read_data,"File is empty!");
        return read_data;
    }
    //check whether indices are valid (start >= 0: range 0 to N-1) (start < 0: range -N to -1)
    if(start_idx >= N || start_idx < -N || end_idx >= N || end_idx < -N){
        return NULL;
    }
    if(start_idx >= 0){      
        start_idx++;
    } 
    else{               
        start_idx = N + start_idx + 1;
    }
    if(end_idx >= 0){
        end_idx++;
    }
    else{
        end_idx = N + end_idx + 1;
    }

    //start index should be less that the end index
    if(start_idx > end_idx){
        return NULL;
    }
    
    //identify the path of file
    char file_path[256] = {0};
    strcpy(file_path,client_uploads_path);
    strcat(file_path,file_name);

    //open file
    FILE *fd = fopen(file_path,"r");
    if(fd == NULL){
        return NULL;
    }

    //reach the starting point
    int count = 0;
    while(count != start_idx-1){
        fscanf(fd,"%*[^\n]\n");
        count++;
    }
    int file_pos = ftell(fd);

    //count number of characters
    char *line = NULL;
    size_t len = 0;
    size_t bytes = 0;
    count = 0;
    while(count != end_idx-start_idx+1){
        getline(&line,&len,fd);
        bytes += strlen(line);
        count++;
    }
    rewind(fd);
    free(line);

    //extract data to read
    char *read_data = malloc(sizeof(char)*(bytes+1));
    bzero(read_data,bytes+1);
    fseek(fd,file_pos,SEEK_SET);
    line = NULL;
    len = 0;
    count = 0;
    while(count != end_idx-start_idx+1){
        getline(&line,&len,fd);
        strcat(read_data,line);
        count++;
    }
    if(read_data[strlen(read_data)-1] == '\n'){
        read_data[strlen(read_data)-1] = '\0';
    }
    free(line);
    fclose(fd);
    return read_data;
}
char* delete_lines(char *file_name,int start_idx, int end_idx){
    //get number of lines in the file.
    int N = nlinex(file_name);

    //check whether indices are valid (start >= 0: range 0 to N-1) (start < 0: range -N to -1)
    if(start_idx >= N || start_idx < -N || end_idx >= N || end_idx < -N){
        return NULL;
    }
    if(start_idx >= 0){      
        start_idx++;
    } 
    else{               
        start_idx = N + start_idx + 1;
    }
    if(end_idx >= 0){
        end_idx++;
    }
    else{
        end_idx = N + end_idx + 1;
    }

    //start index should be less that the end index
    if(start_idx > end_idx){
        return NULL;
    }
    
    //identify the path of file
    char file_path[256] = {0};
    strcpy(file_path,client_uploads_path);
    strcat(file_path,file_name);

    //create a temporary file
    FILE *temp = tmpfile();
    if(temp == NULL){
        return NULL;
    }

    FILE *fd = fopen(file_path,"r+");
    if(fd == NULL){
        fclose(temp);
        return NULL;
    }

    char *line = NULL;
    size_t len = 0;
    int count = 0;

    //read initial part of file
    while(count != start_idx-1){
        getline(&line,&len,fd);
        if(count == start_idx-2 && end_idx == N){
            line[strlen(line)-1] = '\0';
        }
        fputs(line,temp);
        count++;
    }
    int file_pos = ftell(fd);

    //skip the lines to be removed
    count = 0;
    while(count != end_idx-start_idx+1){
        fscanf(fd,"%*[^\n]\n");
        count++;
    }

    //write remaining content to temp file
    while(getline(&line,&len,fd) != -1){
        fputs(line,temp);
    }
    fclose(fd);

    //copy contents of temp file to original file
    rewind(temp);
    fd = fopen(file_path,"w");
    while(getline(&line,&len,temp) != -1){
        fputs(line,fd);
    }
    fclose(fd);
    fclose(temp);
    free(line);

    //update permission records
    perm_record *perm_rec = perm_record_head;
    while(perm_rec != NULL){
        if(!strcmp(file_name,perm_rec->file_name)){
            //file record found
            perm_rec->num_lines = nlinex(file_name);
        }
        perm_rec = perm_rec->next;
    }

    //return contents of modified file
    char *read_data = read_file(file_name,0,-1);
    return read_data;
}
char* insert_lines(char *file_name,int line_no, char *msg){
    //get number of lines in the file.
    int N = nlinex(file_name);

    //check whether line no is valid (lineno >= 0: range 0 to N-1) (lineno < 0: range -N to -1)
    if(line_no > N || line_no < -N){
        return NULL;
    }
    if(line_no >= 0){
        line_no++;
    }
    else{
        line_no = N + line_no + 1;
    }

    //identify the path of file
    char file_path[256] = {0};
    strcpy(file_path,client_uploads_path);
    strcat(file_path,file_name);

    //if lineno not specified append
    if(line_no == (N+1)){
        FILE *fd = fopen(file_path,"a+");
        if(fd == NULL){
            return NULL;
        }
        if(line_no == 1){
            fprintf(fd,"%s",msg);
        }
        else{
            fprintf(fd,"\n%s",msg);
        }  
        fclose(fd);
    }
    else{
        //line number is specified

        //create a temporary file
        FILE *temp = tmpfile();
        if(temp == NULL){
            return NULL;
        }

        FILE *fd = fopen(file_path,"r+");
        if(fd == NULL){
            fclose(temp);
            return NULL;
        }

        char *line = NULL;
        size_t len = 0;
        int count = 0;

        //read initial part of file
        while(count != line_no-1){
            fscanf(fd,"%*[^\n]\n");
            count++;
        }
        int file_pos = ftell(fd);
        //add the message given by user to tmpfile
        fprintf(temp,"%s\n",msg);

        //add the remaining lines to tmpfile
        while(getline(&line,&len,fd) != -1){
            fputs(line,temp);
        }

        //reset file pointer to beginning of file
        rewind(temp);
        fseek(fd,file_pos,SEEK_SET);
        free(line);

        //copy contents of temp file to server_file
        line = NULL;
        len = 0;
        while(getline(&line,&len,temp) != -1){
            fputs(line,fd);
        }

        free(line);
        fclose(fd);
        fclose(temp);
    }

    //update permission records
    perm_record *perm_rec = perm_record_head;
    while(perm_rec != NULL){
        if(!strcmp(file_name,perm_rec->file_name)){
            //file record found
            perm_rec->num_lines = nlinex(file_name);
        }
        perm_rec = perm_rec->next;
    }

    //return contents of modified file
    char *file_data = read_file(file_name,0,-1);
    return file_data;
}
int main(){
    int sockfd,portno = PORTNO,max_fd,desc_ready,len;
    int n,i,new_fd,end_server = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    fd_set master_set, working_set;

    //create the socket
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
        perror("ERROR creating socket");
        exit(1);
    }

    if(fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0){
        errors("can't set non-blocking socket",sockfd);
    }

    //binding the socket
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
        errors("error on binding",sockfd);
    }

    //listen
    if(listen(sockfd,5) < 0){
        errors("listen failed",sockfd);
    }
    

    //initializing the master fd_set
    FD_ZERO(&master_set);
    max_fd = sockfd;
    FD_SET(sockfd,&master_set);

    //define serverStop signal
    signal(SIGINT,serverStop);

    //assign client uploads path
    client_uploads_path = malloc(20*sizeof(char));
    bzero(client_uploads_path,20);
    strcpy(client_uploads_path,"Client_Uploads/");

    //create directory
    mkdir(client_uploads_path,0777); 

    do{
        //copy master_fd set to working fd_set
        memcpy(&working_set, &master_set, sizeof(master_set));

        //call select and wait
        n = select(max_fd+1,&working_set,NULL,NULL,NULL);
        if(n < 0){
            perror("select failed");
            closeSockets(max_fd,&master_set);
            exit(1);
        }

        //some descriptors are readable
        desc_ready = n;
        for(i = 0; i <= max_fd && desc_ready > 0; i++){
            if(FD_ISSET(i,&working_set)){
                desc_ready--;

                //is it is lisning socket for server
                if(i == sockfd){
                    do{
                        new_fd = accept(sockfd,NULL,NULL);
                        if(new_fd < 0){
                            //possibly an error or no connection request
                            if(errno != EWOULDBLOCK){
                                perror("accept failed");
                            }
                            break;
                        }
                        if(connections + 1 > MAX_CONN){
                            //max connections limit reached
                            int n = send(new_fd, "Max Limit reached",18,EWOULDBLOCK);
                            if (n < 0){
                                if(errno != EWOULDBLOCK){
                                    perror("max limit message send failed");
                                }
                                continue;
                            }
                            close(new_fd);
                            continue;
                        }
                        //add new incomming connection to master set read
                        FD_SET(new_fd, &master_set);
                        if(new_fd > max_fd){
                            max_fd = new_fd;
                        }
                        connections++;
                        int id;
                        if((id = generate_id(new_fd)) < 0){
                            perror("Failed to generate unique ID\n");
                            closeSocket(new_fd,max_fd,&master_set);
                            continue;
                        }

                        //prepare welcome message
                        char welcome_msg[50] = {0};
                        sprintf(welcome_msg,"%d Welcome connected to server",id);
                        int n = send(new_fd, welcome_msg,strlen(welcome_msg),EWOULDBLOCK);
                        if (n < 0){
                            if(errno != EWOULDBLOCK){
                                perror("welcome message send failed");
                                closeSocket(new_fd,max_fd,&master_set);
                            }
                            continue;
                        }
                        
                    }while(new_fd != -1);
                }
                else{
                    //socket other than lisning socket was ready
                    do{
                        bzero(buffer,BUFFER_SIZE);
                        //recieve data from the connection until 
                        //recv fails with EWOULDBLOCK
                        n = recv(i,buffer,BUFFER_SIZE,MSG_DONTWAIT);
                        if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                            break;
                        }

                        //printing client commmand
                        printf("%d client message: %s\n",i+10000,buffer);
                        
                        if(!strcmp(buffer,"Yes") || !strcmp(buffer,"No")){
                            //check whether the response is for pending invites requests
                            invite_record *invites = invite_record_head;
                            invite_record *parent = invite_record_head;
                            int invite_found = 0;
                            while(invites != NULL){
                                if(invites->dest == i+10000){
                                    //invite record found
                                    invite_found = 1;

                                    if(!strcmp("Yes",buffer)){
                                        //dest id accepted the invite request
                                        perm_record *temp = perm_record_head;
                                        while(temp != NULL){
                                            if(!strcmp(temp->file_name,invites->file_name)){
                                                //permission record found
                                                if(invites->permission == 'V'){
                                                    int j = 0;
                                                    for(j = 0; j < 5; j++){
                                                        if((temp->view)[j] == -1){
                                                            (temp->view)[j] = invites->dest;
                                                            break;
                                                        }
                                                    }
                                                }
                                                else if(invites->permission == 'E'){
                                                    int j = 0;
                                                    for(j = 0; j < 5; j++){
                                                        if((temp->edit)[j] == -1){
                                                            (temp->edit)[j] = invites->dest;
                                                            break;
                                                        }
                                                    }
                                                }
                                                break;
                                            }
                                            temp = temp->next;
                                        }

                                        //send success message to source
                                        n = send((invites->source)-10000,"Invite accepted",16,MSG_DONTWAIT);
                                        if(check_send_recv_errors(n,(invites->source)-10000,max_fd,&master_set) < 0){
                                            break;
                                        }

                                        //send success message to destination
                                        n = send(i,"Invite accepted",16,MSG_DONTWAIT);
                                        if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                            break;
                                        }

                                    }
                                    else if(!strcmp("No",buffer)){
                                        //dest id didn't accepted the invite request
                                        //send failure message to source
                                        n = send((invites->source)-10000,"Invite rejected",16,MSG_DONTWAIT);
                                        if(check_send_recv_errors(n,(invites->source)-10000,max_fd,&master_set) < 0){
                                            break;
                                        }

                                        //send success message to dest
                                        n = send(i,"Invite rejected",16,MSG_DONTWAIT);
                                        if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                            break;
                                        } 
                                    }

                                    //remove the invite record
                                    if(invites == invite_record_head){
                                        //remove first record
                                        invite_record_head = invites->next;
                                        free(invites);
                                    }
                                    else{
                                        parent->next = invites->next;
                                        free(invites);
                                    }
                                    break;
                                }
                                
                                if(invites != parent){
                                    parent = parent->next;
                                }
                                invites = invites->next;
                            }

                            if(!invite_found){
                                n = send(i,"Invite cancelled, sender disconnected or file was removed",58,MSG_DONTWAIT);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                } 
                            }

                            //after processing invite break
                            break;
                        }

                        //extract command
                        char cmd[10] = {0};
                        sscanf(buffer,"%s",cmd);

                        //get users list command
                        if(!strncmp(cmd,"/users",6)){
                            //get list of client ids
                            bzero(buffer,BUFFER_SIZE);
                            client_info *temp = client_info_head;
                            while(temp != NULL){
                                char line[20] = {0};
                                sprintf(line,"%d %d\n",temp->id,temp->sockfd);
                                strcat(buffer,line);
                                temp = temp->next;
                            }
                            
                            //send list of client ids
                            n = send(i,buffer,BUFFER_SIZE,MSG_DONTWAIT);
                            if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                break;
                            }              
                        }
                        else if(!strcmp(cmd,"/read")){     
                            char file_name[256] = {0};
                            int start_idx = 0, end_idx = 0;
                            int client_id = 10000+i;

                            //extract file_name, start_idx, end_idx
                            int args = sscanf(buffer,"%*s %s %d %d",file_name,&start_idx,&end_idx);

                            //check if client has required permissions to read the file.
                            int res = isfileOwner(file_name,client_id);
                            if(res == -1){
                                //file doesnot exists
                                size_t msg_size = 33;
                                n = send(i,&msg_size,sizeof(msg_size),0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                n = send(i,"Read Failed: File doesnot exists",msg_size,0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                break;
                            }
                            else if(res || has_perm(client_id,file_name,'V') || has_perm(client_id,file_name,'E')){
                                //client is either file owner or has view and edit permissions.
                                char *read_data;
                                if(args == 1){ //entire file should be returned
                                    read_data = read_file(file_name,0,-1);
                                }
                                else if(args == 2){//single line should be returned
                                    read_data = read_file(file_name,start_idx,start_idx);
                                }
                                else if(args == 3){
                                    read_data = read_file(file_name,start_idx,end_idx);
                                }
                                if(read_data == NULL){
                                    size_t msg_size = 33;
                                    n = send(i,&msg_size,sizeof(msg_size),0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                    n = send(i,"Read failed: Index out of bounds",msg_size,0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                }
                                else{
                                    //send read_data to client
                                    size_t data_size = strlen(read_data);
                                    n = send(i,&data_size,sizeof(data_size),0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                    n = send(i,read_data,strlen(read_data),0);
                                    free(read_data);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                }     
                            }
                            else{
                                //client doesn't have required permissions
                                size_t msg_size = 40;
                                n = send(i,&msg_size,sizeof(msg_size),0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                n = send(i,"Read Failed: Not permitted to read file",msg_size,0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                } 
                                break;
                            }
                        }
                        else if(!strcmp(cmd,"/delete")){
                            char file_name[32] = {0};
                            int start_idx = 0, end_idx = 0;
                            int client_id = 10000+i;

                            //extract file_name, start_idx, end_idx
                            int args = sscanf(buffer,"%*s %s %d %d",file_name,&start_idx,&end_idx);

                            //check if client has required permissions to delete the file.
                            int res = isfileOwner(file_name,client_id);
                            if(res == -1){
                                //file doesnot exists
                                size_t msg_size = 35;
                                n = send(i,&msg_size,sizeof(msg_size),0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                n = send(i,"Delete Failed: File doesnot exists",msg_size,0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                break;
                            }
                            else if(res || has_perm(client_id,file_name,'E')){
                                //client is either file owner or has edit permissions.
                                char *file_data;
                                if(args == 1){ //entire file contents deleted
                                    file_data = delete_lines(file_name,0,-1);
                                }
                                else if(args == 2){//single line should be deleted
                                    file_data = delete_lines(file_name,start_idx,start_idx);
                                }
                                else if(args == 3){
                                    file_data = delete_lines(file_name,start_idx,end_idx);
                                }
                                if(file_data == NULL){
                                    size_t msg_size = 35;
                                    n = send(i,&msg_size,sizeof(msg_size),0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                    n = send(i,"Delete failed: Index out of bounds",msg_size,0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                }
                                else{
                                    //send read_data to client
                                    size_t data_size = strlen(file_data);
                                    n = send(i,&data_size,sizeof(data_size),0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                    n = send(i,file_data,strlen(file_data),0);
                                    free(file_data);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                }     
                            }
                            else{
                                //client doesn't have required permissions
                                size_t msg_size = 50;
                                n = send(i,&msg_size,sizeof(msg_size),0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                n = send(i,"Delete Failed: Not permitted to delete from file",msg_size,0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                break;
                            }
                        }
                        else if(!strcmp(cmd,"/insert")){
                            char file_name[32] = {0};
                            int index = 0;
                            char message[1024] = {0};
                            int client_id = 10000+i;
                            
                            //extract arugments from command
                            int args2 = 0;
                            int args = sscanf(buffer,"%*s %s %d \"%[^\"]s\"",file_name,&index,message);
                            if(args == 1){
                                args2 = sscanf(buffer,"%*s %s \"%[^\"]s\"",file_name,message);
                            }

                            //check if client has required permissions to insert into the file.
                            int res = isfileOwner(file_name,client_id);
                            if(res == -1){
                                //file doesnot exists
                                size_t msg_size = 34;
                                n = send(i,&msg_size,sizeof(msg_size),0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                n = send(i,"Insert Failed: File doesnot exists",msg_size,0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                break;
                            }
                            else if(res || has_perm(client_id,file_name,'E')){
                                //client is either file owner or has edit permissions.
                                char *file_data;
                                if(args == 1 && args2 == 2){
                                    //line number not specified.
                                    int num_lines = nlinex(file_name);
                                    file_data = insert_lines(file_name,num_lines,message);
                                }
                                else if(args == 3){
                                    file_data = insert_lines(file_name,index,message);
                                }
                                if(file_data == NULL){
                                    size_t msg_size = 35;
                                    n = send(i,&msg_size,sizeof(msg_size),0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                    n = send(i,"Insert failed: Index out of bounds",msg_size,0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                }
                                else{
                                    //send data to client
                                    size_t data_size = strlen(file_data);
                                    n = send(i,&data_size,sizeof(data_size),0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                    n = send(i,file_data,strlen(file_data),0);
                                    free(file_data);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }
                                }     
                            }
                            else{
                                //client doesn't have required permissions
                                size_t msg_size = 49;
                                n = send(i,&msg_size,sizeof(msg_size),0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                n = send(i,"Insert Failed: Not permitted to insert into file",msg_size,0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                break;
                            }
                        }
                        else if(!strcmp(cmd,"/invite")){
                            char file_name[256] = {0};
                            int dest_id;
                            char perm;

                            //extract client id, permission and filename
                            sscanf(buffer,"%*s %s %d %c",file_name,&dest_id,&perm);

                            //identify self invites
                            if(i+10000 == dest_id){
                                n = send(i,"Invite failed: self invite",27,MSG_DONTWAIT);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                } 
                                break;
                            }

                            //check if sender is file owner
                            int res = isfileOwner(file_name,i+10000);
                            if(res == -1){
                                n = send(i,"Invite failed: file doesn't exist",34,MSG_DONTWAIT);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                break;
                            }
                            else if(res == 0){
                                n = send(i,"Invite failed: file owner didn't match",39,MSG_DONTWAIT);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                break;
                            }

                            //check whether the destination is active user
                            if(!isActive(dest_id)){
                                n = send(i,"Invite failed: destination client not found",44,MSG_DONTWAIT);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                } 
                                break;
                            }

                            //checking if the destination already has those preveleges
                            if(has_perm(dest_id,file_name,perm)){
                                 n = send(i,"Invite failed: destination already has permission",50,MSG_DONTWAIT);
                                 if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                break;
                            }

                            //check for duplicate invtes
                            if(duplicate_invite(i+10000,file_name,dest_id,perm)){
                                n = send(i,"Invite failed: duplicate invite",31,MSG_DONTWAIT);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                break;
                            }

                            //prepare invite messsage
                            bzero(buffer,BUFFER_SIZE);
                            sprintf(buffer,"/invite request: %s %c sender: %d",file_name,perm,i+10000);
                        
                            //send invite request to the destination client id
                            n = send(dest_id-10000,buffer,BUFFER_SIZE,MSG_DONTWAIT);
                            if(n <= 0){
                                //checking whether connection is closed by client
                                if(n == 0){
                                    int t;
                                    t = send(i,"Invite send failed: client closed connection",45,MSG_DONTWAIT);
                                    if(t == 0){
                                        printf("Send error: source %d closed connection\n",i+10000);
                                        closeSocket(i,max_fd,&master_set);
                                    }
                                    printf("%d Client closed the connection\n",dest_id);
                                    closeSocket(dest_id-10000,max_fd,&master_set);
                                }
                                //might need to chek for EAGAIN
                                if(errno != EWOULDBLOCK){
                                    int t;
                                    t = send(i,"Invite send failed",20,MSG_DONTWAIT);
                                    if(t == 0){
                                        printf("Send error: source %d closed connection\n",i+10000);
                                        closeSocket(i,max_fd,&master_set);
                                    }
                                    perror("ERROR: send() failed");
                                    closeSocket(dest_id,max_fd,&master_set);
                                } 
                                break;
                            }

                            //create invite record and add it to the link list
                            invite_record *new_record = malloc(sizeof(invite_record));
                            new_record->source = i+10000;
                            new_record->dest = dest_id;
                            strcpy(new_record->file_name,file_name);
                            new_record->permission = perm;
                            
                            //adding the record to end of linklist
                            invite_record *temp = invite_record_head;
                            if(temp == NULL){
                                invite_record_head = new_record;
                                new_record->next = NULL;
                            }
                            else{
                                while(temp->next != NULL){
                                    temp = temp->next;
                                }
                                temp->next = new_record;
                                new_record->next = NULL;
                            }
                        }
                        else if(!strncmp(cmd,"/files",6)){
                            char *perm_data = get_file_records();
                            if(perm_data == NULL){
                                perror("ERROR obtaining list of files");
                                break;
                            }
                            if(!strcmp(perm_data,"")){
                                n = send(i,"No files uploaded yet!",23,MSG_DONTWAIT);
                            }
                            else{
                                n = send(i,perm_data,strlen(perm_data),MSG_DONTWAIT); 
                                free(perm_data);
                            } 
                            if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                break;
                            }          
                        }
                        else if(!strncmp(cmd,"/download",9)){
                            int client_id = 10000+i;
                            
                            char file_name[255] = {0};

                            //extract file name
                            sscanf(buffer,"%*s %s",file_name);

                            //check if client has required permissions to read the file.
                            int res = isfileOwner(file_name,client_id);
                            if(res == -1){
                                //file doesnot exists
                                size_t msg_size = 37;
                                n = send(i,&msg_size,sizeof(msg_size),0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                } 
                                n = send(i,"Download Failed: File doesnot exists",msg_size,0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                break;
                            }
                            else if(res || has_perm(client_id,file_name,'V') || has_perm(client_id,file_name,'E')){
                                //client is either file owner or has view and edit permissions.
                                char file_path[256] = {0};
                                strcpy(file_path,client_uploads_path);
                                strcat(file_path,file_name);

                                FILE *fd = fopen(file_path,"r");
                                if(fd == NULL){
                                    perror("ERROR opening file");
                                    size_t msg_size = 19;
                                    n = send(i,&msg_size,sizeof(msg_size),0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }

                                    n = send(i,"ERROR: opening file",msg_size,0);
                                    if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                        break;
                                    }

                                    break;
                                }

                                //calculate size of file
                                fseek(fd,0,SEEK_END);
                                size_t file_size = ftell(fd) + strlen(file_name) + 1;

                                //send size of file to client
                                n = send(i, &file_size, sizeof(file_size), 0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    fclose(fd);
                                    break;
                                }

                                //copy contents of file to buffer
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
                                n = send(i,file_data,file_size, 0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    fclose(fd);
                                    free(file_data);
                                    break;
                                }

                                //cleanup
                                fclose(fd);
                                free(file_data);                                 
                            }
                            else{
                                //client doesn't have required permissions
                                size_t msg_size = 44;
                                n = send(i,&msg_size,sizeof(msg_size),0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
 
                                n = send(i,"Download Failed: Not permitted to read file",msg_size,0);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    break;
                                }
                                 
                                break;
                            }
                        }
                        else if(!strncmp(cmd,"/upload",7)){
                            size_t file_size = 0;
                            char file_name[32] = {0};

                            //recive size of file to upload
                            n = recv(i,&file_size,sizeof(file_size),0);
                            if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                break;
                            }

                            //recieve file data from client.
                            char *file_data = malloc(sizeof(char)*file_size+1);
                            bzero(file_data,file_size+1);
                            n = recv(i,file_data,file_size,0);      
                            if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                free(file_data);
                                break;
                            }

                            sscanf(file_data,"%[^\n]s\n",file_name);
                            printf("user %d: Upload file name: %s file size: %ld\n",i+10000,file_name,file_size);

                            //saving the data in file
                            int tmp = 0;
                            if((tmp = save_data(file_data,file_name,i+10000)) < 0){
                                perror("ERROR:");
                                n = send(i,"File upload failed",19,MSG_DONTWAIT);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    free(file_data);
                                    break;
                                }
                            }
                            else if(tmp == 1){
                                n = send(i,"File already exists upload failed",34,MSG_DONTWAIT);   
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    free(file_data);
                                    break;
                                }     
                            }
                            else{
                                n = send(i,"File uploaded succesfully",26,MSG_DONTWAIT);
                                if(check_send_recv_errors(n,i,max_fd,&master_set) < 0){
                                    free(file_data);
                                    break;
                                }
                            }
                            free(file_data);

                        }
                        else if(!strncmp(cmd,"/exit",5)){
                            printf("%d client exit\n",i+10000);
                            closeSocket(i,max_fd,&master_set);
                            break;
                        }
                    } while(1);
                }//end else
            }//end if FD_ISSET
        }//end for
    }while(1);
    return 0;
}
