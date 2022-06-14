
// Definitions for baked-in command line argument config.

#define BAKED_IN_MAGIC1    "_-_vUtILS_"
#define BAKED_IN_MAGIC2    "-_aRgS_-_"
#define BAKED_IN_MAX_LEN   1024

#define HAVE_BAKED_IN_CONFIG(cfg) ((cfg).config[0] != '\0')

struct baked_in_config {
	char magic[20];
	char config[BAKED_IN_MAX_LEN];
};

