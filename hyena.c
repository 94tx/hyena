/*
 * Copyright (c) 2021 wolf <wolf@fastmail.co.uk>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <dirent.h>
#if HAVE_ERR
#include <err.h>
#endif
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#if HAVE_CAPSICUM
#include <sys/resource.h>
#include <sys/capsicum.h>
#endif

#include <kcgi.h>
#include <kcgijson.h>
#include <libconfig.h>

#include "hyena.h"

dirlist_t
dirlist_new(const char *path, const char *prefix)
{
	char **list = NULL;
	size_t len = 0, plen;
	char *npath = NULL;
	DIR *dp;
	struct dirent *ep;
	dirlist_t ds;

	if (prefix == NULL)
		prefix = path;
	plen = strlen(prefix);
	dp = opendir(path);
	if (dp == NULL)
		err(EXIT_FAILURE, "%s", path);
	while ((ep = readdir(dp)) != NULL) {
		if (ep->d_name[0] == '.' || ep->d_type == DT_DIR)
			continue;
		list = (char **)kreallocarray(list, len + 1, sizeof(char **));
		npath = strdup(prefix);
		if (prefix[plen - 1] != '/') {
			npath = kreallocarray(npath, plen + strlen(ep->d_name) + 2, sizeof(char));
			strcat(npath, "/");
		} else {
			npath = kreallocarray(npath, plen + strlen(ep->d_name) + 1, sizeof(char));
		}
		strcat(npath, ep->d_name);
		len++;
		list[len - 1] = kstrdup(npath);
	}
	free(npath);
	closedir(dp);
	ds = (dirlist_t) {
		.list = list,
			.len = len,
	};
	return ds;
}

void
dirlist_delete(dirlist_t ds)
{
	for (size_t i = 0; i < ds.len; i++)
		free(ds.list[i]);
	free(ds.list);
}

file_t
file_getrand(dirlist_t ds, int idx)
{
	char *buf, *path;
	size_t sz;
	FILE *f;
	file_t sf;

	if (ds.len == 0)
		errx(EXIT_FAILURE, "file_getrand: no files in `%s'", ds.path);
	if (idx == -1)
#if HAVE_ARC4RANDOM
		idx = arc4random_uniform(ds.len);
#else
		idx = rand() % ds.len;
#endif
	idx = idx % ds.len;
	path = ds.list[idx];

	if ((f = fopen(path, "r")) == NULL)	/* This should not happen */
		err(EXIT_FAILURE, NULL);
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	rewind(f);
	buf = (char *)kcalloc(sz + 1, sizeof(char));
	fread(buf, 1, sz, f);
	fclose(f);

	sf = (file_t) {
		.buffer = buf,
			.path = path,
			.index = idx,
			.size = sz
	};
	return sf;
}

