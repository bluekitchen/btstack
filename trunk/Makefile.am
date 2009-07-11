AUTOMAKE_OPTIONS = foreign
SUBDIRS = src

iphone_ip=@IPHONE_IP@
install-iphone: src/BTdaemon
	cp $< $<-signed
	export CODESIGN_ALLOCATE=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate ; ldid -S $<-signed
	scp $<-signed mobile@$(iphone_ip):/var/mobile/BTdaemon

