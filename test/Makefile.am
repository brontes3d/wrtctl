TBD=$(top_builddir)
TSD=$(top_srcdir)

if ENABLE_STUNNEL
STUNNEL=1
else
STUNNEL=0
endif

RUN_TESTS = shell_test
if ENABLE_PYTHON
RUN_TESTS += python_test
endif

EXTRA_DIST = test.sh.in test.py.in start-wrtctld.sh.in \
	systest/initd.test systest/shutdown

test.sh: test.sh.in
	sed \
		-e "s,[@]TOP_BUILDDIR[@],$(TBD),g" \
		-e "s,[@]TOP_SRCDIR[@],$(TSD),g" \
		-e "s,[@]STUNNEL[@],$(STUNNEL),g" \
		< $< > $@
	chmod +x $@

test.py: test.py.in
	sed \
		-e "s,[@]TOP_BUILDDIR[@],$(TBD),g" \
		-e "s,[@]TOP_SRCDIR[@],$(TSD),g" \
		-e "s,[@]STUNNEL[@],$(STUNNEL),g" \
		< $< > $@

start-wrtctld.sh: start-wrtctld.sh.in
	sed \
		-e "s,[@]TOP_BUILDDIR[@],$(TBD),g" \
		-e "s,[@]TOP_SRCDIR[@],$(TSD),g" \
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

shell_test: test.sh
	./test.sh

python_test: test.py start-wrtctld.sh
	python test.py
	
check: stunnel.pem $(RUN_TESTS)

clean-local:
	rm -rf test.sh config test.log wrtctld.log stunnel.pem test.py \
		start-wrtctld.sh start-wrtctl.pid initd.test.log start-wrtctld.pid \
		shutdown.test.log
