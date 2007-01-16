OBJFILES = farvcs.obj miscutil.obj plugutil.obj cvsentries.obj regwrap.obj svn.obj
RESFILES = farvcs.res
DEFFILE  = farvcs.def

LIBS += advapi32.lib shell32.lib

#LIBS += kernel32.lib ws2_32.lib oldnames.lib
#LIBS += libsvn_client-1.lib libsvn_delta-1.lib libsvn_diff-1.lib libsvn_fs-1.lib libsvn_fs_base-1.lib libsvn_fs_fs-1.lib
#LIBS += libsvn_ra-1.lib libsvn_ra_dav-1.lib libsvn_ra_local-1.lib libsvn_ra_svn-1.lib libsvn_repos-1.lib
#LIBS += libsvn_subr-1.lib libsvn_wc-1.lib libapr.lib libaprutil.lib xml.lib libneon.lib intl3_svn.lib libdb44.lib

%.res : %.rc
	rc $<

%.obj : %.cpp
	cl -Zi -c -MT -W4 -EHsc -D_CRT_SECURE_NO_DEPRECATE -DWIN32 $<

farvcs.dll : ${OBJFILES} ${RESFILES} ${DEFFILE} farvcs_en.lng
	link -debug -dll -incremental:no -def:${DEFFILE} ${OBJFILES} ${RESFILES} ${LIBS}

install : farvcs.dll
	mkdir -p "${PROGRAMFILES}/Far/Plugins/FarVCS"
	pskill far.exe
	sleep 4
	cp -p $< "${PROGRAMFILES}/Far/Plugins/FarVCS"
	"${PROGRAMFILES}/Far/Far.exe"

clean :
	rm -f *.obj *.map *.lib *.pdb *.exp *.[Rr][Ee][Ss] *.dll
