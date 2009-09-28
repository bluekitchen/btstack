if USE_SPRINGBOARD
    springboardaccess = SpringBoardAccess
endif

AUTOMAKE_OPTIONS = foreign
SUBDIRS = src example $(springboardaccess)

iphone_ip=@IPHONE_IP@
install-iphone: src/BTdaemon
	scp $< root@$(iphone_ip):/usr/local/bin/BTdaemon
	scp resources/ch.ringwald.BTstack.plist root@$(iphone_ip):/Library/LaunchDaemons/
