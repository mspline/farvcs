SVN_DIR  ?= ../svn
APR_DIR  ?= ../apr
APU_DIR  ?= ../apr-util
ZLIB_DIR ?= ../zlib
NEON_DIR ?= ../neon

OBJFILES = farvcs.obj miscutil.obj plugutil.obj vcs.obj regwrap.obj
RESFILES = farvcs.res
DEFFILE  = farvcs.def

LIBS += advapi32.lib shell32.lib

INCLUDES_SVN = ${SVN_DIR}/subversion/include ${APR_DIR}/include ${APU_DIR}/include ${APU_DIR}/xml/expat/lib ${ZLIB_DIR} ${NEON_DIR}/0.28.2/src

%.res : %.rc
	rc $<

%.obj : %.cpp
	cl -c ${CXXRT} -W4 -EHsc -D_CRT_SECURE_NO_DEPRECATE -DWIN32 -DSVN_NEON_0_25 -DAPR_DECLARE_STATIC -DAPU_DECLARE_STATIC -DAPI_DECLARE_STATIC $(addprefix -I,${INCLUDES_SVN}) $<

%.obj : %.c
	cl -c ${CXXRT} -W4 -D_CRT_SECURE_NO_DEPRECATE -DWIN32 -DSVN_NEON_0_25 -DAPR_DECLARE_STATIC -DAPU_DECLARE_STATIC -DAPI_DECLARE_STATIC -DHAVE_EXPAT -DHAVE_EXPAT_H -DNE_HAVE_DAV $(addprefix -I,${INCLUDES_SVN}) -Fo$@ $<

                CXXRT = -MT
#farvcs_svn.obj: CXXRT = -MD

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

LIBS_SVN += advapi32.lib shell32.lib kernel32.lib ws2_32.lib mswsock.lib rpcrt4.lib ole32.lib

# LIBS_SVN += ${SVN_DIR}/lib/intl3_svn.lib
# LIBS_SVN += ${SVN_DIR}/lib/libdb44.lib

OBJFILES_SVN = farvcs_svn.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_client-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_client/checkout.obj \
                d:/work/svn/subversion/libsvn_client/commit_util.obj \
                d:/work/svn/subversion/libsvn_client/ctx.obj \
                d:/work/svn/subversion/libsvn_client/export.obj \
                d:/work/svn/subversion/libsvn_client/externals.obj \
                d:/work/svn/subversion/libsvn_client/log.obj \
                d:/work/svn/subversion/libsvn_client/mergeinfo.obj \
                d:/work/svn/subversion/libsvn_client/prop_commands.obj \
                d:/work/svn/subversion/libsvn_client/ra.obj \
                d:/work/svn/subversion/libsvn_client/relocate.obj \
                d:/work/svn/subversion/libsvn_client/revisions.obj \
                d:/work/svn/subversion/libsvn_client/status.obj \
                d:/work/svn/subversion/libsvn_client/switch.obj \
                d:/work/svn/subversion/libsvn_client/update.obj \
                d:/work/svn/subversion/libsvn_client/url.obj \
                d:/work/svn/subversion/libsvn_client/util.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_delta-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_delta/cancel.obj \
                d:/work/svn/subversion/libsvn_delta/compat.obj \
                d:/work/svn/subversion/libsvn_delta/compose_delta.obj \
                d:/work/svn/subversion/libsvn_delta/default_editor.obj \
                d:/work/svn/subversion/libsvn_delta/depth_filter_editor.obj \
                d:/work/svn/subversion/libsvn_delta/path_driver.obj \
                d:/work/svn/subversion/libsvn_delta/svndiff.obj \
                d:/work/svn/subversion/libsvn_delta/text_delta.obj \
                d:/work/svn/subversion/libsvn_delta/vdelta.obj \
                d:/work/svn/subversion/libsvn_delta/version.obj \
                d:/work/svn/subversion/libsvn_delta/xdelta.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_diff-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_diff/diff.obj \
                d:/work/svn/subversion/libsvn_diff/diff3.obj \
                d:/work/svn/subversion/libsvn_diff/diff4.obj \
                d:/work/svn/subversion/libsvn_diff/diff_memory.obj \
                d:/work/svn/subversion/libsvn_diff/diff_file.obj \
                d:/work/svn/subversion/libsvn_diff/lcs.obj \
                d:/work/svn/subversion/libsvn_diff/token.obj \
                d:/work/svn/subversion/libsvn_diff/util.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_fs-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_fs/access.obj \
                d:/work/svn/subversion/libsvn_fs/fs-loader.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_fs_fs-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_fs_fs/dag.obj \
                d:/work/svn/subversion/libsvn_fs_fs/err.obj \
                d:/work/svn/subversion/libsvn_fs_fs/fs.obj \
                d:/work/svn/subversion/libsvn_fs_fs/fs_fs.obj \
                d:/work/svn/subversion/libsvn_fs_fs/id.obj \
                d:/work/svn/subversion/libsvn_fs_fs/key-gen.obj \
                d:/work/svn/subversion/libsvn_fs_fs/lock.obj \
                d:/work/svn/subversion/libsvn_fs_fs/tree.obj

