NAME = LogExtension

CC = /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-gcc-4.0.1
OBJC     = /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-g++-4.0.1

CPPFLAGS = -I/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.sdk/usr/include \
		   -I/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.sdk/usr/lib/gcc/arm-apple-darwin9/4.0.1/include \
           -F/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.sdk/System/Library/Frameworks

LDFLAGS  = -lobjc -Wl,-x \
           -multiply_defined suppress \
		   -F/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.sdk/System/Library/Frameworks \
		   -L/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.sdk/usr/lib \
		   -framework CoreFoundation \
		   -framework QuartzCore \
		   -framework Foundation \
		   -framework UIKit \
		   /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.sdk/System/Library/PrivateFrameworks/GraphicsServices.framework/GraphicsServices \
           -L3rdparty -lsubstrate

all: $(NAME).dylib

clean:
	rm -f $(NAME).dylib LogMe

$(NAME).dylib: $(NAME).mm
	$(OBJC) $(CPPFLAGS) -dynamiclib -o $@ $(NAME).mm -init _$(NAME)Initialize $(LDFLAGS)