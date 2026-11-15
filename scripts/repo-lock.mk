# Shared by the local CI and delegated source Makefiles.
override WITH_REPO_LOCK := "$(REPO_ROOT)/scripts/with_repo_lock.sh" --workspace
override LOCKED_MAKE = $(WITH_REPO_LOCK) $(MAKE) --no-print-directory
override define ENSURE_REPO_LOCK
if ! $(WITH_REPO_LOCK) --held; then \
	$(LOCKED_MAKE) "$@"; \
	exit $$?; \
fi
endef
