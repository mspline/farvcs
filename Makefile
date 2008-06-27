SVN_DIR  ?= ../svn/tags/1.5.0
APR_DIR  ?= ../apr
APU_DIR  ?= ../apr-util
ZLIB_DIR ?= ../zlib
NEON_DIR ?= ../neon/0.28.2

OBJFILES = farvcs.obj miscutil.obj plugutil.obj vcs.obj regwrap.obj
RESFILES = farvcs.res
DEFFILE  = farvcs.def

LIBS += advapi32.lib shell32.lib

INCLUDES_SVN = $(SVN_DIR)/subversion/include $(APR_DIR)/include $(APU_DIR)/include $(APU_DIR)/xml/expat/lib $(ZLIB_DIR) $(NEON_DIR)/src

%.res : %.rc
	rc $<

%.obj : %.cpp
	cl -c -MT -W4 -Ox -EHsc -D_CRT_SECURE_NO_DEPRECATE -DWIN32 -DSVN_NEON_0_25 -DAPR_DECLARE_STATIC -DAPU_DECLARE_STATIC -DAPI_DECLARE_STATIC $(addprefix -I,$(INCLUDES_SVN)) $<

%.obj : %.c
	cl -c -MT -W4 -Ox -D_CRT_SECURE_NO_DEPRECATE -DWIN32 -DSVN_NEON_0_25 -DAPR_DECLARE_STATIC -DAPU_DECLARE_STATIC -DAPI_DECLARE_STATIC -DHAVE_EXPAT -DHAVE_EXPAT_H -DNE_HAVE_DAV $(addprefix -I,$(INCLUDES_SVN)) -Fo$@ $<

farvcs.dll : farvcs_cvs.vcs farvcs_svn.vcs $(OBJFILES) $(RESFILES) $(DEFFILE) farvcs_en.lng
	link -out:$@ -dll -incremental:no -def:$(DEFFILE) $(OBJFILES) $(RESFILES) $(LIBS)

install : farvcs.dll farvcs_cvs.vcs farvcs_svn.vcs
	mkdir -p "$(PROGRAMFILES)/Far/Plugins/FarVCS"
	pskill far.exe
	sleep 4
	cp -p farvcs.dll "$(PROGRAMFILES)/Far/Plugins/FarVCS"
	cp -p farvcs_cvs.vcs "$(PROGRAMFILES)/Far/Plugins/FarVCS"
	cp -p farvcs_svn.vcs "$(PROGRAMFILES)/Far/Plugins/FarVCS"
	"$(PROGRAMFILES)/Far/Far.exe"

clean :
	rm -f *.obj *.map *.lib *.pdb *.exp *.[Rr][Ee][Ss] *.dll *.vcs *.manifest *.user

LIBS_CVS = advapi32.lib
OBJFILES_CVS = farvcs_cvs.obj miscutil.obj plugutil.obj regwrap.obj

farvcs_cvs.vcs : $(OBJFILES_CVS)
	link -out:$@ -dll -incremental:no $(OBJFILES_CVS) $(LIBS_CVS)

LIBS_SVN += advapi32.lib shell32.lib kernel32.lib ws2_32.lib mswsock.lib rpcrt4.lib ole32.lib

# LIBS_SVN += ${SVN_DIR}/lib/intl3_svn.lib
# LIBS_SVN += ${SVN_DIR}/lib/libdb44.lib

