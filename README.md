# sbitx2pskreporter
WIP: A realtime and postponed spots uploader for the sbitx radio

This is a work in progress for building an agent that:
  - connects to the sbitx radio software via telnet
  - detects in realtime the FT8 traffic
  - reports it to PSKReporter
  - If the network is not available (e.g. during a portable activity) then
the packets will be stored in a spool area and sent when the network
is available again (e.g. when coming back home and switching on the agent on the radio)

 Author: Fabrizio Furano, 2025
