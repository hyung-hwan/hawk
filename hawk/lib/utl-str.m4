dnl ---------------------------------------------------------------------------
changequote(`[[', `]]')dnl
dnl ---------------------------------------------------------------------------
define([[fn_comp_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_chau_type_]], $3)pushdef([[_to_lower_]], $4)dnl
int _fn_name_ (const _char_type_* str1, hawk_oow_t len1, const _char_type_* str2, hawk_oow_t len2, int ignorecase)
{
	_chau_type_ c1, c2;
	const _char_type_* end1 = str1 + len1;
	const _char_type_* end2 = str2 + len2;

	if (ignorecase)
	{
		while (str1 < end1)
		{
			c1 = _to_lower_()(*str1);
			if (str2 < end2) 
			{
				c2 = _to_lower_()(*str2);
				if (c1 > c2) return 1;
				if (c1 < c2) return -1;
			}
			else return 1;
			str1++; str2++;
		}
	}
	else
	{
		while (str1 < end1)
		{
			c1 = *str1;
			if (str2 < end2) 
			{
				c2 = *str2;
				if (c1 > c2) return 1;
				if (c1 < c2) return -1;
			}
			else return 1;
			str1++; str2++;
		}
	}

	return (str2 < end2)? -1: 0;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_chau_type_]])popdef([[_to_lower_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_comp_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_chau_type_]], $3)pushdef([[_to_lower_]], $4)dnl
int _fn_name_ (const _char_type_* str1, const _char_type_* str2, int ignorecase)
{
	if (ignorecase)
	{
		while (_to_lower_()(*str1) == _to_lower_()(*str2))
		{
			if (*str1 == '\0') return 0;
			str1++; str2++;
		}

		return ((_chau_type_)_to_lower_()(*str1) > (_chau_type_)_to_lower_()(*str2))? 1: -1;
	}
	else
	{
		while (*str1 == *str2)
		{
			if (*str1 == '\0') return 0;
			str1++; str2++;
		}

		return ((_chau_type_)*str1 > (_chau_type_)*str2)? 1: -1;
	}
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_chau_type_]])popdef([[_to_lower_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_comp_cstr_limited]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_chau_type_]], $3)pushdef([[_to_lower_]], $4)dnl
int _fn_name_ (const _char_type_* str1, const _char_type_* str2, hawk_oow_t maxlen, int ignorecase)
{
	if (maxlen == 0) return 0;

	if (ignorecase)
	{
		while (_to_lower_()(*str1) == _to_lower_()(*str2))
		{
			 if (*str1 == '\0' || maxlen == 1) return 0;
			 str1++; str2++; maxlen--;
		}

		return ((_chau_type_)_to_lower_()(*str1) > (_chau_type_)_to_lower_()(*str2))? 1: -1;
	}
	else
	{
		while (*str1 == *str2)
		{
			 if (*str1 == '\0' || maxlen == 1) return 0;
			 str1++; str2++; maxlen--;
		}

		return ((_chau_type_)*str1 > (_chau_type_)*str2)? 1: -1;
	}
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_chau_type_]])popdef([[_to_lower_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_comp_chars_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_chau_type_]], $3)pushdef([[_to_lower_]], $4)dnl
int _fn_name_ (const _char_type_* str1, hawk_oow_t len, const _char_type_* str2, int ignorecase)
{
	/* for "abc\0" of length 4 vs "abc", the fourth character
	 * of the first string is equal to the terminating null of
	 * the second string. the first string is still considered 
	 * bigger */
	if (ignorecase)
	{
		const _char_type_* end = str1 + len;
		_char_type_ c1;
		_char_type_ c2;
		while (str1 < end && *str2 != '\0') 
		{
			c1 = _to_lower_()(*str1);
			c2 = _to_lower_()(*str2);
			if (c1 != c2) return ((_chau_type_)c1 > (_chau_type_)c2)? 1: -1;
			str1++; str2++;
		}
		return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
	}
	else
	{
		const _char_type_* end = str1 + len;
		while (str1 < end && *str2 != '\0') 
		{
			if (*str1 != *str2) return ((_chau_type_)*str1 > (_chau_type_)*str2)? 1: -1;
			str1++; str2++;
		}
		return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
	}
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_chau_type_]])popdef([[_to_lower_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_concat_chars_to_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_count_str_]], $3)dnl
hawk_oow_t _fn_name_ (_char_type_* buf, hawk_oow_t bsz, const _char_type_* str, hawk_oow_t len)
{
	_char_type_* p, * p2;
	const _char_type_* end;
	hawk_oow_t blen;

	blen = _count_str_()(buf);
	if (blen >= bsz) return blen; /* something wrong */

	p = buf + blen;
	p2 = buf + bsz - 1;

	end = str + len;

	while (p < p2) 
	{
		if (str >= end) break;
		*p++ = *str++;
	}

	if (bsz > 0) *p = '\0';
	return p - buf;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_count_str_]])dnl
]])dnl