OBJFILES_SVN = farvcs_svn.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_client-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_client/checkout.obj \
                $(SVN_DIR)/subversion/libsvn_client/commit_util.obj \
                $(SVN_DIR)/subversion/libsvn_client/ctx.obj \
                $(SVN_DIR)/subversion/libsvn_client/export.obj \
                $(SVN_DIR)/subversion/libsvn_client/externals.obj \
                $(SVN_DIR)/subversion/libsvn_client/log.obj \
                $(SVN_DIR)/subversion/libsvn_client/mergeinfo.obj \
                $(SVN_DIR)/subversion/libsvn_client/prop_commands.obj \
                $(SVN_DIR)/subversion/libsvn_client/ra.obj \
                $(SVN_DIR)/subversion/libsvn_client/relocate.obj \
                $(SVN_DIR)/subversion/libsvn_client/revisions.obj \
                $(SVN_DIR)/subversion/libsvn_client/status.obj \
                $(SVN_DIR)/subversion/libsvn_client/switch.obj \
                $(SVN_DIR)/subversion/libsvn_client/update.obj \
                $(SVN_DIR)/subversion/libsvn_client/url.obj \
                $(SVN_DIR)/subversion/libsvn_client/util.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_delta-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_delta/cancel.obj \
                $(SVN_DIR)/subversion/libsvn_delta/compat.obj \
                $(SVN_DIR)/subversion/libsvn_delta/compose_delta.obj \
                $(SVN_DIR)/subversion/libsvn_delta/default_editor.obj \
                $(SVN_DIR)/subversion/libsvn_delta/depth_filter_editor.obj \
                $(SVN_DIR)/subversion/libsvn_delta/path_driver.obj \
                $(SVN_DIR)/subversion/libsvn_delta/svndiff.obj \
                $(SVN_DIR)/subversion/libsvn_delta/text_delta.obj \
                $(SVN_DIR)/subversion/libsvn_delta/vdelta.obj \
                $(SVN_DIR)/subversion/libsvn_delta/version.obj \
                $(SVN_DIR)/subversion/libsvn_delta/xdelta.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_diff-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_diff/diff.obj \
                $(SVN_DIR)/subversion/libsvn_diff/diff3.obj \
                $(SVN_DIR)/subversion/libsvn_diff/diff4.obj \
                $(SVN_DIR)/subversion/libsvn_diff/diff_memory.obj \
                $(SVN_DIR)/subversion/libsvn_diff/diff_file.obj \
                $(SVN_DIR)/subversion/libsvn_diff/lcs.obj \
                $(SVN_DIR)/subversion/libsvn_diff/token.obj \
                $(SVN_DIR)/subversion/libsvn_diff/util.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_fs-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_fs/access.obj \
                $(SVN_DIR)/subversion/libsvn_fs/fs-loader.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_fs_fs-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_fs_fs/dag.obj \
                $(SVN_DIR)/subversion/libsvn_fs_fs/err.obj \
                $(SVN_DIR)/subversion/libsvn_fs_fs/fs.obj \
                $(SVN_DIR)/subversion/libsvn_fs_fs/fs_fs.obj \
                $(SVN_DIR)/subversion/libsvn_fs_fs/id.obj \
                $(SVN_DIR)/subversion/libsvn_fs_fs/key-gen.obj \
                $(SVN_DIR)/subversion/libsvn_fs_fs/lock.obj \
                $(SVN_DIR)/subversion/libsvn_fs_fs/tree.obj

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_fs_util/fs-util.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_ra-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_ra/compat.obj \
                $(SVN_DIR)/subversion/libsvn_ra/ra_loader.obj \
                $(SVN_DIR)/subversion/libsvn_ra/util.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_ra_local-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_ra_local/ra_plugin.obj \
                $(SVN_DIR)/subversion/libsvn_ra_local/split_url.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_ra_svn-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_ra_svn/client.obj \
                $(SVN_DIR)/subversion/libsvn_ra_svn/cram.obj \
                $(SVN_DIR)/subversion/libsvn_ra_svn/editorp.obj \
                $(SVN_DIR)/subversion/libsvn_ra_svn/internal_auth.obj \
                $(SVN_DIR)/subversion/libsvn_ra_svn/marshal.obj \
                $(SVN_DIR)/subversion/libsvn_ra_svn/streams.obj \
                $(SVN_DIR)/subversion/libsvn_ra_svn/version.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_repos-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_repos/commit.obj \
                $(SVN_DIR)/subversion/libsvn_repos/delta.obj \
                $(SVN_DIR)/subversion/libsvn_repos/hooks.obj \
                $(SVN_DIR)/subversion/libsvn_repos/fs-wrap.obj \
                $(SVN_DIR)/subversion/libsvn_repos/log.obj \
                $(SVN_DIR)/subversion/libsvn_repos/replay.obj \
                $(SVN_DIR)/subversion/libsvn_repos/reporter.obj \
                $(SVN_DIR)/subversion/libsvn_repos/repos.obj \
                $(SVN_DIR)/subversion/libsvn_repos/rev_hunt.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_subr-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_subr/auth.obj \
                $(SVN_DIR)/subversion/libsvn_subr/cmdline.obj \
                $(SVN_DIR)/subversion/libsvn_subr/compat.obj \
                $(SVN_DIR)/subversion/libsvn_subr/config.obj \
                $(SVN_DIR)/subversion/libsvn_subr/config_auth.obj \
                $(SVN_DIR)/subversion/libsvn_subr/config_file.obj \
                $(SVN_DIR)/subversion/libsvn_subr/config_win.obj \
                $(SVN_DIR)/subversion/libsvn_subr/constructors.obj \
                $(SVN_DIR)/subversion/libsvn_subr/ctype.obj \
                $(SVN_DIR)/subversion/libsvn_subr/date.obj \
                $(SVN_DIR)/subversion/libsvn_subr/dso.obj \
                $(SVN_DIR)/subversion/libsvn_subr/error.obj \
                $(SVN_DIR)/subversion/libsvn_subr/hash.obj \
                $(SVN_DIR)/subversion/libsvn_subr/io.obj \
                $(SVN_DIR)/subversion/libsvn_subr/iter.obj \
                $(SVN_DIR)/subversion/libsvn_subr/kitchensink.obj \
                $(SVN_DIR)/subversion/libsvn_subr/lock.obj \
                $(SVN_DIR)/subversion/libsvn_subr/md5.obj \
                $(SVN_DIR)/subversion/libsvn_subr/mergeinfo.obj \
                $(SVN_DIR)/subversion/libsvn_subr/nls.obj \
                $(SVN_DIR)/subversion/libsvn_subr/opt.obj \
                $(SVN_DIR)/subversion/libsvn_subr/path.obj \
                $(SVN_DIR)/subversion/libsvn_subr/pool.obj \
                $(SVN_DIR)/subversion/libsvn_subr/prompt.obj \
                $(SVN_DIR)/subversion/libsvn_subr/properties.obj \
                $(SVN_DIR)/subversion/libsvn_subr/sorts.obj \
                $(SVN_DIR)/subversion/libsvn_subr/simple_providers.obj \
                $(SVN_DIR)/subversion/libsvn_subr/ssl_client_cert_providers.obj \
                $(SVN_DIR)/subversion/libsvn_subr/ssl_client_cert_pw_providers.obj \
                $(SVN_DIR)/subversion/libsvn_subr/ssl_server_trust_providers.obj \
                $(SVN_DIR)/subversion/libsvn_subr/stream.obj \
                $(SVN_DIR)/subversion/libsvn_subr/subst.obj \
                $(SVN_DIR)/subversion/libsvn_subr/svn_base64.obj \
                $(SVN_DIR)/subversion/libsvn_subr/svn_string.obj \
                $(SVN_DIR)/subversion/libsvn_subr/target.obj \
                $(SVN_DIR)/subversion/libsvn_subr/time.obj \
                $(SVN_DIR)/subversion/libsvn_subr/username_providers.obj \
                $(SVN_DIR)/subversion/libsvn_subr/utf.obj \
                $(SVN_DIR)/subversion/libsvn_subr/utf_validate.obj \
                $(SVN_DIR)/subversion/libsvn_subr/user.obj \
                $(SVN_DIR)/subversion/libsvn_subr/validate.obj \
                $(SVN_DIR)/subversion/libsvn_subr/version.obj \
                $(SVN_DIR)/subversion/libsvn_subr/win32_xlate.obj \
                $(SVN_DIR)/subversion/libsvn_subr/xml.obj

