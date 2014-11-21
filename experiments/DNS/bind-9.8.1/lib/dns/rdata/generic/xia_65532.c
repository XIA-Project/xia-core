/*
 * Copyright (C) 2004, 2007-2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: txt_16.c,v 1.47 2009-12-04 22:06:37 tbox Exp $ */

/* Reviewed: Thu Mar 16 15:40:00 PST 2000 by bwelling */

#ifndef RDATA_GENERIC_XIA_65532_C
#define RDATA_GENERIC_XIA_65532_C

#define RRTYPE_XIA_ATTRIBUTES (0)
#include <unistd.h>

static inline isc_result_t
fromtext_xia(ARGS_FROMTEXT) {
	isc_token_t token;
	isc_region_t region;
	int cnt = 0, toklen;
int tmp;

	REQUIRE(type == 65532);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(origin);
	UNUSED(options);
	UNUSED(callbacks);
	while (1)
	{
		RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_qstring,
					      ISC_TRUE));
		if (token.type != isc_tokentype_string &&
		    token.type != isc_tokentype_qstring)
		{
			break;
		}
		isc_buffer_availableregion(target, &region);
		toklen = strlen(token.value.as_pointer);
		if (region.length < toklen+(cnt?1:0))
		{
			return (ISC_R_NOSPACE);
		}
		memset(region.base, ' ', (cnt?1:0));
		isc_buffer_add(target, (cnt?1:0));
		memcpy(region.base+(cnt?1:0), token.value.as_pointer, toklen);
		isc_buffer_add(target, toklen);
		
		cnt++;
	}
	if (!cnt)
	{
		return (ISC_R_UNEXPECTEDEND);
	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
totext_xia(ARGS_TOTEXT) {
	isc_region_t sregion;
	isc_region_t tregion;

	REQUIRE(rdata->type == 65532);
	UNUSED(tctx);

	isc_buffer_availableregion(target, &tregion);
	dns_rdata_toregion(rdata, &sregion);
	if (sregion.length > tregion.length)
	{
		return (ISC_R_NOSPACE);
	}

	memcpy(tregion.base, sregion.base, sregion.length);
	isc_buffer_add(target, sregion.length);
	
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_xia(ARGS_FROMWIRE) {
	isc_region_t sregion;
	isc_region_t tregion;
	
	REQUIRE(type == 65532);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sregion);
	isc_buffer_availableregion(target, &tregion);

	if (tregion.length < sregion.length)
	{
		return (ISC_R_NOSPACE);
	}

	memcpy(tregion.base, sregion.base, sregion.length);
	isc_buffer_forward(source, sregion.length);
	isc_buffer_add(target, sregion.length);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_xia(ARGS_TOWIRE) {
	isc_region_t region;

	REQUIRE(rdata->type == 65532);
	UNUSED(cctx);

	isc_buffer_availableregion(target, &region);
	if (region.length < rdata->length)
	{
		return (ISC_R_NOSPACE);
	}

	memcpy(region.base, rdata->data, rdata->length);
	isc_buffer_add(target, rdata->length);

	return (ISC_R_SUCCESS);
}

static inline int
compare_xia(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == 65532);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);

	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_xia(ARGS_FROMSTRUCT) {
	dns_rdata_xia_t *xia = source;

	REQUIRE(type == 65532);
	REQUIRE(source != NULL);
	REQUIRE(xia->common.rdtype == type);
	REQUIRE(xia->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
tostruct_xia(ARGS_TOSTRUCT) {
	REQUIRE(rdata->type == 65532);
	REQUIRE(target != NULL);

	return (ISC_R_SUCCESS);
}

static inline void
freestruct_xia(ARGS_FREESTRUCT) {
}

static inline isc_result_t
additionaldata_xia(ARGS_ADDLDATA) {
	REQUIRE(rdata->type == 65532);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_xia(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == 65532);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_xia(ARGS_CHECKOWNER) {

	REQUIRE(type == 65532);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_xia(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == 65532);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline isc_result_t
casecompare_xia(ARGS_COMPARE) {
	return (compare_xia(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_XIA_65532_C */
