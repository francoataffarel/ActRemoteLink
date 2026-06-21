PS5_HOST ?= ps5
PS5_PORT ?= 9021
AGENT_PORT ?= 31337

ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

AGENT_ELF := actremotelink_agent.elf
PIN_NOTIFY_ELF := actremotelink_pin_notify.elf

AGENT_CFLAGS := -Wall -g -DAGENT_PORT=$(AGENT_PORT)
AGENT_LDADD := -lSceRegMgr -lSceUserService -lSceSystemService

PIN_NOTIFY_CFLAGS := -Wall -g
PIN_NOTIFY_LDADD := -lSceRegMgr -lSceUserService -lSceRemoteplay

all: $(AGENT_ELF) $(PIN_NOTIFY_ELF)

$(AGENT_ELF): actremotelink_agent.c
	$(CC) $(AGENT_CFLAGS) -o $@ $^ $(AGENT_LDADD)

$(PIN_NOTIFY_ELF): actremotelink_pin_notify.c
	$(CC) $(PIN_NOTIFY_CFLAGS) -o $@ $^ $(PIN_NOTIFY_LDADD)

clean:
	rm -f $(AGENT_ELF) $(PIN_NOTIFY_ELF)

test: $(AGENT_ELF)
	$(PS5_DEPLOY) -h $(PS5_HOST) -p $(PS5_PORT) $^

pin-notify-send: $(PIN_NOTIFY_ELF)
	socat -t 99999999 - TCP:$(PS5_HOST):$(PS5_PORT) < $(PIN_NOTIFY_ELF)