# LIBS_SVN += ${SVN_DIR}/lib/libsvn_wc-1.lib

OBJFILES_SVN += $(SVN_DIR)/subversion/libsvn_wc/adm_crawler.obj \
                $(SVN_DIR)/subversion/libsvn_wc/adm_files.obj \
                $(SVN_DIR)/subversion/libsvn_wc/adm_ops.obj \
                $(SVN_DIR)/subversion/libsvn_wc/ambient_depth_filter_editor.obj \
                $(SVN_DIR)/subversion/libsvn_wc/entries.obj \
                $(SVN_DIR)/subversion/libsvn_wc/lock.obj \
                $(SVN_DIR)/subversion/libsvn_wc/log.obj \
                $(SVN_DIR)/subversion/libsvn_wc/merge.obj \
                $(SVN_DIR)/subversion/libsvn_wc/props.obj \
                $(SVN_DIR)/subversion/libsvn_wc/relocate.obj \
                $(SVN_DIR)/subversion/libsvn_wc/status.obj \
                $(SVN_DIR)/subversion/libsvn_wc/translate.obj \
                $(SVN_DIR)/subversion/libsvn_wc/questions.obj \
                $(SVN_DIR)/subversion/libsvn_wc/update_editor.obj \
                $(SVN_DIR)/subversion/libsvn_wc/util.obj

