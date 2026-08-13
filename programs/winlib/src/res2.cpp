#include <klibkrnl.h>
#include <kalloc.h>

#define STBIR_NO_STDIO
#define STBIR_MALLOC(sz, c)    ((void)(c), malloc(sz))
#define STBIR_FREE(p, c)       ((void)(c), free(p))
#define STBIR_ASSERT(x)        ((void)0)
#define _MM_MALLOC_H_INCLUDED
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <winlib/stb_image_resize2.h>
