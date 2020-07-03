      ______                              _
     / _____)             _              | |
    ( (____  _____ ____ _| |_ _____  ____| |__
     \____ \| ___ |    (_   _) ___ |/ ___)  _ \
     _____) ) ____| | | || |_| ____( (___| | | |
    (______/|_____)_|_|_| \__)_____)\____)_| |_|
        (C)2016 Semtech

SX1280 drivers C
=======================

1. Introduction
----------------
This project feature a C porting of the SX1280 driver originally developped in CPP on top of mbed library.

2. Repository work flow
-----------------------
The work flow of the repository applies to the gitflow work flow. For detailed information, please take a look at: https://github.com/nvie/gitflow or http://nvie.com/posts/a-successful-git-branching-model/. 
The repository contains different branches:
* master 
  The master branch contains the only stable versions which passes the LoRaWAN compliance tests. Therefore if you checkout the repo without switching the branch, you're automatically using the most recent stable version of the stack.
* develop 
  The develop branch provides updates to master branch. It shows the progress of development for the repository. When the develop branch is ready for a release, a release branch will be created at this point. The repository must contain only one develop branch! In general, the develop branch is not a stable version and might not pass the LoRaWAN compliance tests.
* release 
  As soon as we branch from develop to release branch, no further features will be integrated for the upcoming release. This branch is basically for polishing and fixing issues that relevant for the upcoming release only. As soon as this version passes the compliance test, it'll be merged into master branch.
* feature 
  A feature branch reflects the development process on a specific feature. The repository might contain several feature branches. If a feature is ready, the related feature branch will be merged into the develop branch.

A script has been designed to help include this driver with the correct version. Check *repo-init.sh* in firmware projects to see which version of SX1280 driver is in use.



3. Dependencies
----------------

4. Usage
---------

5. Changelog
-------------
See changelog.md file
