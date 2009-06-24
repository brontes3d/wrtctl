TBD=$(top_builddir)

EXTRA_DIST = savedir

test.sh: test.sh.in
	sed -e "s,[@]TOP_BUILDDIR[@],$(TBD),g" < $< > $@
	chmod +x $@

check: test.sh
	./test.sh

clean-local:
	rm -rf test.sh savedir config test.log wrtctld.log
