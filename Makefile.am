AUTOMAKE_OPTIONS = foreign
SUBDIRS = src example

iphone_ip=@IPHONE_IP@
install-iphone: src/BTdaemon
	cp $< $<-signed
	export CODESIGN_ALLOCATE=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate ; ldid -S $<-signed
	scp $<-signed root@$(iphone_ip):/var/root/BTdaemon

