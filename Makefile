all: main_core
PROJECT_SOURCEFILES += mqtt_sn.c

WITH_UIP6=1
UIP_CONF_IPV6=1
CFLAGS+= -DUIP_CONF_IPV6_RPL
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

# Adicionada estas duas linhas de flags para reduzir tamanho do firmware que não cabe no espaço de rom do msp430 que é utilizado na simulação
CFLAGS += -ffunction-sections
LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__

CONTIKI=../
include $(CONTIKI)/Makefile.include