dnl ---------------------------------------------------------------------------
define([[fn_concat_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_count_str_]], $3)dnl
hawk_oow_t _fn_name_ (_char_type_* buf, hawk_oow_t bsz, const _char_type_* str)
{
	_char_type_* p, * p2;
	hawk_oow_t blen;

	blen = _count_str_()(buf);
	if (blen >= bsz) return blen; /* something wrong */

	p = buf + blen;
	p2 = buf + bsz - 1;

	while (p < p2) 
	{
		if (*str == '\0') break;
		*p++ = *str++;
	}

	if (bsz > 0) *p = '\0';
	return p - buf;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_count_str_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_copy_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
void _fn_name_ (_char_type_* dst, const _char_type_* src, hawk_oow_t len)
{
	/* take note of no forced null termination */
	hawk_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_copy_chars_to_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
hawk_oow_t _fn_name_ (_char_type_* dst, hawk_oow_t dlen, const _char_type_* src, hawk_oow_t slen)
{
	hawk_oow_t i;
	if (dlen <= 0) return 0;
	if (dlen <= slen) slen = dlen - 1;
	for (i = 0; i < slen; i++) dst[i] = src[i];
	dst[i] = '\0';
	return i;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_copy_chars_to_cstr_unlimited]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
hawk_oow_t _fn_name_ (_char_type_* dst, const _char_type_* src, hawk_oow_t len)
{
	hawk_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
	dst[i] = '\0';
	return i;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_copy_cstr_to_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
hawk_oow_t _fn_name_ (_char_type_* dst, hawk_oow_t len, const _char_type_* src)
{
	/* no null termination */
	_char_type_* p, * p2;

	p = dst; p2 = dst + len - 1;

	while (p < p2)
	{
		if (*src == '\0') break;
		*p++ = *src++;
	}

	return p - dst;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_copy_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
hawk_oow_t _fn_name_ (_char_type_* dst, hawk_oow_t len, const _char_type_* src)
{
	_char_type_* p, * p2;

	p = dst; p2 = dst + len - 1;

	while (p < p2)
	{
		if (*src == '\0') break;
		*p++ = *src++;
	}

	if (len > 0) *p = '\0';
	return p - dst;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_copy_cstr_unlimited]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
hawk_oow_t _fn_name_ (_char_type_* dst, const _char_type_* src)
{
	_char_type_* org = dst;
	while ((*dst++ = *src++) != '\0');
	return dst - org - 1;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_copy_fmt_cstrs_to_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
hawk_oow_t _fn_name_ (_char_type_* buf, hawk_oow_t bsz, const _char_type_* fmt, const _char_type_* str[])
{
	_char_type_* b = buf;
	_char_type_* end = buf + bsz - 1;
	const _char_type_* f = fmt;

	if (bsz <= 0) return 0;

	while (*f != '\0')
	{
		if (*f == '\\')
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it
			 * normally also. */
			if (f[1] != '\0') f++;
		}
		else if (*f == '$')
		{
			if (f[1] == '{' && (f[2] >= '0' && f[2] <= '9'))
			{
				const _char_type_* tmp;
				hawk_oow_t idx = 0;

				tmp = f;
				f += 2;

				do idx = idx * 10 + (*f++ - '0');
				while (*f >= '0' && *f <= '9');

				if (*f != '}')
				{
					f = tmp;
					goto normal;
				}

				f++;

				tmp = str[idx];
				while (*tmp != '\0')
				{
					if (b >= end) goto fini;
					*b++ = *tmp++;
				}
				continue;
			}
			else if (f[1] == '$') f++;
		}

	normal:
		if (b >= end) break;
		*b++ = *f++;
	}

fini:
	*b = '\0';
	return b - buf;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_copy_fmt_cses_to_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_cs_type_]], $3)dnl
hawk_oow_t _fn_name_ (_char_type_* buf, hawk_oow_t bsz, const _char_type_* fmt, const _cs_type_ str[])
{
	_char_type_* b = buf;
	_char_type_* end = buf + bsz - 1;
	const _char_type_* f = fmt;
 
	if (bsz <= 0) return 0;
 
	while (*f != '\0')
	{
		if (*f == '\\')
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it 
			 * normally also. */
			if (f[1] != '\0') f++;
		}
		else if (*f == '$')
		{
			if (f[1] == '{' && (f[2] >= '0' && f[2] <= '9'))
			{
				const _char_type_* tmp, * tmpend;
				hawk_oow_t idx = 0;
 
				tmp = f;
				f += 2;
 
				do idx = idx * 10 + (*f++ - '0');
				while (*f >= '0' && *f <= '9');
	
				if (*f != '}')
				{
					f = tmp;
					goto normal;
				}
 
				f++;
				
				tmp = str[idx].ptr;
				tmpend = tmp + str[idx].len;
 
				while (tmp < tmpend)
				{
					if (b >= end) goto fini;
					*b++ = *tmp++;
				}
				continue;
			}
			else if (f[1] == '$') f++;
		}
 
	normal:
		if (b >= end) break;
		*b++ = *f++;
	}
 
fini:
	*b = '\0';
	return b - buf;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_cs_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_count_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
hawk_oow_t _fn_name_ (const _char_type_* str)
{
	const _char_type_* ptr = str;
	while (*ptr != '\0') ptr++;
	return ptr - str;
} 
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_count_cstr_limited]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
hawk_oow_t _fn_name_ (const _char_type_* str, hawk_oow_t maxlen)
{
	hawk_oow_t i;
	for (i = 0; i < maxlen; i++)
	{
		if (str[i] == '\0') break;
	}
	return i;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_equal_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
int _fn_name_ (const _char_type_* str1, const _char_type_* str2, hawk_oow_t len)
{
	hawk_oow_t i;

	/* NOTE: you should call this function after having ensured that
	 *       str1 and str2 are in the same length */

	for (i = 0; i < len; i++)
	{
		if (str1[i] != str2[i]) return 0;
	}

	return 1;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_fill_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
void _fn_name_ (_char_type_* dst, _char_type_ ch, hawk_oow_t len)
{
        hawk_oow_t i;
        for (i = 0; i < len; i++) dst[i] = ch;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_find_char_in_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
_char_type_* _fn_name_ (const _char_type_* ptr, hawk_oow_t len, _char_type_ c)
{
	const _char_type_* end;

	end = ptr + len;
	while (ptr < end)
	{
		if (*ptr == c) return (_char_type_*)ptr;
		ptr++;
	}

	return HAWK_NULL;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_rfind_char_in_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
_char_type_* _fn_name_ (const _char_type_* ptr, hawk_oow_t len, _char_type_ c)
{
	const _char_type_* cur;

	cur = ptr + len;
	while (cur > ptr)
	{
		--cur;
		if (*cur == c) return (_char_type_*)cur;
	}

	return HAWK_NULL;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_find_char_in_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
_char_type_* _fn_name_ (const _char_type_* ptr, _char_type_ c)
{
	while (*ptr != '\0')
	{
		if (*ptr == c) return (_char_type_*)ptr;
		ptr++;
	}

	return HAWK_NULL;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_rfind_char_in_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
_char_type_* _fn_name_ (const _char_type_* str, _char_type_ c)
{
	const _char_type_* ptr = str;
	while (*ptr != '\0') ptr++;

	while (ptr > str)
	{
		--ptr;
		if (*ptr == c) return (_char_type_*)ptr;
	}

	return HAWK_NULL;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_find_chars_in_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_to_lower_]], $3)dnl
_char_type_* _fn_name_ (const _char_type_* str, hawk_oow_t strsz, const _char_type_* sub, hawk_oow_t subsz, int ignorecase)
{
	const _char_type_* end, * subp;

	if (subsz == 0) return (_char_type_*)str;
	if (strsz < subsz) return HAWK_NULL;
	
	end = str + strsz - subsz;
	subp = sub + subsz;

	if (HAWK_UNLIKELY(ignorecase))
	{
		while (str <= end) 
		{
			const _char_type_* x = str;
			const _char_type_* y = sub;

			while (1)
			{
				if (y >= subp) return (_char_type_*)str;
				if (_to_lower_()(*x) != _to_lower_()(*y)) break;
				x++; y++;
			}

			str++;
		}
	}
	else
	{
		while (str <= end) 
		{
			const _char_type_* x = str;
			const _char_type_* y = sub;

			while (1)
			{
				if (y >= subp) return (_char_type_*)str;
				if (*x != *y) break;
				x++; y++;
			}

			str++;
		}
	}

	return HAWK_NULL;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_to_lower_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_rfind_chars_in_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_to_lower_]], $3)dnl
_char_type_* _fn_name_ (const _char_type_* str, hawk_oow_t strsz, const _char_type_* sub, hawk_oow_t subsz, int ignorecase)
{
	const _char_type_* p = str + strsz;
	const _char_type_* subp = sub + subsz;

	if (subsz == 0) return (_char_type_*)p;
	if (strsz < subsz) return HAWK_NULL;

	p = p - subsz;

	if (HAWK_UNLIKELY(ignorecase))
	{
		while (p >= str) 
		{
			const _char_type_* x = p;
			const _char_type_* y = sub;

			while (1) 
			{
				if (y >= subp) return (_char_type_*)p;
				if (_to_lower_()(*x) != _to_lower_()(*y)) break;
				x++; y++;
			}

			p--;
		}
	}
	else
	{
		while (p >= str) 
		{
			const _char_type_* x = p;
			const _char_type_* y = sub;

			while (1) 
			{
				if (y >= subp) return (_char_type_*)p;
				if (*x != *y) break;
				x++; y++;
			}	

			p--;
		}
	}

	return HAWK_NULL;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_to_lower_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_compact_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_is_space_]], $3)dnl
hawk_oow_t _fn_name_ (_char_type_* str, hawk_oow_t len)
{
	_char_type_* p = str, * q = str, * end = str + len;
	int followed_by_space = 0;
	int state = 0;

	while (p < end) 
	{
		if (state == 0) 
		{
			if (!_is_space_()(*p)) 
			{
				*q++ = *p;
				state = 1;
			}
		}
		else if (state == 1) 
		{
			if (_is_space_()(*p)) 
			{
				if (!followed_by_space) 
				{
					followed_by_space = 1;
					*q++ = *p;
				}
			}
			else 
			{
				followed_by_space = 0;
				*q++ = *p;	
			}
		}

		p++;
	}

	return (followed_by_space) ? (q - str -1): (q - str);
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_is_space_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_rotate_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)dnl
hawk_oow_t _fn_name_ (_char_type_* str, hawk_oow_t len, int dir, hawk_oow_t n)
{
	hawk_oow_t first, last, count, index, nk;
	_char_type_ c;

	if (dir == 0 || len == 0) return len;
	if ((n %= len) == 0) return len;

	if (dir > 0) n = len - n;
	first = 0; nk = len - n; count = 0;

	while (count < n)
	{
		last = first + nk;
		index = first;
		c = str[first];
		do
		{
			count++;
			while (index < nk)
			{
				str[index] = str[index + n];
				index += n;
			}
			if (index == last) break;
			str[index] = str[index - nk];
			index -= nk;
		}
		while (1);
		str[last] = c; first++;
	}
	return len;
}
popdef([[_fn_name_]])popdef([[_char_type_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_trim_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_is_space_]], $3)pushdef([[_prefix_]], $4)dnl
_char_type_* _fn_name_ (const _char_type_* str, hawk_oow_t* len, int flags)
{
	const _char_type_* p = str, * end = str + *len;

	if (p < end)
	{
		const _char_type_* s = HAWK_NULL, * e = HAWK_NULL;

		do
		{
			if (!_is_space_()(*p))
			{
				if (s == HAWK_NULL) s = p;
				e = p;
			}
			p++;
		}
		while (p < end);

		if (e)
		{
			if (flags & _prefix_()_RIGHT) 
			{
				*len -= end - e - 1;
			}
			if (flags & _prefix_()_LEFT) 
			{
				*len -= s - str;
				str = s;
			}
		}
		else
		{
			/* the entire string need to be deleted */
			if ((flags & _prefix_()_RIGHT) || 
			    (flags & _prefix_()_LEFT)) *len = 0;
		}
	}

	return (_char_type_*)str;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_is_space_]])popdef([[_prefix_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_split_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_is_space_]], $3)pushdef([[_copy_str_unlimited_]], $4)dnl
