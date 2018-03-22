project(
	'libavtp',
	'c',
	version: '0.1',
	license: 'BSD-3-Clause',
)

avtp_lib = library(
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

pkg = import('pkgconfig')
pkg.generate(
	name: 'avtp',
	description: 'AVTP packetization library',
	version: '0.1',
	url: 'github.com/AVnu/OpenAvnu',
	libraries: avtp_lib,
)

test_avtp = executable(
	'test-avtp',
	'unit/test-avtp.c',
	include_directories: include_directories('include'),
	link_with: avtp_lib,
	dependencies: dependency('cmocka'),
	build_by_default: false,
)

test_aaf = executable(
	'test-aaf',
	'unit/test-aaf.c',
	include_directories: include_directories('include'),
	link_with: avtp_lib,
	dependencies: dependency('cmocka'),
	build_by_default: false,
)

test('AVTP API', test_avtp)
test('AAF API', test_aaf)

executable(
	'aaf-talker',
	'examples/aaf-talker.c',
	include_directories: include_directories('include'),
	link_with: avtp_lib,
	build_by_default: false,
)

executable(
	'aaf-listener',
	'examples/aaf-listener.c',
	include_directories: include_directories('include'),
	link_with: avtp_lib,
	build_by_default: false,
)
