# sbitx2pskreporter

A realtime and postponed spots uploader for the sbitx radio

This is an agent that:
  - connects to the sbitx radio via telnet
  - detects in realtime the FT8 traffic
  - reports it to PSKReporter
  - If the network is not available (e.g. during a portable activity) then
the packets will be stored in a spool area and sent when the network
is available again (e.g. when coming back home and switching on the agent on the radio)

 How to build:
 Make sure that you have installed
 - qt6
 - libtelnet2
 - libtelnet-devel
   
 $ qmake6 ./sbitx2pskreporter.pro
 $ make
 $ sudo make install

 How to use:

 - Edit the configuration file and write down your callsign/locator/antenna
 - run the application in your sbitx
 - use the sbitx radio in ft8 mode. The decoded spots will be sent to PSKReporter

 IMPORTANT NOTE: As of 07 Jan 2026 this application needs a small addition to the sbitx code
 that is not yet in the repository. Let's fix this.
 
 Author: Fabrizio Furano, 2025
