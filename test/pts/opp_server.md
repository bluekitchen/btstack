# OPP Server
## Tool: opp_server_demo
OPP/SR/OPH/BV-01-I: wait
OPP/SR/OPH/BV-02-I: wait
OPP/SR/OPH/BV-03-I: wait
OPP/SR/OPH/BV-04-I: wait
OPP/SR/OPH/BV-07-I: wait
OPP/SR/OPH/BV-08-I: wait
OPP/SR/OPH/BV-11-I: wait
OPP/SR/OPH/BV-12-I: wait
OPP/SR/OPH/BV-15-I: wait
OPP/SR/OPH/BV-16-I: wait
OPP/SR/OPH/BV-17-I: wait
OPP/SR/OPH/BV-23-I: wait
OPP/SR/OPH/BV-24-I: wait
OPP/SR/OPH/BV-25-I: wait
OPP/SR/OPH/BV-26-I: wait
OPP/SR/OPH/BV-31-I: wait
OPP/SR/OPH/BV-32-I: wait
OPP/SR/OPH/BV-34-I: wait

OPP/SR/BCP/BV-01-I: wait
OPP/SR/BCP/BV-03-I: wait

OPP/SR/BCE/BV-01-I: wait
OPP/SR/BCE/BV-03-I: wait
OPP/SR/BCE/BV-05-I: wait

OPP/SR/GOEP/CON/BV-02-C: wait

OPP/SR/GOEP/BC/BV-01-I: wait

OPP/SR/GOEP/BC/ROP/BV-01-I: wait
OPP/SR/GOEP/BC/ROP/BV-02-I: wait


ToDo:
- provide command to reject next PUT operation
  - OPP/SR/OPH/BV-05-I: INCONC To verify that, if a vCard item is rejected by the user, it is not stored in the corresponding application or the object store.
  - OPP/SR/OPH/BV-09-I: INCONC To verify that, if a vCal item is rejected by the user, it is not stored in the corresponding application or the object store.
  - OPP/SR/OPH/BV-13-I: INCONC To verify that, if a vMsg item is rejected by the user, it is not stored in the corresponding application or the object store. 
  - OPP/SR/BCE/BV-06-I: INCONC: The IUT must reject this PUT operation.  Please restart the test case and try again.
  - OPP/SR/BCE/BV-07-I: INCONC: The IUT must reject this PUT operation.  Please restart the test case and try again
- inform about abort request
  - OPP/SR/OPH/BV-27-I: NOT IMPLEMENTED - OBEX PUSH with Abort. Data starts with BMV. (not on wikipedia) demo needs to report ABORT operation
- other:
  - OPP/SR/BCP/BV-04-I: FAIL - OBEX PULL, Demo rejects it (it shouldn't)
  - OPP/SR/BCP/BV-05-I: FAIL - OBEX PULL, Demo should actively rejects it (it is rejected accidentally)
  - OPP/SR/BCE/BV-04-I: INCONC: IUT did not complete the business card pull operation as requested.
  - OPP/SR/GOEP/BC/BV-03-I: INCONC: IUT did not complete the business card pull operation as requested.
- SRM / SRMP
  - OPP/SR/GOEP/SRM/BI-02-C
  - OPP/SR/GOEP/SRM/BI-03-C
  - OPP/SR/GOEP/SRM/BI-05-C
  - OPP/SR/GOEP/SRM/BV-04-C
  - OPP/SR/GOEP/SRM/BV-05-C
