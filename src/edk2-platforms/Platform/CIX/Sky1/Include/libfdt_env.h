#ifndef CIX_SKY1_LIBFDT_ENV_COMPAT_H_
#define CIX_SKY1_LIBFDT_ENV_COMPAT_H_

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

typedef UINT16  fdt16_t;
typedef UINT32  fdt32_t;
typedef UINT64  fdt64_t;

typedef UINT8   uint8_t;
typedef UINT16  uint16_t;
typedef UINT32  uint32_t;
typedef UINT64  uint64_t;
typedef UINTN   uintptr_t;
typedef UINTN   size_t;

static inline uint16_t
fdt16_to_cpu (
  fdt16_t  x
  )
{
  return SwapBytes16 (x);
}

#define cpu_to_fdt16(x)  fdt16_to_cpu (x)

static inline uint32_t
fdt32_to_cpu (
  fdt32_t  x
  )
{
  return SwapBytes32 (x);
}

#define cpu_to_fdt32(x)  fdt32_to_cpu (x)

static inline uint64_t
fdt64_to_cpu (
  fdt64_t  x
  )
{
  return SwapBytes64 (x);
}

#define cpu_to_fdt64(x)  fdt64_to_cpu (x)

static inline void *
memcpy (
  void        *Dest,
  const void  *Src,
  size_t      Len
  )
{
  return CopyMem (Dest, Src, Len);
}

static inline void *
memmove (
  void        *Dest,
  const void  *Src,
  size_t      Len
  )
{
  return CopyMem (Dest, Src, Len);
}

static inline void *
memset (
  void    *Buf,
  int     Value,
  size_t  Len
  )
{
  return SetMem (Buf, Len, Value);
}

static inline int
memcmp (
  const void  *Left,
  const void  *Right,
  int         Len
  )
{
  return CompareMem (Left, Right, Len);
}

static inline void *
memchr (
  const void  *Buf,
  int         Value,
  size_t      Len
  )
{
  return ScanMem8 (Buf, Len, Value);
}

static inline size_t
strlen (
  const char  *Str
  )
{
  return AsciiStrLen (Str);
}

static inline char *
strchr (
  const char  *Str,
  int         Value
  )
{
  char  Pattern[2];

  Pattern[0] = Value;
  Pattern[1] = 0;
  return AsciiStrStr (Str, Pattern);
}

static inline size_t
strnlen (
  const char  *Str,
  size_t      StrSize
  )
{
  return AsciiStrnLenS (Str, StrSize);
}

static inline size_t
strcmp (
  const char  *Left,
  const char  *Right
  )
{
  return AsciiStrCmp (Left, Right);
}

static inline size_t
strncmp (
  const char  *Left,
  const char  *Right,
  size_t      Len
  )
{
  return AsciiStrnCmp (Left, Right, Len);
}

static inline size_t
strncpy (
  char        *Dest,
  const char  *Src,
  size_t      DestMax
  )
{
  return AsciiStrCpyS (Dest, DestMax, Src);
}

#endif
