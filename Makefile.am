if USE_SPRINGBOARD
    springboardaccess = SpringBoardAccess
endif

AUTOMAKE_OPTIONS = foreign
SUBDIRS = src example $(springboardaccess)

iphone_ip=@IPHONE_IP@
install-iphone: src/BTdaemon
	cp $< $<-signed
	export CODESIGN_ALLOCATE=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate ; ldid -S $<-signed
	scp $<-signed root@$(iphone_ip):/usr/local/bin/BTdaemon
	scp resources/ch.ringwald.BTstack.plist root@$(iphone_ip):/Library/LaunchDaemons/
