.POSIX:
.SUFFIXES:

test: phony
	crystal run test/*_test.cr -- --verbose

phony:
