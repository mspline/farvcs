%.res : %.rc
	rc $<

%.obj : %.cpp
	cl -Zi -c -MT -W4 -EHsc -D_CRT_SECURE_NO_DEPRECATE $<

farcvs.dll : farcvs.obj miscutil.obj plugutil.obj cvsentries.obj regwrap.obj farcvs.res farcvs.def farcvs_en.lng
	link -debug -dll -incremental:no -def:farcvs.def farcvs.obj miscutil.obj plugutil.obj cvsentries.obj regwrap.obj farcvs.res advapi32.lib shell32.lib

install : farcvs.dll
	mkdir -p "${PROGRAMFILES}/Far/Plugins/FarCVS"
	pskill far.exe
	sleep 4
	cp -p $< "${PROGRAMFILES}/Far/Plugins/FarCVS"
	"${PROGRAMFILES}/Far/Far.exe"

clean :
	rm -f *.obj *.map *.lib *.pdb *.exp *.[Rr][Ee][Ss] *.dll
