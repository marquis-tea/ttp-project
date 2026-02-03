This system simulates a simple ATM Machine Transaction System using Linux system calls. It can only be run in a Linux environment. The program require to run at least two separate machines for client and server (Although one machine is possible too).

Steps to run:
1. Ensure your client and server machines are running on the same network.
2. Go to line 27 on server.c and line 59 on client.c and change the IP address to that of the server machine.
3. Compile the .c files on both server and client machines using
gcc server.c -o server
gcc logger.c -o logger
gcc client.c -o client
4. On server machine, run "./server". It will now wait for client connection.
5. On client machine, run "./client". It will now connect to server if available.
