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
CMake is used to build, the following steps should be done

    mkdir build
    cd build
    cmake ../
    make all

## install
To install run the following fron build directroy

    sudo make install

This step installs all config as well as enabling systemd's fad service


## Uninstall
To uninstall fad do the following steps

    sudo systemctl stop fad
    sudo systemctl disable fad
    sudo systemctl daemon-reload

    xargs rm < install_manifest.txt

## systemd
There is fad.service unit file, which is installed on `make install` To disable fad service run the following code

    sudo systemctl stop fad
    sudo systemctl disable fad
    sudo systemctl daemon-reload


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
The syntax of the record is the following:

    dir [: event [| event]]

The following events are recognized:

* ACCESS

      A file or a directory (but see BUGS) was accessed (read).
* MODIFY

      A file was modified.
* CLOSE_WRITE

      A file that was opened for writing (O_WRONLY or O_RDWR) was closed.
* CLOSE_NOWRITE

      A file or directory that was opened read-only (O_RDONLY) was closed.
* OPEN

      A file or a directory was opened.
* OPEN_EXEC

      A file was opened with the intent to be executed.
* ATTRIB

      A file or directory metadata was changed.
* CREATE

      A child file or directory was created in a watched parent.
* DELETE

      A child file or directory was deleted in a watched parent.
* DELETE_SELF

      A watched file or directory was deleted.
* MOVED_FROM

      A file or directory has been moved from a watched parent directory.
* MOVED_TO

      A file or directory has been moved to a watched parent directory.
* MOVE_SELF

      A watched file or directory was moved.

The following event are defined for convenience:

* ALL

      all possible events
* PATH_EVENTS

      ACCESS | MODIFY | CLOSE | OPEN | OPEN_EXEC
* DIRENT_EVENTS

      MOVE | CREATE | DELETE
* INODE_EVENTS

      DIRENT_EVENTS | FAN_ATTRIB | FAN_MOVE_SELF | FAN_DELETE_SELF

Example of the record:

    watch=/tmp : ACCESS | MODIFY | OPEN
    wtach=/etc : ALL


## Command line options
Program options:

    -h [ --help ]                      produce help message
    -c [ --conf ] arg (=/etc/fad.conf) defines configuration file
    -d [ --daemon ]                    program acts as daemon
    
