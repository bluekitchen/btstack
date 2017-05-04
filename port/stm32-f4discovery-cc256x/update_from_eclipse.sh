#/bin/sh
PROJECT=/Projects/eclipse-neon-cdt/stm32f4discovery-template
TEMPLATE=eclipse-template

rm -rf $TEMPLATE
mkdir $TEMPLATE

for ITEM in .project .cproject .settings include ldscripts src system stm32f4discovery-template-debug.launch
do
	cp -r $PROJECT/$ITEM $TEMPLATE
done

