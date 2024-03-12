# TBS Server Test Sequences

## Setup
Set TSPX_bd_addr_iut for TBS Server in PTS IXIT

Tool: telephony_bearer_server_test


## Command line options
|Command| Description                                                            |
|:-----:|:-----------------------------------------------------------------------|
|   b   | Switch between controlling the generic bearer and an individual bearer |
|   c   | Generate incoming call                                                 |
|   n   | name some characteristics                                              |
|   u   | update some characteristics                                            |
|   o   | update characteristics with something bigger than ATT_MTU-3            |
|   r   | Put active call to remotely held state                                 |
|   a   | Put active dialing call to alarming state                              |
|   h   | Put active call to locally and remotely held state                     |
|   d   | Disallow Join                                                          |

## Commands vs PTS dialogs
|Dialog                                                                                                      |Command|
|:-----------------------------------------------------------------------------------------------------------|:-----:|
|Please generate incoming call from the Server.                                                              |   c   |
|Please make a call ID(%) to the Remotely Held state                                                         |   r   |
|Please make one call (Call ID:%) to the locally and remotely held state.                                    |   h   |
|Please make a call ID (%) to the Alerting state and send Call State Notification.                           |   a   |
|Please update (characteristic name) characteristic with value whose length is greater than the (ATT_MTU-3). |   o   |
|Please update (characteristic name) characteristic(Handle = %) and send a notification containing the updated value of the characteristic with a different value whose length is greater than the (ATT_MTU-3).  |   o   |
|Please Configure to disallow Join.                                                                          |   d   |
|Please update (characteristic name) characteristic(Handle = %) and send a notification containing the updated value of the characteristic.|   u   |

## Tests:

- TBS/SR/SGGIT/SER/BV-01-C: (wait)

- TBS/SR/SGGIT/CHA/BV-01-C: 
- TBS/SR/SGGIT/CHA/BV-02-C: 
- TBS/SR/SGGIT/CHA/BV-03-C: 
- TBS/SR/SGGIT/CHA/BV-04-C: 
- TBS/SR/SGGIT/CHA/BV-05-C: 
- TBS/SR/SGGIT/CHA/BV-06-C: 
- TBS/SR/SGGIT/CHA/BV-07-C: 
- TBS/SR/SGGIT/CHA/BV-08-C: 
- TBS/SR/SGGIT/CHA/BV-09-C: 
- TBS/SR/SGGIT/CHA/BV-10-C: 
- TBS/SR/SGGIT/CHA/BV-11-C: 
- TBS/SR/SGGIT/CHA/BV-12-C: 
- TBS/SR/SGGIT/CHA/BV-13-C: 
- TBS/SR/SGGIT/CHA/BV-14-C: 
- TBS/SR/SGGIT/CHA/BV-15-C: 
- TBS/SR/SGGIT/CHA/BV-16-C: 

- TBS/SR/SGGIT/CP/BV-01-C: 
- TBS/SR/SGGIT/CP/BV-02-C: 
- TBS/SR/SGGIT/CP/BV-03-C: 
- TBS/SR/SGGIT/CP/BV-04-C: 
- TBS/SR/SGGIT/CP/BV-05-C: 
- TBS/SR/SGGIT/CP/BV-06-C: 
- TBS/SR/SGGIT/CP/BV-07-C: 
- TBS/SR/SGGIT/CP/BV-08-C: 
- TBS/SR/SGGIT/CP/BV-09-C: 
- TBS/SR/SGGIT/CP/BV-10-C: 

- TBS/SR/SGGIT/SP/BV-01-C: 
- TBS/SR/SGGIT/SP/BV-02-C: 

- TBS/SR/SGGIT/SPN/BV-01-C: 
- TBS/SR/SGGIT/SPN/BV-02-C: 
- TBS/SR/SGGIT/SPN/BV-03-C: 
- TBS/SR/SGGIT/SPN/BV-04-C: 
- TBS/SR/SGGIT/SPN/BV-05-C: 
- TBS/SR/SGGIT/SPN/BV-06-C: 
- TBS/SR/SGGIT/SPN/BV-07-C: 
- TBS/SR/SGGIT/SPN/BV-08-C: 
- TBS/SR/SGGIT/SPN/BV-09-C: 
- TBS/SR/SGGIT/SPN/BV-10-C: 
- TBS/SR/SGGIT/SPN/BV-11-C: 

- TBS/SR/SGGIT/SPE/BV-01-C: 
- TBS/SR/SGGIT/SPE/BV-02-C: 
- TBS/SR/SGGIT/SPE/BV-03-C: 
- TBS/SR/SGGIT/SPE/BV-04-C: 
- TBS/SR/SGGIT/SPE/BV-05-C: 
