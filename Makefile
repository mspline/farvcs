%.res : %.rc
	rc $<

%.obj : %.cpp
	cl -Zi -c -MT -W4 -EHsc -D_CRT_SECURE_NO_DEPRECATE $<

farvcs.dll : farvcs.obj miscutil.obj plugutil.obj cvsentries.obj regwrap.obj farvcs.res farvcs.def farvcs_en.lng
	link -debug -dll -incremental:no -def:farvcs.def farvcs.obj miscutil.obj plugutil.obj cvsentries.obj regwrap.obj farvcs.res advapi32.lib shell32.lib

install : farvcs.dll
	mkdir -p "${PROGRAMFILES}/Far/Plugins/FarVCS"
	pskill far.exe
	sleep 4
	cp -p $< "${PROGRAMFILES}/Far/Plugins/FarVCS"
	"${PROGRAMFILES}/Far/Far.exe"

clean :
	rm -f *.obj *.map *.lib *.pdb *.exp *.[Rr][Ee][Ss] *.dll
