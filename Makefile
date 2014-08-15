descend = \
	+mkdir -p $(OUTPUT)$(1) && \
	$(MAKE) $(COMMAND_O) subdir=$(if $(subdir),$(subdir)/$(1),$(1)) $(PRINT_DIR) -C $(1) $(2)

help:
	@echo 'Possible targets:'
	@echo ''
	@echo '  igb               - igb module'
	@echo ''
	@echo '  lib               - igb library'
	@echo ''
	@echo '  daemons_all       - build all daemons (mrpd gptp maap)'
	@echo '  mrpd              - mrpd daemon'
	@echo '  gptp              - gptp daemon for linux'
	@echo '  maap              - maap daemon'
	@echo ''
	@echo '  examples_all      - build all examples (simple_talker simple_listener mrp_client live_stream jackd-talker jackd-listener)'
	@echo '  simple_talker     - simple_talker application'
	@echo '  simple_listener   - simple_listener application'
	@echo '  mrp_client        - mrp_client application'
	@echo '  jackd-talker      - jackd-talker application'
	@echo '  jackd-listener    - jackd-listener application'
	@echo '  live_stream       - live_stream application'
	@echo ''
	@echo 'Cleaning targets:'
	@echo ''
	@echo '  all of the above with the "_clean" string appended cleans'
	@echo '    the respective build directory.'
	@echo '  clean: a summary clean target to clean _all_ folders'
	@echo ''

igb: FORCE
	$(call descend,kmod/$@)

igb_clean:
	$(call descend,kmod/igb/,clean)

lib: FORCE
	$(call descend,lib/igb)

lib_clean:
	$(call descend,lib/igb/,clean)

mrpd:
	$(call descend,daemons/$@)

mrpd_clean:
	$(call descend,daemons/mrpd/,clean)

gptp:
	$(call descend,daemons/$@/linux/build/)

gptp_clean:
	$(call descend,daemons/gptp/linux/build/,clean)

maap:
	$(call descend,daemons/$@/linux/)

maap_clean:
	$(call descend,daemons/maap/linux/,clean)

daemons_all: mrpd maap gptp

daemons_all_clean: mrpd_clean gptp_clean maap_clean

simple_talker:
	$(MAKE) lib
	$(call descend,examples/$@)

simple_talker_clean:
	$(call descend,examples/simple_talker/,clean)

simple_listener:
	$(MAKE) lib
	$(call descend,examples/$@)

simple_listener_clean:
	$(call descend,examples/simple_listener/,clean)

mrp_client:
	$(call descend,examples/$@)

mrp_client_clean:
	$(call descend,examples/mrp_client/,clean)

jackd-talker:
	$(MAKE) lib
	$(call descend,examples/$@)

jackd-talker_clean:
	$(call descend,examples/jackd-talker/,clean)

jackd-listener:
	$(MAKE) lib
	$(call descend,examples/$@)

jackd-listener_clean:
	$(call descend,examples/jackd-listener/,clean)

live_stream:
	$(MAKE) lib
	$(call descend,examples/$@)

live_stream_clean:
	$(call descend,examples/live_stream/,clean)

examples_all: simple_talker simple_listener mrp_client live_stream jackd-talker \
	jackd-listener

examples_all_clean: simple_talker_clean simple_listener_clean mrp_client_clean \
	jackd-talker_clean jackd-listener_clean live_stream_clean

all: igb lib daemons_all examples_all

clean: igb_clean lib_clean daemons_all_clean examples_all_clean

.PHONY: FORCE
