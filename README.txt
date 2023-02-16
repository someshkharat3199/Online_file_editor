Assignment 3
Roll No: 21CS60R40

1. server compilation: gcc server.c -o server
client compilation: gcc client.c -o client

2. When server is started it creates "client_uploads" directory automatically. This directory is used to store the files uploaded by the client.

3. Each client when successfully connected to server automatically creates directory "local_<client_id>" and a sub directory "Downloads". 

4. To upload a file it is necessary to place the file under "local_<client_id>" directory of corresponding client.

5. All files downloaded by the client will be stored under "local_<client_id>/Downloads" directory.

6. To support multiline commands we use '$' as delimiter. Every command (except response to invite request) should end with '$' character.
eg: /users commands should be written as : /users$<ENTER>
files command : /files$<ENTER>
upload command: /upload <file_name>$<ENTER>
download command: /download <file_name>$<ENTER>
invite command: /invite <file_name> <client_id> <permission>$<ENTER>
read command: /read <file_name> <start_idx> <end_idx>$<ENTER>
insert command: /insert <file_name> <index> "<message>"$<ENTER>
exit command: /exit$<ENTER>

7. NOTE: reply of an invite need not end with '$' character.
reply of an invite should be Yes/No with out any delimiter.
eg: Yes<ENTER>
     No<ENTER>

8. following format is followed to display users list
<user_id> <socket_id>

9. following format is followed to display list of files
<file_name> lines: <number of lines>, owner: <client_id>, V: <space-seperated list of client_ids with view permission>,E: <space-seperated list of client_ids with edit permission>

10. following format is followed to display invite request
	/invite request: <file_name> <permission> sender: <sender client_id>