# LIBS_SVN += ${SVN_DIR}/lib/apr/libapr-1.lib

LIBS_SVN += apr-1.lib

# LIBS_SVN += ${SVN_DIR}/lib/apr-util/libaprutil-1.lib

LIBS_SVN += aprutil-1.lib
LIBS_SVN += apriconv-1.lib

# LIBS_SVN += ${SVN_DIR}/lib/apr-util/xml.lib

OBJFILES_SVN += $(APU_DIR)/xml/expat/lib/xmlparse.obj \
                $(APU_DIR)/xml/expat/lib/xmlrole.obj \
                $(APU_DIR)/xml/expat/lib/xmltok.obj

# LIBS_SVN += ${SVN_DIR}/lib/neon/libneon.lib

OBJFILES_SVN += $(NEON_DIR)/src/ne_207.obj \
                $(NEON_DIR)/src/ne_alloc.obj \
                $(NEON_DIR)/src/ne_auth.obj \
                $(NEON_DIR)/src/ne_basic.obj \
                $(NEON_DIR)/src/ne_compress.obj \
                $(NEON_DIR)/src/ne_dates.obj \
                $(NEON_DIR)/src/ne_locks.obj \
                $(NEON_DIR)/src/ne_md5.obj \
                $(NEON_DIR)/src/ne_props.obj \
                $(NEON_DIR)/src/ne_request.obj \
                $(NEON_DIR)/src/ne_session.obj \
                $(NEON_DIR)/src/ne_socket.obj \
                $(NEON_DIR)/src/ne_sspi.obj \
                $(NEON_DIR)/src/ne_string.obj \
                $(NEON_DIR)/src/ne_stubssl.obj \
                $(NEON_DIR)/src/ne_uri.obj \
                $(NEON_DIR)/src/ne_utils.obj \
                $(NEON_DIR)/src/ne_xml.obj \
                $(NEON_DIR)/src/ne_xmlreq.obj

OBJFILES_SVN += $(ZLIB_DIR)/adler32.obj \
                $(ZLIB_DIR)/compress.obj \
                $(ZLIB_DIR)/crc32.obj \
                $(ZLIB_DIR)/deflate.obj \
                $(ZLIB_DIR)/inffast.obj \
                $(ZLIB_DIR)/inflate.obj \
                $(ZLIB_DIR)/inftrees.obj \
                $(ZLIB_DIR)/trees.obj \
                $(ZLIB_DIR)/uncompr.obj \
                $(ZLIB_DIR)/zutil.obj

farvcs_svn.vcs : $(OBJFILES_SVN)
	link -out:$@ -dll -incremental:no $(OBJFILES_SVN) $(LIBS_SVN) | grep -v LNK4099
