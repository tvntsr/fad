# File audit daemon (fad)

## Dependency
To get FAD bult correctly the following dependencies should be met:
* g++ 9.3.0 or up
* Boost libraries 1.71 or up

The following BOOST libraries are used:
* Boost ASIO
* Program options
* String algorithms

## Build
There is a makefile and the following targets are supported:
- all
- clean
- install
- uninstall

## systemd
There is fad.service unit file, which is installed on `make install` and removed on `make uninstall`

## signal handling
The following signals are catched: SIGUSR1, SIGINT, SIGTERM

### SIGUSR1
Log and report files are closed, and opened again. To get logs rotated, do

    mv /var/log/fad.log /var/log/fad.log.1 && 
    mv /var/log/fad.report /var/log/fad.report.1 &&
    kill -USR1 `cat /var/run/fad.pid`


### SIGINT, SIGTERM
Terminates the program

## configuration file
Symple ini-based text file, by default should be listed /etc/

       daemon=true
       pidfile=/var/run/fad.pid
       logfile=/var/log/fad.log
       loglevel=Debug
       report=/var/log/fad.report
       watch=/tmp
       watch=/var/tmp
       watch=/etc

### daemon
Act as daemon, true or false

### pidfile
File to store fad's process id

### logfile
Define the log file

### loglevel
Defines log level, the following values are accepted:
* LogDebug,
* LogInfo,
* LogWarn,
* LogError,
* LogFatal

### report
File to keep records about the access to the watched directories

### watch
Directory to be watched, multiple values are allowed

## Command line options
Program options:

    -h [ --help ]                      produce help message
    -c [ --conf ] arg (=/etc/fad.conf) defines configuration file
    -d [ --daemon ]                    program acts as daemon
    
