help:
	$(MAKE) -f Makefile help
	echo Suggestion: '"make linuxhost"'
linuxhost:
	$(MAKE) HOSTOS=linux -f Makefile
winnthost:
	$(MAKE) HOSTOS=winnt -f Makefile
%:
	$(MAKE) -f Makefile $*
.PHONY: % help linuxhost winnthost
