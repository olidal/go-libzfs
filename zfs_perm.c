#include <libzfs.h>
#include <libzutil.h>
#include <sys/nvpair.h>
#include "common.h"
#include "zpool.h"
#include "zfs.h"
#include "zfs_perm.h"
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* ZFS_DELEG_* constants from zfs_deleg.h — redeclared here to avoid pulling
 * kernel headers. Values come from the ZoL source. */
#define	GO_DELEG_USER       'u'
#define	GO_DELEG_LOCAL      'l'
#define	GO_DELEG_DESCENDENT 'd'

/* Resolve a who string to a uid. Accepts numeric ("1000") or username.
 * Returns 0 on success and fills uid_out, non-zero on failure. */
static int
resolve_who(const char *who, uid_t *uid_out)
{
	char *end;
	long v;
	struct passwd pwd;
	struct passwd *result;
	char buf[2048];

	if (who == NULL || *who == '\0')
		return (EINVAL);

	/* Try numeric first. */
	v = strtol(who, &end, 10);
	if (*end == '\0' && v >= 0) {
		*uid_out = (uid_t)v;
		return (0);
	}

	if (getpwnam_r(who, &pwd, buf, sizeof (buf), &result) != 0)
		return (errno);
	if (result == NULL)
		return (ENOENT);
	*uid_out = pwd.pw_uid;
	return (0);
}

/* Build a perm nvlist from a comma-separated list. */
static int
build_perm_nvl(const char *perms, nvlist_t **out)
{
	nvlist_t *nvl;
	char *buf, *curr, *delim, *end;

	if (nvlist_alloc(&nvl, NV_UNIQUE_NAME, 0) != 0)
		return (ENOMEM);

	if (perms == NULL || *perms == '\0') {
		*out = nvl;
		return (0);
	}

	buf = strdup(perms);
	if (buf == NULL) {
		nvlist_free(nvl);
		return (ENOMEM);
	}

	curr = buf;
	end = buf + strlen(buf);
	while (curr < end) {
		delim = strchr(curr, ',');
		if (delim != NULL)
			*delim = '\0';
		if (*curr != '\0')
			(void) nvlist_add_boolean(nvl, curr);
		if (delim == NULL)
			break;
		curr = delim + 1;
	}

	free(buf);
	*out = nvl;
	return (0);
}

int
go_zfs_user_allow(dataset_list_ptr ds, const char *who, const char *perms,
    int locality, int unset)
{
	uid_t uid;
	int rc;
	nvlist_t *top = NULL, *perm_nvl = NULL;
	char key[64];

	if (ds == NULL || ds->zh == NULL)
		return (EINVAL);
	if ((locality & (GO_PERM_LOCAL | GO_PERM_DESCENDENT)) == 0)
		return (EINVAL);

	rc = resolve_who(who, &uid);
	if (rc != 0)
		return (rc);

	rc = build_perm_nvl(perms, &perm_nvl);
	if (rc != 0)
		return (rc);

	if (nvlist_alloc(&top, NV_UNIQUE_NAME, 0) != 0) {
		nvlist_free(perm_nvl);
		return (ENOMEM);
	}

	if (locality & GO_PERM_LOCAL) {
		(void) snprintf(key, sizeof (key), "%c%c$%u",
		    GO_DELEG_USER, GO_DELEG_LOCAL, uid);
		(void) nvlist_add_nvlist(top, key, perm_nvl);
	}
	if (locality & GO_PERM_DESCENDENT) {
		(void) snprintf(key, sizeof (key), "%c%c$%u",
		    GO_DELEG_USER, GO_DELEG_DESCENDENT, uid);
		(void) nvlist_add_nvlist(top, key, perm_nvl);
	}

	rc = zfs_set_fsacl(ds->zh, unset ? B_TRUE : B_FALSE, top);
	nvlist_free(perm_nvl);
	nvlist_free(top);
	return (rc);
}

