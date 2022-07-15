typedef struct dirlist {
	const char *path;
	char **list;
	size_t len;
} dirlist_t;

dirlist_t dirlist_new(const char *, const char *);
void      dirlist_delete(dirlist_t ds);

typedef struct file {
	char *buffer;
	char *path;
	int index;
	size_t size;
} file_t;

file_t file_getrand(dirlist_t ds, int idx);

enum key {
	KEY_PICTURE,
	KEY__MAX,
};
static const struct kvalid keys[KEY__MAX] = {
	{ kvalid_int, "p" },
};

enum page {
	PAGE_INDEX,
	PAGE_INFO,
	PAGE_COUNT,
	PAGE__MAX,
};
static const char *const pages[PAGE__MAX] = {
	"index", "info", "count"
};

enum kcgi_err dispatch(struct kreq *, config_t *);