OBJFILES_SVN += d:/work/svn/subversion/libsvn_fs_util/fs-util.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_ra-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_ra/compat.obj \
                d:/work/svn/subversion/libsvn_ra/ra_loader.obj \
                d:/work/svn/subversion/libsvn_ra/util.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_ra_local-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_ra_local/ra_plugin.obj \
                d:/work/svn/subversion/libsvn_ra_local/split_url.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_ra_svn-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_ra_svn/client.obj \
                d:/work/svn/subversion/libsvn_ra_svn/cram.obj \
                d:/work/svn/subversion/libsvn_ra_svn/editorp.obj \
                d:/work/svn/subversion/libsvn_ra_svn/internal_auth.obj \
                d:/work/svn/subversion/libsvn_ra_svn/marshal.obj \
                d:/work/svn/subversion/libsvn_ra_svn/streams.obj \
                d:/work/svn/subversion/libsvn_ra_svn/version.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_repos-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_repos/commit.obj \
                d:/work/svn/subversion/libsvn_repos/delta.obj \
                d:/work/svn/subversion/libsvn_repos/hooks.obj \
                d:/work/svn/subversion/libsvn_repos/fs-wrap.obj \
                d:/work/svn/subversion/libsvn_repos/log.obj \
                d:/work/svn/subversion/libsvn_repos/replay.obj \
                d:/work/svn/subversion/libsvn_repos/reporter.obj \
                d:/work/svn/subversion/libsvn_repos/repos.obj \
                d:/work/svn/subversion/libsvn_repos/rev_hunt.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_subr-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_subr/auth.obj \
                d:/work/svn/subversion/libsvn_subr/cmdline.obj \
                d:/work/svn/subversion/libsvn_subr/compat.obj \
                d:/work/svn/subversion/libsvn_subr/config.obj \
                d:/work/svn/subversion/libsvn_subr/config_auth.obj \
                d:/work/svn/subversion/libsvn_subr/config_file.obj \
                d:/work/svn/subversion/libsvn_subr/config_win.obj \
                d:/work/svn/subversion/libsvn_subr/constructors.obj \
                d:/work/svn/subversion/libsvn_subr/ctype.obj \
                d:/work/svn/subversion/libsvn_subr/date.obj \
                d:/work/svn/subversion/libsvn_subr/dso.obj \
                d:/work/svn/subversion/libsvn_subr/error.obj \
                d:/work/svn/subversion/libsvn_subr/hash.obj \
                d:/work/svn/subversion/libsvn_subr/io.obj \
                d:/work/svn/subversion/libsvn_subr/iter.obj \
                d:/work/svn/subversion/libsvn_subr/kitchensink.obj \
                d:/work/svn/subversion/libsvn_subr/lock.obj \
                d:/work/svn/subversion/libsvn_subr/md5.obj \
                d:/work/svn/subversion/libsvn_subr/mergeinfo.obj \
                d:/work/svn/subversion/libsvn_subr/nls.obj \
                d:/work/svn/subversion/libsvn_subr/opt.obj \
                d:/work/svn/subversion/libsvn_subr/path.obj \
                d:/work/svn/subversion/libsvn_subr/pool.obj \
                d:/work/svn/subversion/libsvn_subr/prompt.obj \
                d:/work/svn/subversion/libsvn_subr/properties.obj \
                d:/work/svn/subversion/libsvn_subr/sorts.obj \
                d:/work/svn/subversion/libsvn_subr/simple_providers.obj \
                d:/work/svn/subversion/libsvn_subr/ssl_client_cert_providers.obj \
                d:/work/svn/subversion/libsvn_subr/ssl_client_cert_pw_providers.obj \
                d:/work/svn/subversion/libsvn_subr/ssl_server_trust_providers.obj \
                d:/work/svn/subversion/libsvn_subr/stream.obj \
                d:/work/svn/subversion/libsvn_subr/subst.obj \
                d:/work/svn/subversion/libsvn_subr/svn_base64.obj \
                d:/work/svn/subversion/libsvn_subr/svn_string.obj \
                d:/work/svn/subversion/libsvn_subr/target.obj \
                d:/work/svn/subversion/libsvn_subr/time.obj \
                d:/work/svn/subversion/libsvn_subr/username_providers.obj \
                d:/work/svn/subversion/libsvn_subr/utf.obj \
                d:/work/svn/subversion/libsvn_subr/utf_validate.obj \
                d:/work/svn/subversion/libsvn_subr/user.obj \
                d:/work/svn/subversion/libsvn_subr/validate.obj \
                d:/work/svn/subversion/libsvn_subr/version.obj \
                d:/work/svn/subversion/libsvn_subr/win32_xlate.obj \
                d:/work/svn/subversion/libsvn_subr/xml.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_wc-1.lib

