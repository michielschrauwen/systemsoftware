
Compile the libraries like this for tcpsock.c and dplist.c :

gcc -c -Wall -Werror -fpic dplist.c

gcc -shared -o libdplist.so dplist.o

gcc -c -Wall -Werror -fpic tcpsock.c

gcc -shared -o libtcpsock.so tcpsock.o

move both .so files to lib folder

Compilation of final:

gcc -L/home/michiel/sysprog2018b/final/lib -Wall -std=c11 -Werror -o test.out main.c sbuffer.c connmgr.c datamgr.c sensor_db.c -ldplist -ltcpsock -pthread -lsqlite3 -DTIMEOUT=5 -DSET_MAX_TEMP=21 -DSET_MIN_TEMP=19 -DDEBUG_PARENT -DDEBUG_CHILD
export LD_LIBRARY_PATH=/home/michiel/sysprog2018b/final/lib
./test.out 5678

or define the library location while compiling:

gcc main.c connmgr.c datamgr.c sbuffer.c sensor_db.c -Wall -Werror -o test.out -lm -L./lib -Wl,-rpath=./lib -ltcpsock -ldplist -lpthread -lsqlite3 -DTIMEOUT=5 -DSET_MAX_TEMP=20 -DSET_MIN_TEMP=10 -DDEBUG_PARENT -DDEBUG_CHILD


run ./test.out 5678 in one terminal and in another run one of the sensor simulation files from lab8:

sensors_finite24sdev50.out

*finite24 means the measurements stop after 24seconds
*dev50 means there is a deviation of 50 which means a 5°C deviation

*you can create these files yourself by looking at the differences between sensor_node.c and sensor_node_loops.c
*compile those files to change temperature deviation again
*mainsensorsdev50.c runs multiple sensors together using threads
