
Compile the code using the provided makefile:

    make

To clear the directory (and remove .txt files):
   
    make clean

To run everything
    chmod +x startup.sh
    ./startup.sh

    // Make sure to kill all processes before running again
    // After killing all processes wait for a little
    

To run the client  

    ./tsc -h 0.0.0.0 -p [coordinator_port] -i [id]