int _fn_name_ (_char_type_* s, const _char_type_* delim, _char_type_ lquote, _char_type_ rquote, _char_type_ escape)
{
	_char_type_* p = s, *d;
	_char_type_* sp = HAWK_NULL, * ep = HAWK_NULL;
	int delim_mode;
	int cnt = 0;

	if (delim == HAWK_NULL) delim_mode = 0;
	else 
	{
		delim_mode = 1;
		for (d = (_char_type_*)delim; *d != '\0'; d++)
			if (!_is_space_()(*d)) delim_mode = 2;
	}

	if (delim_mode == 0) 
	{
		/* skip preceding space characters */
		while (_is_space_()(*p)) p++;

		/* when 0 is given as "delim", it has an effect of cutting
		   preceding and trailing space characters off "s". */
		if (lquote != '\0' && *p == lquote) 
		{
			_copy_str_unlimited_ (p, p + 1);

			for (;;) 
			{
				if (*p == '\0') return -1;

				if (escape != '\0' && *p == escape) 
				{
					_copy_str_unlimited_ (p, p + 1);
				}
				else 
				{
					if (*p == rquote) 
					{
						p++;
						break;
					}
				}

				if (sp == 0) sp = p;
				ep = p;
				p++;
			}
			while (_is_space_()(*p)) p++;
			if (*p != '\0') return -1;

			if (sp == 0 && ep == 0) s[0] = '\0';
			else 
			{
				ep[1] = '\0';
				if (s != (_char_type_*)sp) _copy_str_unlimited_ (s, sp);
				cnt++;
			}
		}
		else 
		{
			while (*p) 
			{
				if (!_is_space_()(*p)) 
				{
					if (sp == 0) sp = p;
					ep = p;
				}
				p++;
			}

			if (sp == 0 && ep == 0) s[0] = '\0';
			else 
			{
				ep[1] = '\0';
				if (s != (_char_type_*)sp) _copy_str_unlimited_ (s, sp);
				cnt++;
			}
		}
	}
	else if (delim_mode == 1) 
	{
		_char_type_* o;

		while (*p) 
		{
			o = p;
			while (_is_space_()(*p)) p++;
			if (o != p) { _copy_str_unlimited_ (o, p); p = o; }

			if (lquote != '\0' && *p == lquote) 
			{
				_copy_str_unlimited_ (p, p + 1);

				for (;;) 
				{
					if (*p == '\0') return -1;

					if (escape != '\0' && *p == escape) 
					{
						_copy_str_unlimited_ (p, p + 1);
					}
					else 
					{
						if (*p == rquote) 
						{
							*p++ = '\0';
							cnt++;
							break;
						}
					}
					p++;
				}
			}
			else 
			{
				o = p;
				for (;;) 
				{
					if (*p == '\0') 
					{
						if (o != p) cnt++;
						break;
					}
					if (_is_space_()(*p)) 
					{
						*p++ = '\0';
						cnt++;
						break;
					}
					p++;
				}
			}
		}
	}
	else /* if (delim_mode == 2) */
	{
		_char_type_* o;
		int ok;

		while (*p != '\0') 
		{
			o = p;
			while (_is_space_()(*p)) p++;
			if (o != p) { _copy_str_unlimited_ (o, p); p = o; }

			if (lquote != '\0' && *p == lquote) 
			{
				_copy_str_unlimited_ (p, p + 1);

				for (;;) 
				{
					if (*p == '\0') return -1;

					if (escape != '\0' && *p == escape) 
					{
						_copy_str_unlimited_ (p, p + 1);
					}
					else 
					{
						if (*p == rquote) 
						{
							*p++ = '\0';
							cnt++;
							break;
						}
					}
					p++;
				}

				ok = 0;
				while (_is_space_()(*p)) p++;
				if (*p == '\0') ok = 1;
				for (d = (_char_type_*)delim; *d != '\0'; d++) 
				{
					if (*p == *d) 
					{
						ok = 1;
						_copy_str_unlimited_ (p, p + 1);
						break;
					}
				}
				if (ok == 0) return -1;
			}
			else 
			{
				o = p; sp = ep = 0;

				for (;;) 
				{
					if (*p == '\0') 
					{
						if (ep) 
						{
							ep[1] = '\0';
							p = &ep[1];
						}
						cnt++;
						break;
					}
					for (d = (_char_type_*)delim; *d != '\0'; d++) 
					{
						if (*p == *d)  
						{
							if (sp == HAWK_NULL) 
							{
								_copy_str_unlimited_ (o, p); p = o;
								*p++ = '\0';
							}
							else 
							{
								_copy_str_unlimited_ (&ep[1], p);
								_copy_str_unlimited_ (o, sp);
								o[ep - sp + 1] = '\0';
								p = &o[ep - sp + 2];
							}
							cnt++;
							/* last empty field after delim */
							if (*p == '\0') cnt++;
							goto exit_point;
						}
					}

					if (!_is_space_()(*p)) 
					{
						if (sp == HAWK_NULL) sp = p;
						ep = p;
					}
					p++;
				}
exit_point:
				;
			}
		}
	}

	return cnt;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_is_space_]])popdef([[_copy_str_unlimited_]])dnl
]])dnl
define([[fn_tokenize_chars]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_cs_type_]], $3)pushdef([[_is_space_]], $4)pushdef([[_to_lower_]], $5)dnl
_char_type_* _fn_name_ (const _char_type_* s, hawk_oow_t len, const _char_type_* delim, hawk_oow_t delim_len, _cs_type_* tok, int ignorecase)
{
	const _char_type_* p = s, *d;
	const _char_type_* end = s + len;
	const _char_type_* sp = HAWK_NULL, * ep = HAWK_NULL;
	const _char_type_* delim_end = delim + delim_len;
	_char_type_ c; 
	int delim_mode;

#define __DELIM_NULL      0
#define __DELIM_EMPTY     1
#define __DELIM_SPACES    2
#define __DELIM_NOSPACES  3
#define __DELIM_COMPOSITE 4
	if (delim == HAWK_NULL) delim_mode = __DELIM_NULL;
	else 
	{
		delim_mode = __DELIM_EMPTY;

		for (d = delim; d < delim_end; d++) 
		{
			if (_is_space_()(*d)) 
			{
				if (delim_mode == __DELIM_EMPTY)
					delim_mode = __DELIM_SPACES;
				else if (delim_mode == __DELIM_NOSPACES)
				{
					delim_mode = __DELIM_COMPOSITE;
					break;
				}
			}
			else
			{
				if (delim_mode == __DELIM_EMPTY)
					delim_mode = __DELIM_NOSPACES;
				else if (delim_mode == __DELIM_SPACES)
				{
					delim_mode = __DELIM_COMPOSITE;
					break;
				}
			}
		}

		/* TODO: verify the following statement... */
		if (delim_mode == __DELIM_SPACES && delim_len == 1 && delim[0] != ' ') delim_mode = __DELIM_NOSPACES;
	}		
	
	if (delim_mode == __DELIM_NULL) 
	{ 
		/* when HAWK_NULL is given as "delim", it trims off the 
		 * leading and trailing spaces characters off the source
		 * string "s" eventually. */

		while (p < end && _is_space_()(*p)) p++;
		while (p < end) 
		{
			c = *p;

			if (!_is_space_()(c)) 
			{
				if (sp == HAWK_NULL) sp = p;
				ep = p;
			}
			p++;
		}
	}
	else if (delim_mode == __DELIM_EMPTY)
	{
		/* each character in the source string "s" becomes a token. */
		if (p < end)
		{
			c = *p;
			sp = p;
			ep = p++;
		}
	}
	else if (delim_mode == __DELIM_SPACES) 
	{
		/* each token is delimited by space characters. all leading
		 * and trailing spaces are removed. */

		while (p < end && _is_space_()(*p)) p++;
		while (p < end) 
		{
			c = *p;
			if (_is_space_()(c)) break;
			if (sp == HAWK_NULL) sp = p;
			ep = p++;
		}
		while (p < end && _is_space_()(*p)) p++;
	}
	else if (delim_mode == __DELIM_NOSPACES)
	{
		/* each token is delimited by one of charaters 
		 * in the delimeter set "delim". */
		if (ignorecase)
		{
			while (p < end) 
			{
				c = _to_lower_()(*p);
				for (d = delim; d < delim_end; d++) 
				{
					if (c == _to_lower_()(*d)) goto exit_loop;
				}

				if (sp == HAWK_NULL) sp = p;
				ep = p++;
			}
		}
		else
		{
			while (p < end) 
			{
				c = *p;
				for (d = delim; d < delim_end; d++) 
				{
					if (c == *d) goto exit_loop;
				}

				if (sp == HAWK_NULL) sp = p;
				ep = p++;
			}
		}
	}
	else /* if (delim_mode == __DELIM_COMPOSITE) */ 
	{
		/* each token is delimited by one of non-space charaters
		 * in the delimeter set "delim". however, all space characters
		 * surrounding the token are removed */
		while (p < end && _is_space_()(*p)) p++;
		if (ignorecase)
		{
			while (p < end) 
			{
				c = _to_lower_()(*p);
				if (_is_space_()(c)) 
				{
					p++;
					continue;
				}
				for (d = delim; d < delim_end; d++) 
				{
					if (c == _to_lower_()(*d)) goto exit_loop;
				}
				if (sp == HAWK_NULL) sp = p;
				ep = p++;
			}
		}
		else
		{
			while (p < end) 
			{
				c = *p;
				if (_is_space_()(c)) 
				{
					p++;
					continue;
				}
				for (d = delim; d < delim_end; d++) 
				{
					if (c == *d) goto exit_loop;
				}
				if (sp == HAWK_NULL) sp = p;
				ep = p++;
			}
		}
	}

exit_loop:
	if (sp == HAWK_NULL) 
	{
		tok->ptr = HAWK_NULL;
		tok->len = (hawk_oow_t)0;
	}
	else 
	{
		tok->ptr = (_char_type_*)sp;
		tok->len = ep - sp + 1;
	}

	/* if HAWK_NULL is returned, this function should not be called again */
	if (p >= end) return HAWK_NULL;
	if (delim_mode == __DELIM_EMPTY || delim_mode == __DELIM_SPACES) return (_char_type_*)p;
	return (_char_type_*)++p;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_cs_type_]])popdef([[_is_space_]])popdef([[_to_lower_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_byte_to_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_prefix_]], $3)dnl
