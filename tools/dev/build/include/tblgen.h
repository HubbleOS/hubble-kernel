#pragma once

typedef enum
{
	TBL_SYSCALL = 1,
	// TBL_ERRORS,
} TblGenType;

int generate_tbl2header(const char *src, const char *out, TblGenType type);
