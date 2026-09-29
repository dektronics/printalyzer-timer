/*
 * Copyright (c) 2016, Marek Koza (qyx@krtko.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "c_semver.h"


static bool semver_expect_continue(struct semver_context *ctx) {
	/* The version string continues if the current character is not 0. */
	return *ctx->pos != '\0';
}


static bool semver_expect_char(struct semver_context *ctx, char expected) {
	if (*ctx->pos == expected) {
		ctx->pos++;
		return true;
	} else {
		return false;
	}
}


static bool semver_parse_num_id(struct semver_context *ctx, uint32_t *num) {
	*num = 0;

	/* If the component ends without any content */
	if (*ctx->pos < '0' || *ctx->pos > '9') {
		return false;
	}

	/* Save the leading zero */
	bool leading_zero = *ctx->pos == '0';

	while (*ctx->pos >= '0' && *ctx->pos <= '9') {
		*num = *num * 10 + (uint32_t)(*ctx->pos - '0');
		ctx->pos++;
	}

	/* If the number is non-zero, there cannot be any leading zero. */
	if (leading_zero && *num > 0) {
		return false;
	}

	return true;
}


static bool semver_parse_alnum_id(struct semver_context *ctx) {
	uint32_t parsed_chars = 0;

	bool leading_zero = *ctx->pos == '0';

	while ((*ctx->pos >= '0' && *ctx->pos <= '9') ||
	       (*ctx->pos >= 'a' && *ctx->pos <= 'z') ||
	       (*ctx->pos >= 'A' && *ctx->pos <= 'Z') ||
	       (*ctx->pos == '-')) {
		/* Just count the characters, do nothing more now. */
		ctx->pos++;
		parsed_chars++;
	}

	if (leading_zero && parsed_chars > 1) {
		return false;
	}

	return true;
}


static void semver_save_pos(struct semver_context *ctx) {
	ctx->saved_pos = ctx->pos;
}


static void semver_restore_pos(struct semver_context *ctx) {
	if (ctx->saved_pos != NULL) {
		ctx->pos = ctx->saved_pos;
	}
}


int32_t semver_init(struct semver_context *ctx, const char *semver) {
	memset(ctx, 0, sizeof(struct semver_context));

	ctx->major = 0;
	ctx->minor = 0;
	ctx->patch = 0;

	ctx->is_prerelease = false;
	ctx->has_metadata = false;

	ctx->version_str = semver;
	ctx->pos = semver;

	return SEMVER_INIT_OK;
}


int32_t semver_free(struct semver_context *ctx) {
	(void)ctx;
	return SEMVER_FREE_OK;
}


int32_t semver_parse(struct semver_context *ctx) {
	/* Start from the beginning. */
	ctx->pos = ctx->version_str;

	/* Empty string is not a valid version. */
	if (semver_expect_continue(ctx) == false) {
		return SEMVER_PARSE_EMPTY;
	}

	/* Parse the major, minor and patch version numbers. */
	if ((semver_parse_num_id(ctx, &ctx->major) &&
	     semver_expect_char(ctx, '.') &&
	     semver_parse_num_id(ctx, &ctx->minor) &&
	     semver_expect_char(ctx, '.') &&
	     semver_parse_num_id(ctx, &ctx->patch)) == false) {
		return SEMVER_PARSE_VERSION_MISSING;
	}

	/* Check if there is a prelease field and parse it. The prerelease field starts
	 * with a hyphen and contains multiple numeric or alphanumeric identifiers separated
	 * by a dot. */
	if (semver_expect_char(ctx, '-')) {
		ctx->is_prerelease = true;
		ctx->prerelease = ctx->pos;

		do {
			uint32_t num;

			/* Save the current parsing position and try to parse the identifier as a number. */
			semver_save_pos(ctx);
			if (semver_parse_num_id(ctx, &num) == false || semver_expect_char(ctx, '-')) {
				/* If the identifier is not a number OR it was parsed as a number but the
				 * next character is a hyphen (so the following characters are part of the
				 * same identifier too), restore the saved position and try to parse it as an
				 * alphanumeric identifier. */
				semver_restore_pos(ctx);
				if (semver_parse_alnum_id(ctx) == false) {
					/* If that failed too, end the parsing. */
					return SEMVER_PARSE_BAD_PRERELEASE;
				}
			}
		/* And do that until there are no more dots (no identifiers are following). */
		} while (semver_expect_char(ctx, '.'));

		ctx->prerelease_size = (uint32_t)(ctx->pos - ctx->prerelease);
	}

	/* Check the build metadata field. It starts after a plus character. */
	if (semver_expect_char(ctx, '+')) {
		ctx->has_metadata = true;
		ctx->metadata = ctx->pos;

		do {
			uint32_t num;

			semver_save_pos(ctx);
			if (semver_parse_num_id(ctx, &num) == false || semver_expect_char(ctx, '-')) {
				semver_restore_pos(ctx);
				if (semver_parse_alnum_id(ctx) == false) {
					return SEMVER_PARSE_BAD_METADATA;
				}
			}
		} while (semver_expect_char(ctx, '.'));

		ctx->metadata_size = (uint32_t)(ctx->pos - ctx->metadata);
	}

	/* Check if the end was reached. If not, there is probably some garbage. */
	if (semver_expect_continue(ctx)) {
		return SEMVER_PARSE_GARBAGE_REMAINS;
	}

	return SEMVER_PARSE_OK;
}

