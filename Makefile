descend = \
	+mkdir -p $(OUTPUT)$(1) && \
	$(MAKE) $(COMMAND_O) subdir=$(if $(subdir),$(subdir)/$(1),$(1)) $(PRINT_DIR) -C $(1) $(2)

help:
	@echo 'Possible targets:'
	@echo ''
	@echo '  atl_lib	       - atl library'
	@echo '  lib               - igb library'
	@echo ''
	@echo '  daemons_all       - build all daemons (mrpd maap shaper)'
	@echo '  mrpd              - mrpd daemon'
	@echo '  maap              - maap daemon'
	@echo '  shaper            - shaper daemon for linux'
	@echo ''
	@echo '  avtp_pipeline     - AVTP pipeline'
	@echo '  avtp_pipeline_doc - AVTP pipeline doc'
	@echo '  avtp_avdecc       - AVTP avdecc'
	@echo '  avtp_avdecc_doc   - AVTP avdecc doc'
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

atl_lib: FORCE
	$(call descend,lib/atl_avb/lib)
	$(call descend,lib/common)

atl_lib_clean:
	$(call descend,lib/atl_avb/lib/,clean)
	$(call descend,lib/common/,clean)

lib: FORCE
	$(call descend,lib/igb_avb/lib)
	$(call descend,lib/common)

lib_clean:
	$(call descend,lib/igb_avb/lib/,clean)
	$(call descend,lib/common/,clean)

mrpd:
	$(call descend,daemons/$@)

mrpd_clean:
	$(call descend,daemons/mrpd/,clean)

maap:
	$(call descend,daemons/$@/linux/build/)

maap_clean:
	$(call descend,daemons/maap/linux/build/,clean)

shaper:
	$(call descend,daemons/$@)

shaper_clean:
	$(call descend,daemons/shaper/,clean)

daemons_all: mrpd maap shaper

daemons_all_clean: mrpd_clean maap_clean shaper_clean

examples_common:
	$(call descend,examples/common)

examples_common_clean:
	$(call descend,examples/common/,clean)

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

simple_rx:
	$(MAKE) lib
	$(call descend,examples/$@)

simple_rx_clean:
	$(call descend,examples/simple_rx/,clean)

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

avtp_pipeline: lib atl_lib
	$(MAKE) -s -C lib/avtp_pipeline -f avtp_pipeline.mk

avtp_pipeline_clean:
	$(MAKE) -s -C lib/avtp_pipeline -f avtp_pipeline.mk clean

avtp_pipeline_doc: lib
	$(MAKE) -s -C lib/avtp_pipeline -f avtp_pipeline.mk doc

avtp_avdecc: lib
	$(MAKE) -s -C lib/avtp_pipeline -f avtp_avdecc.mk

avtp_avdecc_clean:
	$(MAKE) -s -C lib/avtp_pipeline -f avtp_avdecc.mk clean

avtp_avdecc_doc: lib
	$(MAKE) -s -C lib/avtp_pipeline -f avtp_avdecc.mk doc

examples_all: examples_common simple_talker simple_listener mrp_client live_stream jackd-talker \
	jackd-listener simple_rx

examples_all_clean: examples_common_clean simple_talker_clean simple_listener_clean mrp_client_clean \
	jackd-talker_clean jackd-listener_clean live_stream_clean simple_rx_clean

all: lib atl_lib daemons_all examples_all avtp_pipeline avtp_avdecc

clean: lib_clean atl_lib_clean daemons_all_clean examples_all_clean avtp_pipeline_clean avtp_avdecc_clean

.PHONY: FORCE