/* Parse one top-level key "uX$<id>" and append an entry. */
static int
emit_entry(const char *key, nvlist_t *perms_nvl, go_deleg_entry_t **head)
{
	go_deleg_entry_t *e;
	nvpair_t *pp;
	char type_ch, loc_ch;
	const char *who;
	size_t off;
	struct passwd pwd, *result;
	char pwdbuf[2048];
	uid_t uid;
	char *endp;

	if (strlen(key) < 3 || key[2] != '$')
		return (0); /* unknown format, skip */

	type_ch = key[0];
	loc_ch = key[1];

	/* Only care about user entries; skip groups/everyone/sets. */
	if (type_ch != GO_DELEG_USER)
		return (0);

	who = key + 3;

	e = calloc(1, sizeof (*e));
	if (e == NULL)
		return (ENOMEM);

	/* Resolve uid back to username if possible. */
	uid = (uid_t)strtol(who, &endp, 10);
	if (*who != '\0' && *endp == '\0' &&
	    getpwuid_r(uid, &pwd, pwdbuf, sizeof (pwdbuf), &result) == 0 &&
	    result != NULL) {
		(void) snprintf(e->user, sizeof (e->user), "%s", pwd.pw_name);
	} else {
		(void) snprintf(e->user, sizeof (e->user), "%s", who);
	}

	if (loc_ch == GO_DELEG_LOCAL)
		e->locality = GO_PERM_LOCAL;
	else if (loc_ch == GO_DELEG_DESCENDENT)
		e->locality = GO_PERM_DESCENDENT;

	off = 0;
	pp = NULL;
	while ((pp = nvlist_next_nvpair(perms_nvl, pp)) != NULL) {
		const char *pname = nvpair_name(pp);
		size_t plen = strlen(pname);
		if (off + plen + 2 > sizeof (e->perms))
			break;
		if (off > 0)
			e->perms[off++] = ',';
		memcpy(e->perms + off, pname, plen);
		off += plen;
		e->perms[off] = '\0';
	}

	e->next = *head;
	*head = e;
	return (0);
}

int
go_zfs_get_allow(dataset_list_ptr ds, go_deleg_entry_t **out)
{
	nvlist_t *nvl = NULL;
	nvpair_t *np = NULL;
	go_deleg_entry_t *head = NULL;
	int rc;

	*out = NULL;
	if (ds == NULL || ds->zh == NULL)
		return (EINVAL);
	rc = zfs_get_fsacl(ds->zh, &nvl);
	if (rc != 0)
		return (rc);
	if (nvl == NULL)
		return (0);

	/* zfs_get_fsacl returns a two-level nvlist:
	 *   outer: key = dataset name, value = nvlist
	 *   inner: key = "uX$<uid>", value = nvlist of permission names
	 * Iterate both levels. */
	while ((np = nvlist_next_nvpair(nvl, np)) != NULL) {
		nvlist_t *inner;
		nvpair_t *ip = NULL;

		if (nvpair_type(np) != DATA_TYPE_NVLIST)
			continue;
		if (nvpair_value_nvlist(np, &inner) != 0)
			continue;

		while ((ip = nvlist_next_nvpair(inner, ip)) != NULL) {
			nvlist_t *perms_nvl;
			const char *key = nvpair_name(ip);
			if (nvpair_type(ip) != DATA_TYPE_NVLIST)
				continue;
			if (nvpair_value_nvlist(ip, &perms_nvl) != 0)
				continue;
			rc = emit_entry(key, perms_nvl, &head);
			if (rc != 0) {
				go_zfs_free_deleg_entries(head);
				nvlist_free(nvl);
				return (rc);
			}
		}
	}

	nvlist_free(nvl);
	*out = head;
	return (0);
}

void
go_zfs_free_deleg_entries(go_deleg_entry_t *head)
{
	while (head != NULL) {
		go_deleg_entry_t *next = head->next;
		free(head);
		head = next;
	}
}
