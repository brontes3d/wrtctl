TBD=$(top_builddir)
if ENABLE_STUNNEL
STUNNEL=1
else
STUNNEL=0
endif

util_scripts = py-wrapper.sh wrtctld-wrapper.sh
all: $(util_scripts)

SED_REGEXP =  -e "s,[@]TOP_BUILDDIR[@],$(TBD),g" \
	-e "s,[@]STUNNEL[@],$(STUNNEL),g" 

wrtctld-wrapper.sh: wrtctld-wrapper.sh.in
	sed $(SED_REGEXP) < $< > $@
	chmod +x $@

py-wrapper.sh: py-wrapper.sh.in
	sed $(SED_REGEXP) < $< > $@
	chmod +x $@


clean-local:
	rm -f wrtctld-wrapper.sh py-wrapper.sh

