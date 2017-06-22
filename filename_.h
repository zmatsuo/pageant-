#ifndef _FILENAME__H_
#define _FILENAME__H_

#ifdef __cplusplus
extern "C" {
#endif

struct Filename {
#if defined(_MSC_VER)
    wchar_t *path;
#else
    char *path;
#endif
};

typedef struct Filename Filename;

#if defined(_MSC_VER)
#define f_open(filename, mode, isprivate) ( _wfopen((filename)->path, (L ## mode) ) )
#else
#define f_open(filename, mode, isprivate) ( fopen((filename)->path, (mode)) )
#endif

Filename *filename_from_str(const char *string);
const char *filename_to_str(const Filename *fn);
int filename_equal(const Filename *f1, const Filename *f2);
int filename_is_null(const Filename *fn);
Filename *filename_copy(const Filename *fn);
void filename_free(Filename *fn);
int filename_serialise(const Filename *f, void *data);
Filename *filename_deserialise(void *data, int maxsize, int *used);

#ifdef __cplusplus
}
#endif

#endif // _FILENAME__H_
