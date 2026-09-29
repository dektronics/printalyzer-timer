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

/**
 * This is a simple and pure-C library to check and parse version strings conforming
 * to the Semantic Versioning 2.0.0 standard. Of course there are many ways of doing
 * this, but this piece of code has some different main goals - it is targeting embedded
 * environments. It has very low stack and memory requirements and the code size and
 * complexity is low to be usable even on 8bit microcontrollers.
 */

#pragma once

#include <stdbool.h>


struct semver_context {
	const char *version_str;
	const char *pos;
	const char *saved_pos;

	uint32_t major;
	uint32_t minor;
	uint32_t patch;

	bool is_prerelease;
	const char *prerelease;
	size_t prerelease_size;

	bool has_metadata;
	const char *metadata;
	size_t metadata_size;
};


/**
 * @brief Initialize the parser context with a version string
 *
 * This function must be called before actual parsing of the version string. It initializes
 * the parsing contexts and sets the string to be parsed.
 *
 * @param ctx The parsing context
 * @param semver Null terminated string describing a version
 *
 * @return SEMVER_INIT_OK on success or
 *         SEMVER_INIT_FAILED otherwise.
 */
int32_t semver_init(struct semver_context *ctx, const char *semver);
#define SEMVER_INIT_OK 0
#define SEMVER_INIT_FAILED -1

/**
 * @brief Release the parsing context
 *
 * @param ctx The parsing context
 *
 * @return SEMVER_FREE_OK on success or
 *         SEMVER_FREE_FAILED otherwise.
 */
int32_t semver_free(struct semver_context *ctx);
#define SEMVER_FREE_OK 0
#define SEMVER_FREE_FAILED -1

/**
 * @brief Parse the version string
 *
 * This function parses the version string which was set in the @p semver_init. It traverses
 * all version fields and checks if they conform to the Semantic Versioning standard. If the
 * parsing was successful, some important parts of the version are available in the parsing
 * context structure (version major, minor, patch). The parse function can be called multiple
 * times on the same parsing context.
 *
 * @param ctx The parsing context
 *
 * @return SEMVER_PARSE_OK if the version string is OK and the parsing was successful,
 *         SEMVER_PARSE_EMPTY if the given version string was empty,
 *         SEMVER_PARSE_VERSION_MISSING if one of the basic fields (major, minor, patch) is missing,
 *         SEMVER_PARSE_BAD_PRERELEASE if the prerelease identifiers have bad format,
 *         SEMVER_PARSE_BAD_METADATA if the build metadata identifiers have bad format,
 *         SEMVER_PARSE_GARBAGE_REMAINS if there are non-recognized characters at the end.
 */
int32_t semver_parse(struct semver_context *ctx);
#define SEMVER_PARSE_OK 0
#define SEMVER_PARSE_EMPTY -1
#define SEMVER_PARSE_VERSION_MISSING -2
#define SEMVER_PARSE_BAD_PRERELEASE -3
#define SEMVER_PARSE_BAD_METADATA -4
#define SEMVER_PARSE_GARBAGE_REMAINS -5


