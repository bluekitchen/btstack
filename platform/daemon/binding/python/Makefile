BTSTACK_ROOT=../../../../

all: btstack/command_builder.py btstack/event_factory.py

btstack/command_builder.py: ${BTSTACK_ROOT}/tool/python_generator.py ${BTSTACK_ROOT}/src/bluetooth.h ${BTSTACK_ROOT}/src/btstack_defines.h
	${BTSTACK_ROOT}/tool/python_generator.py

btstack/event_factory.py: ${BTSTACK_ROOT}/tool/python_generator.py ${BTSTACK_ROOT}/src/bluetooth.h ${BTSTACK_ROOT}/src/btstack_defines.h
	${BTSTACK_ROOT}/tool/python_generator.py

clean:
	rm -f btstack/command_builder.py btstack/event_factory.py
