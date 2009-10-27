TBD=$(top_builddir)

if ENABLE_STUNNEL
STUNNEL=1
else
STUNNEL=0
endif

EXTRA_DIST = savedir

test.sh: test.sh.in
	sed \
		-e "s,[@]TOP_BUILDDIR[@],$(TBD),g" \
		-e "s,[@]STUNNEL[@],$(STUNNEL),g" \
		< $< > $@
	chmod +x $@

stunnel.pem:
	@echo
	@echo "Creating a temporary self-signed certificate for testing only."
	@echo
	@printf \
		"US\nMA\nBoston\nwrtctl-test.org\n\nwrtctl-test.org\nadmin@wrtctl-test.org\n" \
		| openssl req -new -x509 -days 2 -nodes -out stunnel.pem -keyout stunnel.pem
	@echo

check: test.sh stunnel.pem
	./test.sh

clean-local:
	rm -rf test.sh savedir config test.log wrtctld.log stunnel.pem
