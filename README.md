# Rowing Performance Monitoring System Source
This is the soucecode for the Rowing Performance Monitoring System (RPMS). This zip file contains the following files:
* RPMS.zip
  - Contains the main programs for the base and satelite
  - Base.ino
  - Satelite.ino
* Libraries.zip
  - Contains the protocol and physical layer and also a header file for general definitions in the entire sourcecode
  - rfpr.h
  - rfpr.cpp
  - rfphy.h
  - rfphy.cpp
  - rfapp.h
* Tools.zip
  - Contains some of the tests used in developing of the program
  - Console.zip
    - Contains a program for testing communication with the RF module to a slave.
    - Master.ino
    - Slave.ino
  - PhySpeedTest.zip
    - Contains a program which determines how much time it takes for the RF module for sending and receiving a package of a specific size. Used for UPPAAL times
    - Master.ino
    - Slave.ino

## Compiling the program
To compile the program there are a number of steps needed to be performed:

1. Install Arduino IDE (Arduino IDE can be downloaded at https://www.arduino.cc/en/Main/Software)
2. Install library by opening Arduino IDE and select menu Sketch -> Include library -> Add .ZIP Library and then select the Libraries.zip zip file
3. Download the library QueueArray from http://playground.arduino.cc/Code/QueueArray and add library (same procedure as step 2)
4. Open either Base.ino or Satelite.ino
5. Compile the program by clicking on the verify button

## Uploading Base.ino to Arduino UNO
It's possible to upload the Base.ino to a Arduino UNO by clicking upload in the Arduino IDE. The board needs to be connected as follows:

Arduino pin	  | RFM12B pin
------------- | -------------
13            | SCK
12            | SDO
11            | SDI
10            | nSEL
2             | nIRQ

It's however not possible to use the base on the ATTiny84.

## Uploading Satelite.ino to ATTiny84
For uploading the satelite a number of configuration changes is first needed for the Arduino IDE please follow this guide for setting up the IDE and prepare a ISP:
http://highlowtech.org/?p=1695

Upload the Satelite.ino normally to the ATTiny84 via the ISP (the code is not supported on Arduino UNO).

The diagram of the satelite can be found on the included CD for the project.