OBJFILES_SVN += d:/work/svn/subversion/libsvn_wc/adm_crawler.obj \
                d:/work/svn/subversion/libsvn_wc/adm_files.obj \
                d:/work/svn/subversion/libsvn_wc/adm_ops.obj \
                d:/work/svn/subversion/libsvn_wc/ambient_depth_filter_editor.obj \
                d:/work/svn/subversion/libsvn_wc/entries.obj \
                d:/work/svn/subversion/libsvn_wc/lock.obj \
                d:/work/svn/subversion/libsvn_wc/log.obj \
                d:/work/svn/subversion/libsvn_wc/merge.obj \
                d:/work/svn/subversion/libsvn_wc/props.obj \
                d:/work/svn/subversion/libsvn_wc/relocate.obj \
                d:/work/svn/subversion/libsvn_wc/status.obj \
                d:/work/svn/subversion/libsvn_wc/translate.obj \
                d:/work/svn/subversion/libsvn_wc/questions.obj \
                d:/work/svn/subversion/libsvn_wc/update_editor.obj \
                d:/work/svn/subversion/libsvn_wc/util.obj

# LIBS_SVN += ${SVN_DIR}/lib/apr/libapr-1.lib

LIBS_SVN += apr-1.lib

# LIBS_SVN += ${SVN_DIR}/lib/apr-util/libaprutil-1.lib

LIBS_SVN += aprutil-1.lib
LIBS_SVN += apriconv-1.lib

# LIBS_SVN += ${SVN_DIR}/lib/apr-util/xml.lib

OBJFILES_SVN += d:/work/apr-util/xml/expat/lib/xmlparse.obj \
                d:/work/apr-util/xml/expat/lib/xmlrole.obj \
                d:/work/apr-util/xml/expat/lib/xmltok.obj

# LIBS_SVN += ${SVN_DIR}/lib/neon/libneon.lib

OBJFILES_SVN += d:/work/neon/0.28.2/src/ne_207.obj \
                d:/work/neon/0.28.2/src/ne_alloc.obj \
                d:/work/neon/0.28.2/src/ne_auth.obj \
                d:/work/neon/0.28.2/src/ne_basic.obj \
                d:/work/neon/0.28.2/src/ne_compress.obj \
                d:/work/neon/0.28.2/src/ne_dates.obj \
                d:/work/neon/0.28.2/src/ne_locks.obj \
                d:/work/neon/0.28.2/src/ne_md5.obj \
                d:/work/neon/0.28.2/src/ne_props.obj \
                d:/work/neon/0.28.2/src/ne_request.obj \
                d:/work/neon/0.28.2/src/ne_session.obj \
                d:/work/neon/0.28.2/src/ne_socket.obj \
                d:/work/neon/0.28.2/src/ne_sspi.obj \
                d:/work/neon/0.28.2/src/ne_string.obj \
                d:/work/neon/0.28.2/src/ne_stubssl.obj \
                d:/work/neon/0.28.2/src/ne_uri.obj \
                d:/work/neon/0.28.2/src/ne_utils.obj \
                d:/work/neon/0.28.2/src/ne_xml.obj \
                d:/work/neon/0.28.2/src/ne_xmlreq.obj

OBJFILES_SVN += d:/work/zlib/adler32.obj \
                d:/work/zlib/compress.obj \
                d:/work/zlib/crc32.obj \
                d:/work/zlib/deflate.obj \
                d:/work/zlib/inffast.obj \
                d:/work/zlib/inflate.obj \
                d:/work/zlib/inftrees.obj \
                d:/work/zlib/trees.obj \
                d:/work/zlib/uncompr.obj \
                d:/work/zlib/zutil.obj

farvcs_svn.vcs : ${OBJFILES_SVN}
	link -out:$@  -debug -dll -incremental:no -nodefaultlib:msvcrt ${OBJFILES_SVN} ${LIBS_SVN} | grep -v LNK4099
