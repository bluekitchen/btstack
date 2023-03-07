# EDIT HERE:
MK_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
CURDIR := $(patsubst %/, %, $(dir $(MK_PATH)))

C_SOURCES_MBEDTLS := $(wildcard $(CURDIR)/library/*.c)

C_INCLUDES_MBEDTLS := -I$(CURDIR)/include
C_INCLUDES_MBEDTLS += -I$(CURDIR)/include/mbedtls
C_INCLUDES_MBEDTLS += -I$(CURDIR)/include/psa
C_INCLUDES_MBEDTLS += -I$(CURDIR)/library
C_INCLUDES_MBEDTLS += -I$(CURDIR)/configs
################################################################################