include .github/local/Makefile.local

.PHONY: all
all: build

#
# Test
#
.PHONY: test
test:

#
# Build
#
.PHONY: build
build: build-doc

SRC-DOC		:=	debian
DOCS		:=	$(SRC-DOC)/SOURCE
.PHONY: build-doc
build-doc: $(DOCS)

.PHONY: $(SRC-DOC)/SOURCE
$(SRC-DOC)/SOURCE: $(SRC-DOC)
	echo -e "git clone $(shell git remote get-url origin)\ngit checkout $(shell git rev-parse HEAD)" > "$@"

#
# Documentation
#
.PHONY: serve
serve:
	mdbook serve

.PHONY: serve_zh-CN
serve_zh-CN:
	MDBOOK_BOOK__LANGUAGE=zh-CN mdbook serve -d book/zh-CN

PO_LOCALE := zh-CN
.PHONY: translate
translate:
	MDBOOK_OUTPUT='{"xgettext": {"pot-file": "messages.pot"}}' mdbook build -d po
	cd po; \
	for i in $(PO_LOCALE); \
	do \
		if [ ! -f $$i.po ]; \
		then \
			msginit -l $$i --no-translator; \
		else \
			msgmerge --update $$i.po messages.pot; \
		fi \
	done

#
# Clean
#
.PHONY: distclean
distclean: clean

.PHONY: clean
clean: clean-doc clean-deb

.PHONY: clean-doc
clean-doc:
	rm -rf $(DOCS)

.PHONY: clean-deb
clean-deb:
	rm -rf debian/.debhelper debian/${PROJECT}*/ debian/debhelper-build-stamp debian/files debian/*.debhelper.log debian/*.postrm.debhelper debian/*.substvars

#
# Release
#
.PHONY: dch
dch: debian/changelog
	EDITOR=true gbp dch --ignore-branch --multimaint-merge --commit --release --dch-opt=--upstream

.PHONY: deb
deb: debian
	dos2unix src/edk2/BaseTools/Conf/tools_def.template
	debuild --no-lintian --lintian-hook "lintian --fail-on error,warning --suppress-tags bad-distribution-in-changes-file -- %p_%v_*.changes" --no-sign -b
	unix2dos src/edk2/BaseTools/Conf/tools_def.template

.PHONY: release
release:
	gh workflow run .github/workflows/new_version.yml --ref $(shell git branch --show-current)
