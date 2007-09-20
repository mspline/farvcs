SVN_DIR ?= c:/misc/svn

OBJFILES = farvcs.obj miscutil.obj plugutil.obj vcs.obj regwrap.obj
RESFILES = farvcs.res
DEFFILE  = farvcs.def

LIBS += advapi32.lib shell32.lib

INCLUDES_SVN = ${SVN_DIR}/include ${SVN_DIR}/include/apr ${SVN_DIR}/include/apr-util

%.res : %.rc
	rc $<

%.obj : %.cpp
	cl -Zi -c ${CXXRT} -W4 -EHsc -D_CRT_SECURE_NO_DEPRECATE -DWIN32 $(addprefix -I,${INCLUDES_SVN}) $<

                CXXRT = -MT
farvcs_svn.obj: CXXRT = -MD

farvcs.dll : farvcs_cvs.vcs farvcs_svn.vcs ${OBJFILES} ${RESFILES} ${DEFFILE} farvcs_en.lng
	link -out:$@ -debug -dll -incremental:no -def:${DEFFILE} ${OBJFILES} ${RESFILES} ${LIBS}

install : farvcs.dll farvcs_cvs.vcs farvcs_svn.vcs
	mkdir -p "${PROGRAMFILES}/Far/Plugins/FarVCS"
	pskill far.exe
	sleep 4
	cp -p farvcs.dll "${PROGRAMFILES}/Far/Plugins/FarVCS"
	cp -p farvcs_cvs.vcs "${PROGRAMFILES}/Far/Plugins/FarVCS"
	cp -p farvcs_svn.vcs "${PROGRAMFILES}/Far/Plugins/FarVCS"
	"${PROGRAMFILES}/Far/Far.exe"

clean :
	rm -f *.obj *.map *.lib *.pdb *.exp *.[Rr][Ee][Ss] *.dll *.vcs *.manifest *.user

LIBS_CVS = advapi32.lib
OBJFILES_CVS = farvcs_cvs.obj miscutil.obj plugutil.obj regwrap.obj

farvcs_cvs.vcs : ${OBJFILES_CVS}
	link -out:$@ -debug -dll -incremental:no ${OBJFILES_CVS} ${LIBS_CVS}

LIBS_SVN += advapi32.lib shell32.lib kernel32.lib ws2_32.lib

LIBS_SVN += ${SVN_DIR}/lib/libsvn_client-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_delta-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_diff-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_fs-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_fs_base-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_fs_fs-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_ra-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_ra_dav-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_ra_local-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_ra_svn-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_repos-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_subr-1.lib
LIBS_SVN += ${SVN_DIR}/lib/libsvn_wc-1.lib
LIBS_SVN += ${SVN_DIR}/lib/apr/libapr-1.lib
LIBS_SVN += ${SVN_DIR}/lib/apr-util/libaprutil-1.lib
LIBS_SVN += ${SVN_DIR}/lib/apr-util/xml.lib
LIBS_SVN += ${SVN_DIR}/lib/neon/libneon.lib
LIBS_SVN += ${SVN_DIR}/lib/intl3_svn.lib
LIBS_SVN += ${SVN_DIR}/lib/libdb44.lib

OBJFILES_SVN = farvcs_svn.obj

farvcs_svn.vcs : ${OBJFILES_SVN}
	link -out:$@  -debug -dll -incremental:no ${OBJFILES_SVN} ${LIBS_SVN} | grep -v LNK4099
	mt -manifest $@.manifest -outputresource:"$@;#2"