hawk_oow_t _fn_name_ (hawk_uint8_t byte, _char_type_* buf, hawk_oow_t size, int flagged_radix, _char_type_ fill)
{
	_char_type_ tmp[(HAWK_SIZEOF(hawk_uint8_t) * HAWK_BITS_PER_BYTE)];
	_char_type_* p = tmp, * bp = buf, * be = buf + size - 1;
	int radix;
	_char_type_ radix_char;

	radix = (flagged_radix & _prefix_()_RADIXMASK);
	radix_char = (flagged_radix & _prefix_()_LOWERCASE)? 'a': 'A';
	if (radix < 2 || radix > 36 || size <= 0) return 0;

	do 
	{
		hawk_uint8_t digit = byte % radix;
		if (digit < 10) *p++ = digit + '0';
		else *p++ = digit + radix_char - 10;
		byte /= radix;
	}
	while (byte > 0);

	if (fill != '\0') 
	{
		while (size - 1 > p - tmp) 
		{
			*bp++ = fill;
			size--;
		}
	}

	while (p > tmp && bp < be) *bp++ = *--p;
	*bp = '\0';
	return bp - buf;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_cs_type_]])popdef([[_prefix_]])dnl
dnl ---------------------------------------------------------------------------
]])dnl
define([[fn_int_to_cstr]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_int_type_]], $3)pushdef([[_count_cstr_]], $4)dnl
hawk_oow_t _fn_name_ (_int_type_ value, int radix, const _char_type_* prefix, _char_type_* buf, hawk_oow_t size)
{
	_int_type_ t, rem;
	hawk_oow_t len, ret, i;
	hawk_oow_t prefix_len;

	prefix_len = (prefix != HAWK_NULL)? _count_cstr_()(prefix): 0;

	t = value;
	if (t == 0)
	{
		/* zero */
		if (buf == HAWK_NULL) 
		{
			/* if buf is not given, 
			 * return the number of bytes required */
			return prefix_len + 1;
		}

		if (size < prefix_len+1) 
		{
			/* buffer too small */
			return (hawk_oow_t)-1;
		}

		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
		buf[prefix_len] = '0';
		if (size > prefix_len+1) buf[prefix_len+1] = '\0';
		return prefix_len+1;
	}

	/* non-zero values */
	len = prefix_len;
	if (t < 0) { t = -t; len++; }
	while (t > 0) { len++; t /= radix; }

	if (buf == HAWK_NULL)
	{
		/* if buf is not given, return the number of bytes required */
		return len;
	}

	if (size < len) return (hawk_oow_t)-1; /* buffer too small */
	if (size > len) buf[len] = '\0';
	ret = len;

	t = value;
	if (t < 0) t = -t;

	while (t > 0) 
	{
		rem = t % radix;
		if (rem >= 10)
			buf[--len] = (_char_type_)rem + 'a' - 10;
		else
			buf[--len] = (_char_type_)rem + '0';
		t /= radix;
	}

	if (value < 0) 
	{
		for (i = 1; i <= prefix_len; i++) 
		{
			buf[i] = prefix[i-1];
			len--;
		}
		buf[--len] = '-';
	}
	else
	{
		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
	}

	return ret;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_int_type_]])popdef([[_count_cstr_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_chars_to_int]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_int_type_]], $3)pushdef([[_is_space_]], $4)pushdef([[_prefix_]], $5)dnl
