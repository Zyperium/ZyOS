#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define _MM_MALLOC_H_INCLUDED
#include <klibkrnl.h>
#include <kalloc.h>

#define STBI_MALLOC(sz)        malloc(sz)
#define STBI_REALLOC(p, newsz) realloc(p, newsz)
#define STBI_FREE(p)           free(p)
#define STBI_ABS(x)            abs(x)
#define STBI_ASSERT(x)         ((void)0)

#define STB_IMAGE_IMPLEMENTATION
#include <winlib/stb_img.h>