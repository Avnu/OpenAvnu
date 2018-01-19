project(
	'libavtp',
	'c',
	version: '0.1',
	license: 'BSD-3-Clause',
)

library(
	'avtp',
	[
	 'src/avtp.c',
	 'src/avtp_aaf.c',
	],
	include_directories: include_directories('include'),
	install: true,
)

install_headers(
	'include/avtp.h',
	'include/avtp_aaf.h',
)