_int_type_ _fn_name_ (const _char_type_* str, hawk_oow_t len, int option, const _char_type_** endptr, int* is_sober)
{
	_int_type_ n = 0;
	const _char_type_* p, * pp;
	const _char_type_* end;
	hawk_oow_t rem;
	int digit, negative = 0;
	int base = _prefix_()_GET_OPTION_BASE(option);

	p = str; 
	end = str + len;

	if (_prefix_()_GET_OPTION_LTRIM(option))
	{
		/* strip off leading spaces */
		while (p < end && _is_space_()(*p)) p++;
	}

	/* check for a sign */
	while (p < end)
	{
		if (*p == '-') 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == '+') p++;
		else break;
	}

	/* check for a binary/octal/hexadecimal notation */
	rem = end - p;
	if (base == 0) 
	{
		if (rem >= 1 && *p == '0') 
		{
			p++;

			if (rem == 1) base = 8;
			else if (*p == 'x' || *p == 'X')
			{
				p++; base = 16;
			} 
			else if (*p == 'b' || *p == 'B')
			{
				p++; base = 2;
			}
			else base = 8;
		}
		else base = 10;
	} 
	else if (rem >= 2 && base == 16)
	{
		if (*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) p += 2; 
	}
	else if (rem >= 2 && base == 2)
	{
		if (*p == '0' && (*(p + 1) == 'b' || *(p + 1) == 'B')) p += 2; 
	}

	/* process the digits */
	pp = p;
	while (p < end)
	{
		digit = HAWK_ZDIGIT_TO_NUM(*p, base);
		if (digit >= base) break;
		n = n * base + digit;
		p++;
	}

	if (_prefix_()_GET_OPTION_E(option))
	{
		if (*p == 'E' || *p == 'e')
		{
			_int_type_ e = 0, i;
			int e_neg = 0;
			p++;
			if (*p == '+')
			{
				p++;
			}
			else if (*p == '-')
			{
				p++; e_neg = 1;
			}
			while (p < end)
			{
				digit = HAWK_ZDIGIT_TO_NUM(*p, base);
				if (digit >= base) break;
				e = e * base + digit;
				p++;
			}
			if (e_neg)
				for (i = 0; i < e; i++) n /= 10;
			else
				for (i = 0; i < e; i++) n *= 10;
		}
	}

	/* base 8: at least a zero digit has been seen.
	 * other case: p > pp to be able to have at least 1 meaningful digit. */
	if (is_sober) *is_sober = (base == 8 || p > pp); 

	if (_prefix_()_GET_OPTION_RTRIM(option))
	{
		/* consume trailing spaces */
		while (p < end && _is_space_()(*p)) p++;
	}

	if (endptr) *endptr = p;
	return (negative)? -n: n;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_int_type_]])popdef([[_is_space_]])popdef([[_prefix_]])dnl
]])dnl
dnl ---------------------------------------------------------------------------
define([[fn_chars_to_uint]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_int_type_]], $3)pushdef([[_is_space_]], $4)pushdef([[_prefix_]], $5)dnl
_int_type_ _fn_name_ (const _char_type_* str, hawk_oow_t len, int option, const _char_type_** endptr, int* is_sober)
{
	_int_type_ n = 0;
	const _char_type_* p, * pp;
	const _char_type_* end;
	hawk_oow_t rem;
	int digit;
	int base = _prefix_()_GET_OPTION_BASE(option);

	p = str; 
	end = str + len;

	if (_prefix_()_GET_OPTION_LTRIM(option))
	{
		/* strip off leading spaces */
		while (p < end && _is_space_()(*p)) p++;
	}

	/* check for a sign */
	while (p < end)
	{
		if (*p == '+') p++;
		else break;
	}

	/* check for a binary/octal/hexadecimal notation */
	rem = end - p;
	if (base == 0) 
	{
		if (rem >= 1 && *p == '0') 
		{
			p++;

			if (rem == 1) base = 8;
			else if (*p == 'x' || *p == 'X')
			{
				p++; base = 16;
			} 
			else if (*p == 'b' || *p == 'B')
			{
				p++; base = 2;
			}
			else base = 8;
		}
		else base = 10;
	} 
	else if (rem >= 2 && base == 16)
	{
		if (*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) p += 2; 
	}
	else if (rem >= 2 && base == 2)
	{
		if (*p == '0' && (*(p + 1) == 'b' || *(p + 1) == 'B')) p += 2; 
	}

	/* process the digits */
	pp = p;
	while (p < end)
	{
		digit = HAWK_ZDIGIT_TO_NUM(*p, base);
		if (digit >= base) break;
		n = n * base + digit;
		p++;
	}

	if (_prefix_()_GET_OPTION_E(option))
	{
		if (*p == 'E' || *p == 'e')
		{
			_int_type_ e = 0, i;
			int e_neg = 0;
			p++;
			if (*p == '+')
			{
				p++;
			}
			else if (*p == '-')
			{
				p++; e_neg = 1;
			}
			while (p < end)
			{
				digit = HAWK_ZDIGIT_TO_NUM(*p, base);
				if (digit >= base) break;
				e = e * base + digit;
				p++;
			}
			if (e_neg)
				for (i = 0; i < e; i++) n /= 10;
			else
				for (i = 0; i < e; i++) n *= 10;
		}
	}

	/* base 8: at least a zero digit has been seen.
	 * other case: p > pp to be able to have at least 1 meaningful digit. */
	if (is_sober) *is_sober = (base == 8 || p > pp); 

	if (_prefix_()_GET_OPTION_RTRIM(option))
	{
		/* consume trailing spaces */
		while (p < end && _is_space_()(*p)) p++;
	}

	if (endptr) *endptr = p;
	return n;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_int_type_]])popdef([[_is_space_]])popdef([[_prefix_]])dnl
]])
dnl ---------------------------------------------------------------------------
define([[fn_fnmat]], [[pushdef([[_fn_name_]], $1)pushdef([[_char_type_]], $2)pushdef([[_to_lower_]], $3)pushdef([[_match_ch_class_]], $4)dnl
int _fn_name_ (const _char_type_* str, hawk_oow_t slen, const _char_type_* ptn, hawk_oow_t plen, int flags, int no_first_period)
{
	const _char_type_* sp = str;
	const _char_type_* pp = ptn;
	const _char_type_* se = str + slen;
	const _char_type_* pe = ptn + plen;
	_char_type_ sc, pc, pc2;

	while (1) 
	{
		if (pp < pe && HAWK_FNMAT_IS_ESC(*pp) && !(flags & HAWK_FNMAT_NOESCAPE)) 
		{
			/* pattern is escaped and escaping is allowed. */

			if ((++pp) >= pe) 
			{
				/* 
				 * the last character of the pattern is an WCS_ESC. 
				 * matching is performed as if the end of the pattern is
				 * reached just without an WCS_ESC.
				 */
				if (sp < se) return 0;
				return 1;
			}

			if (sp >= se) return 0; /* premature string termination */

			sc = *sp; pc = *pp; /* pc is just a normal character */
			if ((flags & HAWK_FNMAT_IGNORECASE) != 0) 
			{
				/* make characters to lower-case */
				sc = _to_lower_()(sc);
				pc = _to_lower_()(pc); 
			}

			if (sc != pc) return 0;
			sp++; pp++; 
			continue;
		}
		if (pp >= pe) 
		{
			/* 
			 * the end of the pattern has been reached. 
			 * the string must terminate too.
			 */
			return sp >= se;
		}

		if (sp >= se) 
		{
			/* the string terminats prematurely */
			while (pp < pe && *pp == '*') pp++;
			return pp >= pe;
		}

		sc = *sp; pc = *pp;

		if (sc == '.' && (flags & HAWK_FNMAT_PERIOD)) 
		{
			/* 
			 * a leading period in the staring must match 
			 * a period in the pattern explicitly 
			 */
			if ((!no_first_period && sp == str) || 
			    (HAWK_FNMAT_IS_SEP(sp[-1]) && (flags & HAWK_FNMAT_PATHNAME))) 
			{
				if (pc != '.') return 0;
				sp++; pp++;
				continue;
			}
		}
		else if (HAWK_FNMAT_IS_SEP(sc) && (flags & HAWK_FNMAT_PATHNAME)) 
		{
			while (pc == '*') 
			{
				if ((++pp) >= pe) return 0;
				pc = *pp;
			}

			/* a path separator must be matched explicitly */
			if (!HAWK_FNMAT_IS_SEP(pc)) return 0;
			sp++; pp++;
			continue;
		}

		/* the handling of special pattern characters begins here */
		if (pc == '?') 
		{
			/* match any single character */
			sp++; pp++; 
		} 
		else if (pc == '*') 
		{ 
			/* match zero or more characters */

			/* compact asterisks */
			do { pp++; } while (pp < pe && *pp == '*');

			if (pp >= pe) 
			{
				/* 
				 * if the last character in the pattern is an asterisk,
				 * the string should not have any directory separators
				 * when HAWK_FNMAT_PATHNAME is set.
				 */
				if (flags & HAWK_FNMAT_PATHNAME)
				{
					const _char_type_* s = sp;
					for (s = sp; s < se; s++)
					{
						if (HAWK_FNMAT_IS_SEP(*s)) return 0;
					}
				}
				return 1;
			}
			else 
			{
				do 
				{
					if (_fn_name_()(sp, se - sp, pp, pe - pp, flags, 1)) return 1;
					if (HAWK_FNMAT_IS_SEP(*sp) && (flags & HAWK_FNMAT_PATHNAME)) break;
					sp++;
				} 
				while (sp < se);

				return 0;
			}
		}
		else if (pc == '[') 
		{
			/* match range */
			int negate = 0;
			int matched = 0;

			if ((++pp) >= pe) return 0;
			if (*pp == '!') { negate = 1; pp++; } 

			while (pp < pe && *pp != ']') 
			{
				if (*pp == '[') 
				{
					hawk_oow_t pl = pe - pp;

					if (pl >= 9)  /* assumption that [:class:] is at least 9 in _match_ch_class_ */
					{
						int x = _match_ch_class_()(pp, sc, &matched);
						if (x > 0)
						{
							pp += x;
							continue;
						}
					}

					/* 
					 * characters in an invalid class name are 
					 * just treated as normal characters 
					 */
				}

				if (HAWK_FNMAT_IS_ESC(*pp) && !(flags & HAWK_FNMAT_NOESCAPE)) pp++;
				else if (*pp == ']') break;

				if (pp >= pe) break;

				pc = *pp;
				if ((flags & HAWK_FNMAT_IGNORECASE) != 0) 
				{
					sc = _to_lower_()(sc); 
					pc = _to_lower_()(pc); 
				}

				if (pp + 1 < pe && pp[1] == '-') 
				{
					pp += 2; /* move the a character next to a dash */

					if (pp >= pe) 
					{
						if (sc >= pc) matched = 1;
						break;
					}

					if (HAWK_FNMAT_IS_ESC(*pp) && !(flags & HAWK_FNMAT_NOESCAPE)) 
					{
						if ((++pp) >= pe) 
						{
							if (sc >= pc) matched = 1;
							break;
						}
					}
					else if (*pp == ']') 
					{
						if (sc >= pc) matched = 1;
						break;
					}

					pc2 = *pp;
					if ((flags & HAWK_FNMAT_IGNORECASE) != 0) 
						pc2 = _to_lower_()(pc2); 

					if (sc >= pc && sc <= pc2) matched = 1;
					pp++;
				}
				else 
				{
					if (sc == pc) matched = 1;
					pp++;
				}
			}

			if (negate) matched = !matched;
			if (!matched) return 0;
			sp++; if (pp < pe) pp++;
		}
		else 
		{
			/* a normal character */
			if ((flags & HAWK_FNMAT_IGNORECASE) != 0) 
			{
				sc = _to_lower_()(sc); 
				pc = _to_lower_()(pc); 
			}

			if (sc != pc) return 0;
			sp++; pp++;
		}
	}

	/* will never reach here. but make some immature compilers happy... */
	return 0;
}
popdef([[_fn_name_]])popdef([[_char_type_]])popdef([[_int_type_]])popdef([[_match_ch_class_]])dnl
]])
