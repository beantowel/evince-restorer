# Session restorer for Evince pdf reader

Evince is a nice and clean (maybe the best) pdf viewer for ubuntu desktop users. However, it can't restore the last session as most web-browsers do. For users who have opened multiple pdf documents and want to continue their works after evince has been closed or terminated (say, by a shutdown), opening the documents again and again is hard to bear.

The evince-restorer comes to rescue those. The program records the documents opened by evince every 15 seconds, and saves the session 15~30 seconds before the last window of evince was closed. Next time, we can use the program to restore the saved session.

## Usage

Use the application launcher to start the program, it will work until the last window of evince was closed.

## Install

Clone or download the project to the place you want to put the program:
```
$ git clone https://github.com/beantowel/evince-restorer.git
```

change into the directory:
```
$ cd evince-restorer
```

compile the source file and install as a ubuntu application:
```
$ make && make install
```

then you can find the restorer in launcher by the name 'evince-restorer'.