enum kcgi_err
dispatch(struct kreq *req, config_t * cfg)
{
	const char *path = NULL, *default_vhost;
	int use_default = 0, idx = -1;
	config_setting_t *vhosts;

	vhosts = config_lookup(cfg, "vhosts");
	if (vhosts == NULL)
		return KCGI_FORM;
	int res = config_lookup_string(cfg, "default-vhost", &default_vhost);
	if (!res)
		return KCGI_FORM;

lookup_vhost:
	if (req->host != NULL && req->host[0] != 0) {
		for (int i = 0; i < config_setting_length(vhosts); i++) {
			config_setting_t *vhost_setting = config_setting_get_elem(vhosts, i);
			const char *host;
			int res = config_setting_lookup_string(vhost_setting, "host", &host);
			if (!res)
				return KCGI_FORM;
			res = config_setting_lookup_string(vhost_setting, "path", &path);
			if (!res)
				return KCGI_FORM;
			if (strcmp(host, (use_default ? default_vhost : req->host)) == 0)
				break;
		}
	}
	if (path == NULL && use_default == 0) {
		use_default = 1;
		goto lookup_vhost;
	} else if (path == NULL && use_default == 1) {
		return KCGI_FORM;
	}

#if HAVE_UNVEIL
	if (unveil(path, "r") != 0)
		return KCGI_SYSTEM;
#endif

	switch (req->page) {
	case PAGE_INDEX:
		if (req->fieldmap[KEY_PICTURE] != NULL)
			idx = req->fieldmap[KEY_PICTURE]->parsed.i;
		dirlist_t ds = dirlist_new(path, NULL);
		file_t f = file_getrand(ds, idx);
		idx = f.index;
		/* guess MIME type */
		const char *ext = strrchr(f.path, '.') + 1, *mime;
		config_setting_t *mime_types = config_lookup(cfg, "mime-types");
		int res = config_setting_lookup_string(mime_types, ext, &mime);
		if (!res) {
			res = config_setting_lookup_string(mime_types, "*default*", &mime);
			if (!res) {
				mime = "application/octet-stream";
			}
		}
		khttp_head(req, kresps[KRESP_STATUS], "%s", khttps[KHTTP_200]);
		khttp_head(req, kresps[KRESP_CONTENT_TYPE], "%s", mime);
		khttp_head(req, kresps[KRESP_CONTENT_DISPOSITION], "inline; filename=\"%d.%s\"", idx, ext);
		khttp_head(req, "X-Hyena-Index", "%d", idx);
		khttp_body(req);
		khttp_write(req, f.buffer, f.size);
		free(f.buffer);
		dirlist_delete(ds);
		break;
	case PAGE_INFO:
		khttp_head(req, kresps[KRESP_STATUS], "%s", khttps[KHTTP_200]);
		khttp_head(req, kresps[KRESP_CONTENT_TYPE], "%s", kmimetypes[KMIME_APP_JSON]);
		khttp_body(req);

		struct kjsonreq json;
		kjson_open(&json, req);
		kjson_obj_open(&json);
		kjson_arrayp_open(&json, "vhosts");
		for (int i = 0; i < config_setting_length(vhosts); i++) {
			config_setting_t *vhost_setting = config_setting_get_elem(vhosts, i);
			const char *host, *path;
			dirlist_t ds;
			int res = config_setting_lookup_string(vhost_setting, "host", &host);
			if (!res)
				return KCGI_FORM;
			res = config_setting_lookup_string(vhost_setting, "path", &path);
			if (!res)
				return KCGI_FORM;
#if HAVE_UNVEIL
			if (unveil(path, "r") != 0)
				return KCGI_SYSTEM;
#endif
			ds = dirlist_new(path, NULL);
			kjson_obj_open(&json);
			kjson_putstringp(&json, "host", host);
			kjson_putstringp(&json, "path", path);
			kjson_putintp(&json, "count", ds.len);
			kjson_obj_close(&json);
			dirlist_delete(ds);
		}
		kjson_close(&json);
		break;
	case PAGE_COUNT:
		khttp_head(req, kresps[KRESP_STATUS], "%s", khttps[KHTTP_200]);
		khttp_head(req, kresps[KRESP_CONTENT_TYPE], "%s", kmimetypes[KMIME_TEXT_PLAIN]);
		khttp_body(req);
		ds = dirlist_new(path, NULL);
		khttp_printf(req, "%zu", ds.len);
		dirlist_delete(ds);
		break;
	}

	return KCGI_OK;
}

int
main()
{
	config_t cfg;
	const char *cfg_path = PREFIX "/etc/hyena/hyena.conf";

#if !HAVE_ARC4RANDOM
	srand(time(NULL));
#endif

	config_init(&cfg);
	if (!config_read_file(&cfg, cfg_path)) {
		config_destroy(&cfg);
		err(EXIT_FAILURE, "config_read_file: %s", cfg_path);
	}

	if (khttp_fcgi_test()) {
		/* running as FastCGI daemon */
#if HAVE_PLEDGE
		if (pledge("stdio rpath unveil unix sendfd recvfd proc", NULL) != 0)
			err(EXIT_FAILURE, NULL);
#endif
		struct kfcgi *fcgi;
		enum kcgi_err error = khttp_fcgi_init(&fcgi, keys, KEY__MAX, pages, PAGE__MAX, PAGE_INDEX);
#if HAVE_PLEDGE
		if (pledge("stdio rpath unveil", NULL) != 0)
			err(EXIT_FAILURE, NULL);
#endif
		if (error)
			errx(EXIT_FAILURE, "khttp_fcgi_init: %s", kcgi_strerror(error));
		for (struct kreq r; (error = khttp_fcgi_parse(fcgi, &r)) == KCGI_OK; khttp_free(&r)) {
			error = dispatch(&r, &cfg);
			if (error && error != KCGI_HUP)
				break;
		}
		if (error != KCGI_EXIT)
			errx(EXIT_FAILURE, "khttp_fcgi_parse: %s", kcgi_strerror(error));
		khttp_fcgi_free(fcgi);
	} else {
		/* running as normal CGI program */
		struct kreq r;
#if HAVE_PLEDGE
		if (pledge("stdio rpath proc unveil", NULL) != 0)
			err(EXIT_FAILURE, NULL);
#endif
		enum kcgi_err error = khttp_parse(&r, keys, KEY__MAX, pages, PAGE__MAX, PAGE_INDEX);
#if HAVE_PLEDGE
		if (pledge("stdio rpath unveil", NULL) != 0)
			err(EXIT_FAILURE, NULL);
#endif
		if (error)
			errx(EXIT_FAILURE, "khttp_parse: %s", kcgi_strerror(error));
		error = dispatch(&r, &cfg);
		if (error)
			errx(EXIT_FAILURE, "%s", kcgi_strerror(error));
		khttp_free(&r);
	}

	return 0;
}
